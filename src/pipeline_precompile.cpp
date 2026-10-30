#include "pipeline_precompile.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

#include "core/log.h"

namespace openshao::pipelines {
namespace {

constexpr uint64_t kMagic = 0x3145504950534F4Full;  // "OSPIPE1"
constexpr uint32_t kFormat = 1;
constexpr uint64_t kFileLimit = 64ull * 1024 * 1024;
constexpr size_t kEntryLimit = 100000;
constexpr size_t kBatchLimit = 4096;
constexpr unsigned kDefaultWaitMs = 2000;

enum : uint32_t { kRecordRecipe = 1, kRecordRoom = 2, kRecordProgram = 3 };

struct FileHeader {
    uint64_t magic;
    uint64_t layout;
    uint32_t format;
    uint32_t recipe_size;
};
struct RecordHeader {
    uint32_t type;
    uint32_t size;
    uint64_t checksum;
};
struct RoomTag {
    uint64_t key;
    uint32_t room;
    uint32_t reserved;
};

uint64_t Checksum(const void* data, size_t size, uint64_t seed) {
    const auto* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; i++) seed = (seed ^ p[i]) * 1099511628211ull;
    return seed;
}

enum class State : uint8_t { Blocked, Queued, Building, Done, Failed };

struct Entry {
    Recipe recipe;
    uint64_t key = 0;
    State state = State::Done;
    bool persisted = false;
    bool observed = false;
    std::vector<uint32_t> rooms;
    std::vector<uint32_t> written_rooms;
};

struct AssetBatch {
    uint32_t room = 0;
    bool sealed = false, cancelled = false, disabled = false;
    uint32_t rejected = 0;
    std::vector<size_t> entries;
};

unsigned EnvUnsigned(const char* name, unsigned fallback) {
    const char* v = std::getenv(name);
    if (!v || !*v) return fallback;
    char* end = nullptr;
    const unsigned long n = std::strtoul(v, &end, 10);
    return end && *end == 0 ? unsigned(std::min<unsigned long>(n, 600000)) : fallback;
}

void LowerThreadPriority() {
#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#else
    setpriority(PRIO_PROCESS, id_t(syscall(SYS_gettid)), 5);
#endif
}

using Clock = std::chrono::steady_clock;

class Precompiler {
public:
    // Process exit: the log may already be gone.
    ~Precompiler() { Stop(false); }

    void Start(const std::filesystem::path& file, uint64_t layout, Builder* builder) {
        Stop();
        std::lock_guard lock(mutex_);
        Reset();
        path_ = file;
        layout_ = layout;
        builder_ = builder;
        Load();
        started_ = Clock::now();
        enabled_ = EnvUnsigned("OPENSHAO_PIPELINE_PRECOMPILE", 1) != 0;
        wait_ms_ = EnvUnsigned("OPENSHAO_PIPELINE_WAIT_MS", kDefaultWaitMs);
        size_t blocked = 0;
        for (size_t i = 0; i < entries_.size(); i++) {
            if (!enabled_) break;
            if (Buildable(entries_[i])) Enqueue(i, false);
            else Block(i);
            blocked += entries_[i].state == State::Blocked;
        }
        const unsigned cores = std::max(1u, std::thread::hardware_concurrency());
        const unsigned workers = enabled_ ? std::clamp(cores > 4 ? cores - 2 : cores / 2, 1u, 4u) : 1u;
        LOGR("pipeline precompile: %zu recipes, %zu programs, %zu wait for programs; %u workers%s",
             entries_.size(), programs_.size(), blocked, workers, enabled_ ? "" : " (building disabled)");
        stop_ = false;
        for (unsigned i = 0; i < workers; i++) workers_.emplace_back([this] { Run(); });
    }

    void Stop(bool report = true) {
        {
            std::lock_guard lock(mutex_);
            if (workers_.empty()) return;
            stop_ = true;
            for (auto& [id, batch] : batches_) batch.cancelled = true;
        }
        work_.notify_all();
        done_.notify_all();
        for (auto& t : workers_) t.join();
        workers_.clear();
        std::unique_lock lock(mutex_);
        WritePending(lock);
        if (report) Report("session");
        builder_ = nullptr;
    }

