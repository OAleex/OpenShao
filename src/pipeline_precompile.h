#pragma once
// Pipeline precompilation: every pipeline a draw uses is recorded as a recipe (the inputs that
// built it, not driver output), tagged with the room that was loading. Background workers rebuild
// the recipes at boot and move a room's recipes to the front of the queue when that room loads, so
// a draw finds its pipeline already made instead of compiling it on the render thread.
//
// Recipes survive renderer and driver updates: the backend regenerates the HLSL from the recipe.
// A recipe that uses one of the title's vertex programs waits until the program is known, either
// from this machine's recipe file or from the title creating the shader while it loads an asset.
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <span>
#include <unordered_set>

#include "gpu/shaders.h"

namespace openshao::pipelines {

// The vertex shader inputs of a draw, as the backends generate HLSL from them.
struct VertexRecipe {
    uint64_t program = 0;  // the title's program hash (VertexShader::hash), 0 for fixed function
    uint64_t decode = 0;
    uint16_t regs = 0;
    uint8_t pretransformed = 0;
    uint8_t quad = 0;
    uint32_t reserved = 0;
    gpu::FixedKey fixed{};
};
static_assert(sizeof(VertexRecipe) == 48);

inline constexpr size_t kMaxPipelineKey = 320;  // the largest backend key, rounded up

// Written to disk as raw bytes: keep members free of implicit padding.
struct Recipe {
    uint64_t vs_source = 0;  // source hashes when recorded; a mismatch means the HLSL generator changed
    uint64_t ps_source = 0;
    VertexRecipe vertex{};
    gpu::PixelKey pixel{};
    uint32_t key_size = 0;   // the backend's pipeline key, byte for byte
    uint8_t key[kMaxPipelineKey] = {};
};
static_assert(sizeof(gpu::PixelKey) % 4 == 0);
static_assert(sizeof(Recipe) == 16 + sizeof(VertexRecipe) + sizeof(gpu::PixelKey) + 4 + kMaxPipelineKey);

uint64_t KeyHash(std::span<const uint8_t> key);

class Builder {
public:
    virtual ~Builder() = default;
    // Called on a worker thread. program is empty for fixed function recipes. Builds and keeps the
    // pipeline for the next draw that asks for it; false when it cannot be built.
    virtual bool BuildPipeline(const Recipe& recipe, std::span<const uint32_t> program) = 0;
};

// The backend's device is ready. layout identifies the backend and its key layout: recipes from a
// different layout are discarded.
void Start(const std::filesystem::path& file, uint64_t layout, Builder* builder);
// Stops the workers. Call before the backend's device or pipeline storage is released.
void Shutdown();

// Render thread: a pipeline was used successfully for the first time. room is CurrentRoom().
void Record(const Recipe& recipe, std::span<const uint32_t> program, uint32_t room);

using Batch = uint64_t;
enum class BatchState : uint8_t { Pending, Ready, Failed, Cancelled, Disabled };
struct BatchStatus {
    BatchState state = BatchState::Cancelled;
    uint32_t pending = 0, failed = 0, total = 0;
};
// An asset owns a batch until ReleaseBatch. Zero means the active-batch limit was reached.
Batch BeginBatch(uint32_t room);
// Prediction never persists a recipe. Record persists it when ordinary rendering observes it.
// Program words are copied; an omitted program remains a dependency until RegisterProgram.
bool Submit(Batch batch, const Recipe& recipe, std::span<const uint32_t> program = {});
void SealBatch(Batch batch);
// Unknown or released handles are Cancelled. Ready requires sealing and successful dependencies.
BatchStatus Status(Batch batch);
// Startup passes have all shader inputs available. Wait without polling the renderer.
BatchStatus WaitBatch(Batch batch, uint32_t timeout_ms);
// Cancels the asset dependency; compilation shared with other assets may still finish.
void CancelBatch(Batch batch);
void ReleaseBatch(Batch batch);

// The room the title is in or loading, from guest memory.
uint32_t CurrentRoom();
// The title created a vertex shader: recipes that wait for this program can be built.
void RegisterProgram(std::span<const uint32_t> words);
uint64_t ProgramHash(std::span<const uint32_t> words);
// The title is loading a room's assets: its recipes go first.
void RoomLoading(uint32_t room);
// Waits while recipes recorded in this room are queued or building, up to OPENSHAO_PIPELINE_WAIT_MS
// (2000 by default, 0 never waits). Waits once per room entry.
void WaitForRoom(uint32_t room);

// Deduplicates work on one key between the render thread and the workers.
class InFlight {
public:
    // Waits while another thread holds key, then holds it. Check for the result again afterwards.
    void Acquire(uint64_t key) {
        std::unique_lock lock(mutex_);
        released_.wait(lock, [&] { return !keys_.count(key); });
        keys_.insert(key);
    }
    void Release(uint64_t key) {
        {
            std::lock_guard lock(mutex_);
            keys_.erase(key);
        }
        released_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable released_;
    std::unordered_set<uint64_t> keys_;
};

class InFlightScope {
public:
    InFlightScope(InFlight& f, uint64_t key) : f_(f), key_(key) { f_.Acquire(key_); }
    ~InFlightScope() { f_.Release(key_); }
    InFlightScope(const InFlightScope&) = delete;
    InFlightScope& operator=(const InFlightScope&) = delete;

private:
    InFlight& f_;
    uint64_t key_;
};

}  // namespace openshao::pipelines
