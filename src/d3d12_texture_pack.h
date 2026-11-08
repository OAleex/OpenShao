#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <dxgi1_4.h>

#include "gpu/ext.h"
#include "gpu/texture.h"
#include "texture_loader.h"
#include "texture_pack_source.h"
#include "texture_pack_stats.h"
#include "texture_range_allocator.h"
#include "texture_residency.h"

// The Direct3D 12 renderer's side of the texture packs. A game texture asks it for the pack's replacement, found by the hash
// of the level 0 bytes. The loader reads the replacements on threads of its own; once a frame the ones that are ready are
// copied to the GPU, as many as the frame's budget allows. All calls come from the renderer's own thread.
namespace openshao::graphics {
// A replacement in video memory, in the pixel shader resource state.
struct HdImage {
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    uint32_t levels = 0;
    uint64_t bytes = 0;      // the texture data
    size_t heap = 0;         // which heap the resource is placed in
    uint64_t offset = 0;     // where in that heap
    uint64_t allocated = 0;  // what the placement takes there, with alignment
    std::unique_ptr<texpack::Resident> counted;  // keeps the image in the resident figures for as long as it lives
};

// What a game texture knows of its replacement.
struct HdTexState {
    std::optional<texpack::SourceEntry> rep;
    uint64_t rep_hash = 0;
    uint64_t generation = 0;         // the set of packs the lookup was made against; 0 before any lookup
    bool shown = false;              // the replacement has been drawn since it was last asked for
    uint64_t requested = 0;          // frame plus one of the last request to the loader; 0 when none
    uint64_t first_request = 0;      // frame plus one of the first request since the texture last had its image
    uint64_t waited_frames = 0;      // frames drawn as the game's own texture while the replacement was awaited
    uint64_t last_wait = 0;          // frame plus one of the last of those
};

class D3D12HdTextures {
public:
    // Once a frame, before anything is bound or presented: follows a change of the active packs and copies to the GPU the
    // replacements the loader has ready. Later calls in the same frame do nothing. Cheap enough to call on every texture bind.
    void BeginFrame(gpu::Context& context, uint64_t presents);

    // True when `state` was looked up against a set of packs that is no longer the active one.
    bool Stale(const HdTexState& state) const { return state.generation != generation_; }

    // Looks the texture up. `rewritten` means the game changed the memory of a texture that was already looked up, so a
    // replacement it had may no longer apply.
    void Lookup(HdTexState& state, const gpu::TextureDesc& desc, const gpu::FormatInfo& format, const uint8_t* guest,
                bool rewritten);

    // The image to draw instead of the game's texture, or null to draw the game's own. While the replacement is not in video
    // memory, because it is not read yet or was taken out to keep within the budget, it is asked of the loader and the game's
    // texture is drawn. The image is valid until the next BeginFrame.
    const HdImage* Use(gpu::Context& context, HdTexState& state);

    // After a frame is presented.
    void EndFrame(uint64_t presents);

private:
    enum class Sent { Ok, Data, Memory };
    // Textures are placed in heaps of their own kind, many to a heap, so that making one does not ask the driver for memory.
    struct TextureHeap {
        Microsoft::WRL::ComPtr<ID3D12Heap> heap;
        texpack::RangeAllocator ranges;
        explicit TextureHeap(uint64_t size) : ranges(size) {}
    };
    struct Placement {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        size_t heap = 0;
        uint64_t offset = 0, size = 0;
    };
    // Where the rows of a texture are written before the GPU copies them.
    struct Staging {
        ID3D12Resource* resource = nullptr;
        uint64_t offset = 0;
        uint8_t* mapped = nullptr;  // the start of the staged range
        Microsoft::WRL::ComPtr<ID3D12Resource> dedicated;  // a buffer made for this texture alone, released with the frame
    };
    struct UploadSlot {
        Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
        uint8_t* mapped = nullptr;
        uint64_t used = 0;
        uint64_t frame = ~0ull;  // the frame whose copies are using it
    };
    struct DeferredFree {
        size_t heap;
        uint64_t offset, size, due;
    };