    void Record(const Recipe& recipe, std::span<const uint32_t> program, uint32_t room) {
        if (!recipe.key_size || recipe.key_size > kMaxPipelineKey) return;
        const uint64_t key = KeyHash({recipe.key, recipe.key_size});
        std::lock_guard lock(mutex_);
        if (!builder_) return;
        auto it = by_key_.find(key);
        size_t index;
        if (it == by_key_.end()) {
            if (entries_.size() >= kEntryLimit) return;
            index = entries_.size();
            Entry& e = entries_.emplace_back();
            e.recipe = recipe;
            e.key = key;
            e.state = State::Done;
            by_key_[key] = index;
        } else {
            index = it->second;
        }
        Entry& entry = entries_[index];
        entry.observed = true;
        if (!entry.persisted) {
            entry.recipe = recipe;
            entry.persisted = true;
            Append(kRecordRecipe, &recipe, sizeof(recipe));
            recorded_++;
        }
        // A successful ordinary draw can overtake queued prediction or recover a failed build.
        // A running worker still owns its completion, and observes this flag in Finish.
        if (entry.state == State::Queued) RemovePendingRooms(entry);
        if (entry.state != State::Building) entry.state = State::Done;
        AddRoom(index, room, true);
        if (recipe.vertex.program && !program.empty() && !written_programs_.count(recipe.vertex.program)) {
            StoreProgram(recipe.vertex.program, program, true);
        }
        if (room != current_room_) PrioritizeRoom(room);
        if (!pending_.empty()) work_.notify_one();
        done_.notify_all();
    }

    Batch BeginBatch(uint32_t room) {
        std::lock_guard lock(mutex_);
        if (batches_.size() >= kBatchLimit) return 0;
        // Tokens survive Reset so a delayed load callback cannot refer to a new session's asset.
        const Batch id = ++next_batch_;
        AssetBatch batch;
        batch.room = room;
        batch.disabled = !builder_ || !enabled_ || stop_;
        batches_.emplace(id, std::move(batch));
        return id;
    }

    bool Submit(Batch id, const Recipe& recipe, std::span<const uint32_t> program) {
        std::lock_guard lock(mutex_);
        auto it = batches_.find(id);
        if (it == batches_.end()) return false;
        AssetBatch& batch = it->second;
        if (batch.sealed || batch.cancelled || batch.disabled || !builder_ || stop_) return false;
        if (!recipe.key_size || recipe.key_size > kMaxPipelineKey ||
            (!program.empty() && (!recipe.vertex.program || ProgramHash(program) != recipe.vertex.program))) {
            batch.rejected++;
            return false;
        }
        const uint64_t key = KeyHash({recipe.key, recipe.key_size});
        auto known = by_key_.find(key);
        size_t index;
        if (known == by_key_.end()) {
            if (entries_.size() >= kEntryLimit) {
                batch.rejected++;
                return false;
            }
            index = entries_.size();
            Entry entry;
            entry.recipe = recipe;
            entry.key = key;
            entry.state = State::Blocked;
            entries_.push_back(std::move(entry));
            by_key_.emplace(key, index);
            AddRoom(index, batch.room, false);
            if (!program.empty()) StoreProgram(recipe.vertex.program, program, false);
            if (Buildable(entries_[index])) Enqueue(index, true);
            else Block(index);
            reported_ = false;
        } else {
            index = known->second;
            const Entry& entry = entries_[index];
            if (entry.recipe.key_size != recipe.key_size ||
                std::memcmp(entry.recipe.key, recipe.key, recipe.key_size) != 0) {
                batch.rejected++;
                return false;
            }
            AddRoom(index, batch.room, false);
            if (!program.empty()) StoreProgram(recipe.vertex.program, program, false);
        }
        if (std::find(batch.entries.begin(), batch.entries.end(), index) == batch.entries.end())
            batch.entries.push_back(index);
        return true;
    }

    void SealBatch(Batch id) {
        std::lock_guard lock(mutex_);
        if (auto it = batches_.find(id); it != batches_.end()) it->second.sealed = true;
        done_.notify_all();
    }

