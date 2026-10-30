#include "pipeline_precompile.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// The renderer's hash lives in the SDK's shader translation; recipes only need a stable one here.
namespace gpu {
uint64_t Hash(const void* p, size_t n, uint64_t h) {
    for (size_t i = 0; i < n; i++) h = (h ^ static_cast<const uint8_t*>(p)[i]) * 1099511628211ull;
    return h;
}
}

namespace pp = openshao::pipelines;
using namespace std::chrono_literals;

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct FakeBuilder final : pp::Builder {
    std::mutex mutex;
    std::vector<uint32_t> built;  // first key byte of each built recipe
    std::vector<std::vector<uint32_t>> programs;
    std::chrono::milliseconds delay{0};
    std::condition_variable released;
    bool hold = false;
    std::atomic<unsigned> entered{0};
    uint8_t fail_id = 0;
    ~FakeBuilder() { Release(); pp::Shutdown(); }
    bool BuildPipeline(const pp::Recipe& r, std::span<const uint32_t> program) override {
        entered++;
        {
            std::unique_lock lock(mutex);
            released.wait(lock, [&] { return !hold; });
        }
        std::this_thread::sleep_for(delay);
        std::lock_guard lock(mutex);
        built.push_back(r.key[0]);
        programs.emplace_back(program.begin(), program.end());
        return r.key[0] != fail_id;
    }
    void Release() {
        { std::lock_guard lock(mutex); hold = false; }
        released.notify_all();
    }
    size_t Count() {
        std::lock_guard lock(mutex);
        return built.size();
    }
    bool WaitFor(size_t n) {
        for (int i = 0; i < 500; i++) {
            if (Count() >= n) return true;
            std::this_thread::sleep_for(10ms);
        }
        return false;
    }
    bool Built(uint8_t id) {
        std::lock_guard lock(mutex);
        for (uint32_t b : built) if (b == id) return true;
        return false;
    }
};

pp::Recipe MakeRecipe(uint8_t id, uint64_t program = 0) {
    pp::Recipe r{};
    r.vertex.program = program;
    r.vertex.regs = 0x13;
    r.pixel.count = id;
    r.key_size = 16;
    r.key[0] = id;
    return r;
}

pp::BatchStatus WaitBatch(pp::Batch batch) {
    for (int i = 0; i < 500; i++) {
        const auto status = pp::Status(batch);
        if (status.state != pp::BatchState::Pending) return status;
        std::this_thread::sleep_for(10ms);
    }
    throw std::runtime_error("an asset batch never reached a terminal state");
}

void SetBuilding(bool enabled) {
#ifdef _WIN32
    _putenv_s("OPENSHAO_PIPELINE_PRECOMPILE", enabled ? "1" : "0");
#else
    setenv("OPENSHAO_PIPELINE_PRECOMPILE", enabled ? "1" : "0", 1);
#endif
}