    void SyncSource();
    void Pump(gpu::Context& context);
    Sent Send(gpu::Context& context, const texpack::Loaded& loaded);
    bool Place(ID3D12Device* device, const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_DESC& placed, Placement& out);
    bool Stage(ID3D12Device* device, uint64_t size, Staging& out);
    bool EnsureSlot(ID3D12Device* device, UploadSlot& slot);
    // A new heap of at least `size` bytes, the one made ahead of time when `size` is the usual heap size. Its index, or
    // the number of heaps when none could be made.
    size_t AddHeap(ID3D12Device* device, uint64_t size);
    // Makes what the first sends need, so that they do not wait for the driver to find memory.
    void Warm(ID3D12Device* device);
    // Starts making the next heap on another thread when the last one is nearly full.
    void PrefetchHeap(ID3D12Device* device);
    // How much video memory the replacements may take; worked out again every 60 frames.
    texpack::ResidencyLimits Limits(ID3D12Device* device);
    // Takes replacements out of video memory. Their textures go to the frame's garbage, which the GPU is done with before it is
    // freed, and the game's textures that drew them ask for them again when they next draw.
    void Evict(const std::vector<uint64_t>& hashes);
    void Retire();

    texpack::Loader loader_;
    std::shared_ptr<const texpack::PackSource> source_;
    uint64_t pack_generation_ = ~0ull;  // texpack::Generation() as last seen; the first call always reads the source
    uint64_t generation_ = 1;           // bumped when the packs change, so that every state becomes stale
    uint64_t presents_ = 0, pumped_ = ~0ull;
    std::chrono::steady_clock::time_point last_begin_{};  // when the last frame began, to tell a long stop from a slow frame
    uint64_t lookups_ = 0, matches_ = 0, hashed_bytes_ = 0;
    std::unordered_set<uint64_t> distinct_;
    std::unordered_map<uint64_t, std::unique_ptr<HdImage>> images_;  // by replacement hash
    std::vector<texpack::Loaded> waiting_;                           // read by the loader, not yet copied to the GPU
    std::unordered_map<uint64_t, uint64_t> wanted_;                  // hash to the last frame a draw waited for it
    std::unordered_set<uint64_t> bad_;                               // hashes whose replacement cannot be used
    std::unordered_set<uint64_t> evicted_;                           // hashes taken out of video memory, to tell a reload from a first load
    std::unordered_map<uint64_t, uint32_t> read_failures_, memory_failures_;
    texpack::Residency residency_;
    texpack::ResidencyLimits limits_;
    uint64_t limits_frame_ = 0;       // presents_ plus one when `limits_` was last worked out
    uint64_t limits_logged_ = 0;      // the budget the log last named
    Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter_;  // for the driver's video memory budget; null when it cannot be had
    bool adapter_tried_ = false;
    uint64_t dedicated_bytes_ = 0;
    uint64_t evictions_ = 0, reloads_ = 0, no_room_count_ = 0;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> retired_;    // images of a previous set of packs, maybe still in use by the GPU
    std::vector<std::unique_ptr<TextureHeap>> heaps_;
    std::vector<DeferredFree> deferred_;  // placements to give back once the GPU is done with the textures that held them
    std::vector<UploadSlot> uploads_;     // one buffer for the copies of each of the last few frames
    std::future<Microsoft::WRL::ComPtr<ID3D12Heap>> spare_heap_;  // the next heap, being made apart from the frames
    bool warmed_ = false;
    uint64_t resident_bytes_ = 0;
    uint64_t sent_ = 0, sent_bytes_ = 0;
    double stage_ms_[5] = {};  // render thread time of the sends: footprints, texture, upload buffer, rows, record
};
}