    BatchStatus Status(Batch id) {
        std::lock_guard lock(mutex_);
        return StatusLocked(id);
    }

    BatchStatus WaitBatch(Batch id, uint32_t timeout_ms) {
        std::unique_lock lock(mutex_);
        done_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&] {
            return stop_ || StatusLocked(id).state != BatchState::Pending;
        });
        return StatusLocked(id);
    }

    BatchStatus StatusLocked(Batch id) {
        auto it = batches_.find(id);
        if (it == batches_.end()) return {};
        const AssetBatch& batch = it->second;
        BatchStatus status;
        status.total = uint32_t(batch.entries.size()) + batch.rejected;
        status.failed = batch.rejected;
        if (batch.cancelled) return status;
        if (batch.disabled) { status.state = BatchState::Disabled; return status; }
        for (size_t index : batch.entries) {
            const State state = entries_[index].state;
            status.failed += state == State::Failed;
            status.pending += state == State::Blocked || state == State::Queued || state == State::Building;
        }
        status.state = !batch.sealed || status.pending ? BatchState::Pending
            : status.failed ? BatchState::Failed : BatchState::Ready;
        return status;
    }

    void CancelBatch(Batch id) {
        std::lock_guard lock(mutex_);
        if (auto it = batches_.find(id); it != batches_.end()) it->second.cancelled = true;
        done_.notify_all();
    }

    void ReleaseBatch(Batch id) {
        std::lock_guard lock(mutex_);
        batches_.erase(id);
        done_.notify_all();
    }

    void RegisterProgram(uint64_t hash, std::span<const uint32_t> words) {
        std::lock_guard lock(mutex_);
        if (!builder_ || programs_.count(hash)) return;
        StoreProgram(hash, words, false);
    }

    void RoomLoading(uint32_t room) {
        std::lock_guard lock(mutex_);
        if (builder_ && room != current_room_) PrioritizeRoom(room);
    }

    void WaitForRoom(uint32_t room) {
        std::unique_lock lock(mutex_);
        // Once per room entry: later loads in the same room may stream during play.
        if (!builder_ || !enabled_ || !wait_ms_ || room == waited_room_) return;
        waited_room_ = room;
        auto pending = [&] {
            auto it = room_pending_.find(room);
            return it == room_pending_.end() ? 0 : it->second;
        };
        const int before = pending();
        if (!before) return;
        const auto start = Clock::now();
        const bool finished = done_.wait_for(lock, std::chrono::milliseconds(wait_ms_),
                                             [&] { return stop_ || pending() == 0; });
        const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        LOGR("pipeline precompile: room %02X loading waited %.0f ms for %d pipelines%s", room, ms, before,
             finished ? "" : ", continuing before all were built");
    }