void PredictionTests(const std::filesystem::path& root) {
    const auto file = root / "predictions.bin";
    const std::vector<uint32_t> words{31, 32, 33, 34};
    const auto recipe = MakeRecipe(40, pp::ProgramHash(words));
    {
        FakeBuilder b;
        b.hold = true;
        pp::Start(file, 11, &b);
        const auto a = pp::BeginBatch(0xA3), second = pp::BeginBatch(0xA4);
        auto submitted_words = words;
        Require(a && second && a != second, "each asset receives a distinct batch");
        Require(pp::Submit(a, recipe, submitted_words), "a new asset submits a previously unseen pipeline");
        Require(pp::Submit(a, recipe, words), "duplicate submission is accepted without duplicate accounting");
        Require(pp::Submit(second, recipe, words), "two assets may require the same pipeline");
        pp::SealBatch(a);
        pp::SealBatch(second);
        const auto pending = pp::Status(a);
        Require(pending.state == pp::BatchState::Pending && pending.pending == 1 && pending.total == 1,
                "an asset is pending while its unseen pipeline compiles");
        Require(!pp::Submit(a, MakeRecipe(41)), "a sealed batch cannot silently gain more work");
        std::fill(submitted_words.begin(), submitted_words.end(), 0);
        b.Release();
        Require(pp::WaitBatch(a, 5000).state == pp::BatchState::Ready && WaitBatch(second).state == pp::BatchState::Ready,
                "one shared compilation releases both assets");
        Require(b.Count() == 1, "predicted pipelines are deduplicated between assets");
        { std::lock_guard lock(b.mutex); Require(b.programs[0] == words, "prediction owns the program supplied to its worker"); }
        pp::ReleaseBatch(a);
        Require(pp::Status(a).state == pp::BatchState::Cancelled, "released batch handles cannot report readiness");
        pp::ReleaseBatch(second);
        pp::Shutdown();
    }
    {
        FakeBuilder b;
        pp::Start(file, 11, &b);
        std::this_thread::sleep_for(40ms);
        Require(b.Count() == 0, "unobserved speculative recipes are not persisted across sessions");
        const auto a = pp::BeginBatch(0xA3);
        Require(pp::Submit(a, recipe), "a prediction may wait for its vertex program");
        pp::SealBatch(a);
        Require(pp::Status(a).pending == 1 && pp::Status(a).state == pp::BatchState::Pending,
                "unknown programs remain asset dependencies rather than reporting ready");
        pp::RegisterProgram(words);
        Require(WaitBatch(a).state == pp::BatchState::Ready, "registering the asset's program releases its batch");
        pp::Record(recipe, words, 0xA3);
        pp::ReleaseBatch(a);
        pp::Shutdown();
    }
    {
        FakeBuilder b;
        pp::Start(file, 11, &b);
        Require(b.WaitFor(1) && b.Built(40), "a predicted pipeline is persisted when ordinary rendering observes it");
        const auto ready = pp::BeginBatch(0xA3);
        Require(pp::Submit(ready, recipe), "an asset can adopt a completed boot pipeline");
        pp::SealBatch(ready);
        Require(WaitBatch(ready).state == pp::BatchState::Ready, "completed work satisfies a new asset");
        pp::ReleaseBatch(ready);
        b.fail_id = 42;
        const auto failed = pp::BeginBatch(0xA4);
        Require(pp::Submit(failed, MakeRecipe(42)), "a supported new prediction is accepted before compilation");
        pp::SealBatch(failed);
        const auto status = WaitBatch(failed);
        Require(status.state == pp::BatchState::Failed && status.failed == 1 && status.pending == 0,
                "compiler failure completes the asset with a distinct failed result");
        pp::Record(MakeRecipe(42), {}, 0xA4);
        const auto recovered = pp::BeginBatch(0xA4);
        Require(pp::Submit(recovered, MakeRecipe(42)), "successful draw recording recovers failed prediction");
        pp::SealBatch(recovered);
        Require(WaitBatch(recovered).state == pp::BatchState::Ready, "the recovered pipeline is available to later assets");
        pp::ReleaseBatch(recovered);
        pp::ReleaseBatch(failed);
        pp::Shutdown();
    }
    {
        FakeBuilder b;
        pp::Start(root / "cancel.bin", 11, &b);
        const auto open = pp::BeginBatch(0xA3);
        Require(pp::Status(open).state == pp::BatchState::Pending, "an unsealed empty batch is not ready");
        const auto cancelled = pp::BeginBatch(0xA3);
        Require(pp::Submit(cancelled, MakeRecipe(43, 0x12345)), "unresolved cancellation fixture is queued");
        pp::SealBatch(cancelled);
        pp::CancelBatch(cancelled);
        Require(pp::Status(cancelled).state == pp::BatchState::Cancelled, "asset cancellation resolves blocked dependencies");
        pp::Shutdown();
        Require(pp::Status(open).state == pp::BatchState::Cancelled, "shutdown resolves unsealed batches");
        pp::Start(root / "cancel.bin", 11, &b);
        const auto fresh = pp::BeginBatch(0xA3);
        Require(fresh != open && pp::Status(open).state == pp::BatchState::Cancelled,
                "session restart cannot reuse an old asset token");
        pp::SealBatch(fresh);
        Require(pp::Status(fresh).state == pp::BatchState::Ready, "a sealed empty batch is ready");
        pp::ReleaseBatch(fresh);
        const auto invalid = pp::BeginBatch(0xA3);
        auto oversized = MakeRecipe(44);
        oversized.key_size = uint32_t(pp::kMaxPipelineKey + 1);
        Require(!pp::Submit(invalid, oversized), "malformed key sizes are rejected before reading key bytes");
        Require(!pp::Submit(invalid, MakeRecipe(45, 0x12345), words), "mismatched program words cannot satisfy another hash");
        pp::SealBatch(invalid);
        const auto rejected = pp::Status(invalid);
        Require(rejected.state == pp::BatchState::Failed && rejected.failed == 2 && rejected.total == 2,
                "rejected metadata prevents false asset readiness");
        pp::ReleaseBatch(invalid);
        std::vector<pp::Batch> batches;
        for (int i = 0; i < 20000; i++) {
            const auto batch = pp::BeginBatch(0xA3);
            if (!batch) break;
            batches.push_back(batch);
        }
        Require(!batches.empty() && batches.size() < 20000, "active asset batch storage is bounded");
        for (auto batch : batches) pp::ReleaseBatch(batch);
        const auto reclaimed = pp::BeginBatch(0xA3);
        Require(reclaimed != 0, "releasing asset batches reclaims capacity");
        pp::ReleaseBatch(reclaimed);
        pp::Shutdown();
    }
    {
        SetBuilding(false);
        FakeBuilder b;
        pp::Start(root / "disabled.bin", 11, &b);
        const auto disabled = pp::BeginBatch(0xA3);
        Require(pp::Status(disabled).state == pp::BatchState::Disabled, "disabled precompilation does not stall asset loading");
        Require(!pp::Submit(disabled, MakeRecipe(44)), "disabled batches do not enqueue compilation");
        pp::SealBatch(disabled);
        pp::ReleaseBatch(disabled);
        pp::Shutdown();
        SetBuilding(true);
    }
}
}

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("openshao-pipeline-precompile-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    const auto file = root / "pipelines.bin";
    const std::vector<uint32_t> program{1, 2, 3, 4, 5, 6, 7, 8};
    const std::vector<uint32_t> late_program{9, 10, 11, 12};
    int result = 0;
    try {
        PredictionTests(root);
        {
            FakeBuilder b;
            pp::Start(file, 7, &b);
            pp::Record(MakeRecipe(1), {}, 0xA3);
            pp::Record(MakeRecipe(2, pp::ProgramHash(program)), program, 0xA4);
            pp::Record(MakeRecipe(3, pp::ProgramHash(late_program)), {}, 0xA4);
            pp::Record(MakeRecipe(1), {}, 0xA5);  // one recipe, a second room
            pp::Shutdown();
            Require(b.Count() == 0, "recipes recorded during play are already built");
        }
        {
            FakeBuilder b;
            pp::Start(file, 7, &b);
            Require(b.WaitFor(2), "a restart rebuilds the recorded recipes");
            std::this_thread::sleep_for(50ms);
            Require(b.Count() == 2, "a recipe whose program is unknown waits");
            Require(b.Built(1) && b.Built(2), "fixed function and stored programs build at boot");
            {
                std::lock_guard lock(b.mutex);
                for (size_t i = 0; i < b.built.size(); i++)
                    if (b.built[i] == 2) Require(b.programs[i] == program, "the stored program reaches the builder");
            }
            pp::RegisterProgram(late_program);
            Require(b.WaitFor(3) && b.Built(3), "creating the program releases its recipes");
            pp::Shutdown();
        }
        {
            std::ofstream tail(file, std::ios::binary | std::ios::app);
            tail << "interrupted";
        }
        {
            FakeBuilder b;
            b.delay = 30ms;
            pp::Start(file, 7, &b);
            pp::RoomLoading(0xA4);
            const auto start = std::chrono::steady_clock::now();
            pp::WaitForRoom(0xA4);
            Require(std::chrono::steady_clock::now() - start < 1500ms, "a room's wait ends when its pipelines exist");
            Require(b.Built(2), "the loading room's buildable pipelines are built before the wait ends");
            pp::RegisterProgram(late_program);
            Require(b.WaitFor(3), "an interrupted tail keeps the complete records");
            pp::Shutdown();
        }
        {
            FakeBuilder b;
            pp::Start(file, 8, &b);
            std::this_thread::sleep_for(50ms);
            Require(b.Count() == 0, "recipes from another key layout are discarded");
            pp::Shutdown();
        }
        std::puts("pipeline precompile: ok");
    } catch (const std::exception& e) {
        pp::Shutdown();
        std::fprintf(stderr, "pipeline precompile: %s\n", e.what());
        result = 1;
    }
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    return result;
}