private:
    void Reset() {
        entries_.clear();
        by_key_.clear();
        programs_.clear();
        written_programs_.clear();
        waiting_.clear();
        urgent_.clear();
        normal_.clear();
        room_pending_.clear();
        pending_.clear();
        batches_.clear();
        current_room_ = waited_room_ = ~0u;
        built_ = failed_ = recorded_ = 0;
        reported_ = false;
        file_ok_ = false;
    }

    static bool Buildable(const Entry& e, const std::unordered_map<uint64_t, std::vector<uint32_t>>& programs) {
        return !e.recipe.vertex.program || programs.count(e.recipe.vertex.program);
    }
    bool Buildable(const Entry& e) const { return Buildable(e, programs_); }

    void Block(size_t i) {
        entries_[i].state = State::Blocked;
        waiting_[entries_[i].recipe.vertex.program].push_back(i);
    }

    void Enqueue(size_t i, bool urgent) {
        Entry& e = entries_[i];
        e.state = State::Queued;
        for (uint32_t room : e.rooms) room_pending_[room]++;
        if (urgent || std::find(e.rooms.begin(), e.rooms.end(), current_room_) != e.rooms.end()) urgent_.push_back(i);
        else normal_.push_back(i);
        work_.notify_one();
    }

    void RemovePendingRooms(const Entry& e) {
        for (uint32_t room : e.rooms) {
            auto it = room_pending_.find(room);
            if (it != room_pending_.end() && --it->second <= 0) room_pending_.erase(it);
        }
    }

    void Finish(size_t i, bool ok) {
        Entry& e = entries_[i];
        ok = ok || e.observed;
        e.state = ok ? State::Done : State::Failed;
        RemovePendingRooms(e);
        (ok ? built_ : failed_)++;
        done_.notify_all();
    }

    void AddRoom(size_t i, uint32_t room, bool write) {
        Entry& e = entries_[i];
        if (std::find(e.rooms.begin(), e.rooms.end(), room) == e.rooms.end()) {
            e.rooms.push_back(room);
            if (e.state == State::Queued || e.state == State::Building) room_pending_[room]++;
        }
        if (write && std::find(e.written_rooms.begin(), e.written_rooms.end(), room) == e.written_rooms.end()) {
            e.written_rooms.push_back(room);
            const RoomTag tag{e.key, room, 0};
            Append(kRecordRoom, &tag, sizeof(tag));
        }
    }

    void StoreProgram(uint64_t hash, std::span<const uint32_t> words, bool write) {
        // Workers may be reading a stored program: only add programs, never replace one.
        programs_.try_emplace(hash, words.begin(), words.end());
        if (write && written_programs_.insert(hash).second) {
            std::vector<uint8_t> bytes(8 + words.size_bytes());
            std::memcpy(bytes.data(), &hash, 8);
            std::memcpy(bytes.data() + 8, words.data(), words.size_bytes());
            Append(kRecordProgram, bytes.data(), bytes.size());
        }
        auto it = waiting_.find(hash);
        if (it == waiting_.end()) return;
        const auto released = std::move(it->second);
        waiting_.erase(it);
        // A program created while an asset loads releases that asset's pipelines right away.
        if (enabled_) for (size_t i : released) if (entries_[i].state == State::Blocked) Enqueue(i, true);
        if (!released.empty()) LOG_FIRST(32, "pipeline precompile: program %016llx released %zu pipelines",
                                         static_cast<unsigned long long>(hash), released.size());
    }

    void PrioritizeRoom(uint32_t room) {
        current_room_ = room;
        size_t moved = 0;
        for (size_t i = 0; i < entries_.size(); i++) {
            const Entry& e = entries_[i];
            if (e.state != State::Queued) continue;
            if (std::find(e.rooms.begin(), e.rooms.end(), room) == e.rooms.end()) continue;
            urgent_.push_back(i);  // the stale normal_ slot is skipped once this one runs
            moved++;
        }
        if (moved) {
            LOGR("pipeline precompile: room %02X first, %zu pipelines queued", room, moved);
            work_.notify_all();
        }
    }

    bool NextIndex(size_t& index) {
        for (auto* queue : {&urgent_, &normal_}) {
            while (!queue->empty()) {
                index = queue->front();
                queue->pop_front();
                if (entries_[index].state == State::Queued) return true;
            }
        }
        return false;
    }

    void Run() {
        LowerThreadPriority();
        std::unique_lock lock(mutex_);
        for (;;) {
            work_.wait(lock, [&] {
                return stop_ || (!pending_.empty() && !writing_) || !urgent_.empty() || !normal_.empty();
            });
            if (stop_) return;
            if (!pending_.empty() && !writing_) {
                WritePending(lock);
                continue;
            }
            size_t index;
            if (!NextIndex(index)) {
                MaybeReport();
                continue;
            }
            Entry& e = entries_[index];
            e.state = State::Building;
            const Recipe recipe = e.recipe;
            std::span<const uint32_t> program;
            if (recipe.vertex.program) {
                // Stored programs are never replaced while workers run.
                const auto& words = programs_.at(recipe.vertex.program);
                program = {words.data(), words.size()};
            }
            Builder* builder = builder_;
            lock.unlock();
            bool ok = false;
            try {
                if (builder) ok = builder->BuildPipeline(recipe, program);
            } catch (...) {
                LOGE("pipeline precompile: pipeline builder failed with an exception");
            }
            lock.lock();
            Finish(index, ok);
            MaybeReport();
        }
    }

    void MaybeReport() {
        if (reported_ || !urgent_.empty() || !normal_.empty()) return;
        for (const auto& [id, batch] : batches_) if (!batch.sealed && !batch.cancelled && !batch.disabled) return;
        for (const Entry& e : entries_) if (e.state == State::Building) return;
        reported_ = true;
        Report("boot queue finished");
    }

    void Report(const char* when) {
        const double ms = std::chrono::duration<double, std::milli>(Clock::now() - started_).count();
        size_t blocked = 0;
        for (const Entry& e : entries_) blocked += e.state == State::Blocked;
        LOGR("pipeline precompile: %s after %.0f ms; built %zu, failed %zu, recorded during play %zu, "
             "waiting for programs %zu", when, ms, built_, failed_, recorded_, blocked);
    }

    // Callers hold mutex_.
    void Append(uint32_t type, const void* data, size_t size) {
        if (!file_ok_) return;
        const RecordHeader h{type, uint32_t(size), Checksum(data, size, type)};
        const size_t at = pending_.size();
        pending_.resize(at + sizeof(h) + size);
        std::memcpy(pending_.data() + at, &h, sizeof(h));
        std::memcpy(pending_.data() + at + sizeof(h), data, size);
    }

    // Writes outside mutex_ so a draw recording a pipeline never waits for the disk.
    void WritePending(std::unique_lock<std::mutex>& lock) {
        if (pending_.empty() || writing_) return;
        std::vector<uint8_t> bytes = std::move(pending_);
        pending_.clear();
        writing_ = true;
        const bool fits = file_ok_ && file_size_ + bytes.size() <= kFileLimit;
        lock.unlock();
        bool ok = true;
        if (fits) {
            std::ofstream out(path_, std::ios::binary | std::ios::app);
            out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
            ok = bool(out);
        }
        lock.lock();
        writing_ = false;
        if (fits && ok) file_size_ += bytes.size();
        if (fits && !ok) {
            file_ok_ = false;
            LOGE("pipeline precompile: cannot write %s", path_.string().c_str());
        }
        if (!pending_.empty()) work_.notify_one();
    }

    void Load() {
        std::error_code ec;
        std::filesystem::create_directories(path_.parent_path(), ec);
        std::vector<uint8_t> data;
        {
            std::ifstream in(path_, std::ios::binary);
            if (in) data.assign(std::istreambuf_iterator<char>(in), {});
        }
        const FileHeader expected{kMagic, layout_, kFormat, uint32_t(sizeof(Recipe))};
        size_t good = 0;
        FileHeader header{};
        if (data.size() >= sizeof(header) && data.size() <= kFileLimit) std::memcpy(&header, data.data(), sizeof(header));
        if (std::memcmp(&header, &expected, sizeof(header)) == 0) {
            good = sizeof(header);
            while (data.size() - good >= sizeof(RecordHeader)) {
                RecordHeader h;
                std::memcpy(&h, data.data() + good, sizeof(h));
                const size_t at = good + sizeof(h);
                if (h.size > data.size() - at || Checksum(data.data() + at, h.size, h.type) != h.checksum) break;
                if (!Parse(h.type, data.data() + at, h.size)) break;
                good = at + h.size;
            }
        } else if (!data.empty()) {
            LOGR("pipeline precompile: recipes from another renderer layout discarded");
        }
        if (good) {
            // Drop an interrupted tail so appended records stay readable.
            if (good != data.size()) std::filesystem::resize_file(path_, good, ec);
            file_ok_ = !ec;
            file_size_ = good;
        } else {
            std::ofstream out(path_, std::ios::binary | std::ios::trunc);
            out.write(reinterpret_cast<const char*>(&expected), sizeof(expected));
            file_ok_ = bool(out);
            file_size_ = sizeof(expected);
        }
        if (!file_ok_) LOGE("pipeline precompile: %s is not writable; recipes are not kept", path_.string().c_str());
    }

    bool Parse(uint32_t type, const uint8_t* p, size_t size) {
        if (type == kRecordRecipe) {
            if (size != sizeof(Recipe) || entries_.size() >= kEntryLimit) return false;
            Entry e;
            std::memcpy(&e.recipe, p, sizeof(Recipe));
            if (e.recipe.key_size > kMaxPipelineKey) return false;
            e.key = KeyHash({e.recipe.key, e.recipe.key_size});
            e.persisted = true;
            if (by_key_.count(e.key)) return true;
            by_key_[e.key] = entries_.size();
            entries_.push_back(std::move(e));
            return true;
        }
        if (type == kRecordRoom) {
            if (size != sizeof(RoomTag)) return false;
            RoomTag tag;
            std::memcpy(&tag, p, sizeof(tag));
            auto it = by_key_.find(tag.key);
            if (it != by_key_.end()) {
                AddRoom(it->second, tag.room, false);
                entries_[it->second].written_rooms.push_back(tag.room);
            }
            return true;
        }
        if (type == kRecordProgram) {
            if (size < 8 || (size - 8) % 4) return false;
            uint64_t hash;
            std::memcpy(&hash, p, 8);
            std::vector<uint32_t> words((size - 8) / 4);
            std::memcpy(words.data(), p + 8, size - 8);
            programs_.try_emplace(hash, std::move(words));
            written_programs_.insert(hash);
            return true;
        }
        return false;
    }

    std::mutex mutex_;
    std::condition_variable work_, done_;
    std::vector<std::thread> workers_;
    bool stop_ = false, enabled_ = true, reported_ = false, file_ok_ = false, writing_ = false;
    Builder* builder_ = nullptr;
    std::filesystem::path path_;
    uint64_t layout_ = 0, file_size_ = 0;
    unsigned wait_ms_ = kDefaultWaitMs;
    std::vector<Entry> entries_;
    std::unordered_map<Batch, AssetBatch> batches_;
    Batch next_batch_ = 0;
    std::unordered_map<uint64_t, size_t> by_key_;
    std::unordered_map<uint64_t, std::vector<uint32_t>> programs_;
    std::unordered_set<uint64_t> written_programs_;
    std::unordered_map<uint64_t, std::vector<size_t>> waiting_;
    std::deque<size_t> urgent_, normal_;
    std::unordered_map<uint32_t, int> room_pending_;
    std::vector<uint8_t> pending_;
    uint32_t current_room_ = ~0u, waited_room_ = ~0u;
    size_t built_ = 0, failed_ = 0, recorded_ = 0;
    Clock::time_point started_ = Clock::now();
};

Precompiler& Instance() {
    static Precompiler p;
    return p;
}

}  // namespace

uint64_t KeyHash(std::span<const uint8_t> key) { return gpu::Hash(key.data(), key.size()); }
uint64_t ProgramHash(std::span<const uint32_t> words) { return gpu::Hash(words.data(), words.size_bytes()) | 1; }

void Start(const std::filesystem::path& file, uint64_t layout, Builder* builder) {
    Instance().Start(file, layout, builder);
}
void Shutdown() { Instance().Stop(); }
void Record(const Recipe& recipe, std::span<const uint32_t> program, uint32_t room) {
    Instance().Record(recipe, program, room);
}
Batch BeginBatch(uint32_t room) { return Instance().BeginBatch(room); }
bool Submit(Batch batch, const Recipe& recipe, std::span<const uint32_t> program) {
    return Instance().Submit(batch, recipe, program);
}
void SealBatch(Batch batch) { Instance().SealBatch(batch); }
BatchStatus Status(Batch batch) { return Instance().Status(batch); }
BatchStatus WaitBatch(Batch batch, uint32_t timeout_ms) { return Instance().WaitBatch(batch, timeout_ms); }
void CancelBatch(Batch batch) { Instance().CancelBatch(batch); }
void ReleaseBatch(Batch batch) { Instance().ReleaseBatch(batch); }
void RegisterProgram(std::span<const uint32_t> words) {
    if (!words.empty()) Instance().RegisterProgram(ProgramHash(words), words);
}
void RoomLoading(uint32_t room) { Instance().RoomLoading(room); }
void WaitForRoom(uint32_t room) { Instance().WaitForRoom(room); }

}  // namespace openshao::pipelines
