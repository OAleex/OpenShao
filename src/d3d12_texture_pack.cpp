#include "d3d12_texture_pack.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>

#include "core/log.h"
#include "gpu/shaders.h"
#include "texture_budget.h"
#include "texture_pack.h"
#include "texture_pack_match.h"
#include "texture_pack_stats.h"

namespace openshao::graphics {
namespace {
constexpr uint64_t kReportEvery = 300;   // presented frames between log lines
constexpr uint32_t kLoggedMatches = 40;  // distinct replacements named in the log
constexpr uint32_t kLoggedSends = 40;    // replacements sent that the log names
constexpr uint32_t kLoggedUses = 400;    // first requests and first draws that the log names
constexpr uint64_t kBytesPerFrame = 16ull << 20;  // copied to the GPU in one frame, at most
constexpr auto kTimePerFrame = std::chrono::microseconds(2000);
constexpr uint64_t kAskAgainEvery = 30;  // frames; a request is dropped after 240 not asked for again
constexpr uint32_t kFailuresAllowed = 3;
constexpr uint64_t kWantedFrames = 120;  // a texture drawn this recently counts as wanted
constexpr uint64_t kMaxWaitingBytes = 256ull << 20;  // read and not yet copied to the GPU, at most
constexpr uint64_t kLimitsEvery = 60;    // frames between workings out of the video memory budget
constexpr uint32_t kNoLimitOnImages = UINT32_MAX;  // a heap holds any number of textures
constexpr uint32_t kLoggedEvictions = 300;
constexpr uint32_t kLoggedNoRoom = 50;
constexpr uint64_t kHeapBytes = 64ull << 20;       // a texture heap, unless one texture needs more
constexpr uint64_t kHeapGranularity = 64ull << 10;  // a heap's size is a multiple of this
// The renderer keeps three frames in flight; a placement or a staging range is reused after more than that.
constexpr uint64_t kFramesInFlight = 3;
constexpr uint64_t kFreeAfterFrames = kFramesInFlight + 2;
constexpr size_t kUploadSlots = 4;
constexpr uint64_t kUploadSlotBytes = 16ull << 20;  // as much as a frame is allowed to copy
constexpr uint64_t kStagingAlignment = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;

DXGI_FORMAT HostFormat(texpack::BcFormat format) {
    switch (format) {
        case texpack::BcFormat::Bc1: return DXGI_FORMAT_BC1_UNORM;
        case texpack::BcFormat::Rgba8: return DXGI_FORMAT_R8G8B8A8_UNORM;
        default: return DXGI_FORMAT_BC3_UNORM;
    }
}

D3D12_RESOURCE_DESC BufferDescription(uint64_t size) {
    D3D12_RESOURCE_DESC d{};
    d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Width = size;
    d.Height = 1;
    d.DepthOrArraySize = 1;
    d.MipLevels = 1;
    d.SampleDesc.Count = 1;
    d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return d;
}

uint64_t RoundUp(uint64_t value, uint64_t step) { return (value + step - 1) / step * step; }

// Safe to call from any thread: the device creates resources without the renderer's lock.
Microsoft::WRL::ComPtr<ID3D12Heap> MakeHeap(ID3D12Device* device, uint64_t size) {
    D3D12_HEAP_DESC desc{};
    desc.SizeInBytes = size;
    desc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    desc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
    Microsoft::WRL::ComPtr<ID3D12Heap> heap;
    if (FAILED(device->CreateHeap(&desc, IID_PPV_ARGS(&heap)))) heap.Reset();
    return heap;
}
}

void D3D12HdTextures::Retire() {
    for (auto& entry : images_) {
        const HdImage& image = *entry.second;
        // The GPU may still be reading the texture, so its place in the heap is given back later.
        deferred_.push_back({image.heap, image.offset, image.allocated, presents_ + kFreeAfterFrames});
        retired_.push_back(std::move(entry.second->resource));
    }
    images_.clear();
    residency_.Clear();
    waiting_.clear();
    wanted_.clear();
    bad_.clear();
    evicted_.clear();
    read_failures_.clear();
    memory_failures_.clear();
    resident_bytes_ = 0;
    limits_frame_ = 0;
}

void D3D12HdTextures::SyncSource() {
    const uint64_t pack_generation = texpack::Generation();
    if (pack_generation == pack_generation_) return;
    pack_generation_ = pack_generation;
    source_ = texpack::Source();
    loader_.SetPack(source_);
    ++generation_;
    Retire();
    distinct_.clear();
    lookups_ = matches_ = hashed_bytes_ = sent_ = sent_bytes_ = 0;
    if (!source_) {
        LOGI("Texture pack (Direct3D 12): no pack is active");
        return;
    }
    LOGI("Texture pack (Direct3D 12): %zu replacements available", source_->Count());
    // The menus' textures are read now, apart from the frames, below any texture that is being drawn.
    const auto ahead = source_->Preload();
    for (uint64_t hash : ahead) loader_.Request(hash, -1.f);
    if (!ahead.empty()) LOGI("Texture pack (Direct3D 12): %zu textures to have ready for the menus", ahead.size());
}

void D3D12HdTextures::BeginFrame(gpu::Context& context, uint64_t presents) {
    presents_ = presents;
    SyncSource();
    if (pumped_ == presents) return;
    pumped_ = presents;
    // The game can go for seconds without drawing or presenting (while it loads); nothing reaches the GPU in that time.
    const auto now = std::chrono::steady_clock::now();
    if (source_ && last_begin_ != std::chrono::steady_clock::time_point{}) {
        const auto gap = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_begin_).count();
        static std::atomic<uint32_t> gaps{0};
        if (gap >= 300 && gaps.fetch_add(1) < 60)
            LOGI("Texture pack (Direct3D 12): no frame for %lld ms before frame %llu", static_cast<long long>(gap),
                 static_cast<unsigned long long>(presents));
    }
    last_begin_ = now;
    if (source_ && !warmed_) {
        warmed_ = true;
        Warm(context.Device());
    }
    static std::atomic<uint32_t> begun{0};
    if (source_ && begun.fetch_add(1) < 16)
        LOGI("Texture pack (Direct3D 12): frame %llu begins", static_cast<unsigned long long>(presents));
    Pump(context);
}

void D3D12HdTextures::Lookup(HdTexState& state, const gpu::TextureDesc& desc, const gpu::FormatInfo& format,
                             const uint8_t* guest, bool rewritten) {
    const std::optional<texpack::SourceEntry> before = state.rep;
    const uint64_t before_hash = state.rep_hash;
    state.generation = generation_;
    state.rep.reset();
    state.rep_hash = 0;
    state.shown = false;
    state.requested = state.first_request = state.waited_frames = state.last_wait = 0;
    if (before && (!source_ || !guest || !texpack::CanReplace(desc, format))) {
        static std::atomic<uint32_t> lost{0};
        if (lost.fetch_add(1) < kLoggedMatches)
            LOGI("Texture pack (Direct3D 12): %016llx is no longer replaceable, at frame %llu",
                 static_cast<unsigned long long>(before_hash), static_cast<unsigned long long>(presents_));
    }
    if (!source_ || !guest || !texpack::CanReplace(desc, format)) return;
    const uint32_t bytes = gpu::LevelSize(desc, 0);
    const uint64_t hash = gpu::Hash(guest, bytes);
    texpack::NoteHash(bytes);
    ++lookups_;
    hashed_bytes_ += bytes;
    if (bad_.count(hash)) return;
    const auto entry = texpack::Match(*source_, desc, hash);
    if (!entry) return;
    state.rep = entry;
    state.rep_hash = hash;
    ++matches_;
    const bool changed = !before || before_hash != hash;
    if (before && before_hash != hash) {
        static std::atomic<uint32_t> swapped{0};
        if (swapped.fetch_add(1) < kLoggedMatches)
            LOGI("Texture pack (Direct3D 12): the memory of %016llx now holds %016llx, at frame %llu",
                 static_cast<unsigned long long>(before_hash), static_cast<unsigned long long>(hash),
                 static_cast<unsigned long long>(presents_));
    }
    if (changed && distinct_.insert(hash).second) {
        static std::atomic<uint32_t> logged{0};
        if (logged.fetch_add(1) < kLoggedMatches)
            LOGI("Texture pack (Direct3D 12): replacement for %016llx, game %ux%u, pack %ux%u, %u levels%s",
                 static_cast<unsigned long long>(hash), desc.width, desc.height, entry->width, entry->height,
                 unsigned(entry->levels), rewritten ? ", memory rewritten" : "");
    }
}

// Places a texture in a heap with room for it, in a new heap when none has. `placed` is `desc` with the alignment the
// placement uses: a small texture can sit at 4 KiB, anything else at 64 KiB.
bool D3D12HdTextures::Place(ID3D12Device* device, const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_DESC& placed, Placement& out) {
    placed = desc;
    placed.Alignment = D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT;
    D3D12_RESOURCE_ALLOCATION_INFO info = device->GetResourceAllocationInfo(0, 1, &placed);
    if (info.SizeInBytes == 0 || info.SizeInBytes == UINT64_MAX) {  // too big for the small alignment
        placed.Alignment = 0;
        info = device->GetResourceAllocationInfo(0, 1, &placed);
    }
    if (info.SizeInBytes == 0 || info.SizeInBytes == UINT64_MAX) return false;

    size_t index = heaps_.size();
    std::optional<uint64_t> offset;
    for (size_t i = 0; i < heaps_.size() && !offset; i++) {
        offset = heaps_[i]->ranges.Allocate(info.SizeInBytes, info.Alignment);
        if (offset) index = i;
    }
    if (!offset) {
        index = AddHeap(device, std::max(kHeapBytes, RoundUp(info.SizeInBytes, kHeapGranularity)));
        if (index == heaps_.size()) return false;
        offset = heaps_[index]->ranges.Allocate(info.SizeInBytes, info.Alignment);
        if (!offset) return false;
    }
    if (FAILED(device->CreatePlacedResource(heaps_[index]->heap.Get(), *offset, &placed, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                            IID_PPV_ARGS(&out.resource)))) {
        heaps_[index]->ranges.Free(*offset, info.SizeInBytes);
        return false;
    }
    out.heap = index;
    out.offset = *offset;
    out.size = info.SizeInBytes;
    return true;
}

size_t D3D12HdTextures::AddHeap(ID3D12Device* device, uint64_t size) {
    Microsoft::WRL::ComPtr<ID3D12Heap> heap;
    // The heap made ahead of time is taken even when it is not finished: waiting for it costs less than making another.
    if (size == kHeapBytes && spare_heap_.valid()) heap = spare_heap_.get();
    if (!heap) heap = MakeHeap(device, size);
    if (!heap) return heaps_.size();
    auto entry = std::make_unique<TextureHeap>(size);
    entry->heap = std::move(heap);
    heaps_.push_back(std::move(entry));
    LOGI("Texture pack (Direct3D 12): texture heap %zu of %.0f MiB", heaps_.size() - 1, double(size) / 1048576.0);
    return heaps_.size() - 1;
}

void D3D12HdTextures::PrefetchHeap(ID3D12Device* device) {
    if (heaps_.empty() || spare_heap_.valid() || heaps_.back()->ranges.free_bytes() >= kHeapBytes / 2) return;
    // A heap made ahead is memory reserved before it is needed: not past what the budget allows plus one heap.
    uint64_t reserved = 0;
    for (const auto& heap : heaps_) reserved += heap->ranges.capacity();
    if (limits_.max_bytes && reserved >= limits_.max_bytes + kHeapBytes) return;
    Microsoft::WRL::ComPtr<ID3D12Device> owner(device);
    spare_heap_ = std::async(std::launch::async, [owner] { return MakeHeap(owner.Get(), kHeapBytes); });
}

// How much video memory the replacements may take. With the driver's budget (which already leaves out what other programs use)
// it is 60% of it less what this process uses apart from the replacements; without it, 40% of the adapter's own memory.
// OPENSHAO_HD_VRAM_MB forces the bytes. Worked out again every 60 frames, so a program that opens in the middle of a game, a
// stream for one, makes the budget fall and the replacements the game leaves unused are taken out.
texpack::ResidencyLimits D3D12HdTextures::Limits(ID3D12Device* device) {
    if (limits_frame_ && presents_ + 1 - limits_frame_ < kLimitsEvery) return limits_;
    limits_frame_ = presents_ + 1;
    if (!adapter_tried_) {
        adapter_tried_ = true;
        Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
        if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) &&
            SUCCEEDED(factory->EnumAdapterByLuid(device->GetAdapterLuid(), IID_PPV_ARGS(&adapter_)))) {
            DXGI_ADAPTER_DESC desc{};
            if (SUCCEEDED(adapter_->GetDesc(&desc))) dedicated_bytes_ = desc.DedicatedVideoMemory;
        }
    }
    static const uint64_t forced_mb = [] {
        const char* value = std::getenv("OPENSHAO_HD_VRAM_MB");
        return value ? std::strtoull(value, nullptr, 10) : 0ull;
    }();
    texpack::BudgetInputs inputs;
    inputs.forced_mb = forced_mb;
    inputs.dedicated_bytes = dedicated_bytes_;
    DXGI_QUERY_VIDEO_MEMORY_INFO info{};
    if (adapter_ && SUCCEEDED(adapter_->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info))) {
        inputs.driver_budget = info.Budget;
        inputs.driver_usage = info.CurrentUsage;
    }
    for (const auto& heap : heaps_) inputs.own_usage += heap->ranges.capacity();  // what the replacements reserve
    const texpack::Budget budget = texpack::ComputeBudget(inputs);
    limits_ = {budget.bytes, kNoLimitOnImages};
    texpack::SetBudget(budget.bytes);
    if (!limits_logged_ || budget.bytes > limits_logged_ + limits_logged_ / 4 || budget.bytes + budget.bytes / 4 < limits_logged_) {
        limits_logged_ = budget.bytes ? budget.bytes : 1;
        LOGI("Texture pack (Direct3D 12): video memory for the replacements: %.0f MiB (%s); the adapter has %.0f MiB, "
             "the driver's budget is %.0f MiB",
             double(budget.bytes) / 1048576.0, texpack::DescribeBudget(budget.source), double(dedicated_bytes_) / 1048576.0,
             double(info.Budget) / 1048576.0);
    }
    return limits_;
}

void D3D12HdTextures::Evict(const std::vector<uint64_t>& hashes) {
    if (hashes.empty()) return;
    const uint64_t before = residency_.bytes();
    size_t gone = 0;
    for (uint64_t hash : hashes) {
        const auto found = images_.find(hash);
        if (found == images_.end()) continue;
        HdImage& image = *found->second;
        deferred_.push_back({image.heap, image.offset, image.allocated, presents_ + kFreeAfterFrames});
        resident_bytes_ -= std::min(resident_bytes_, image.allocated);
        retired_.push_back(std::move(image.resource));
        images_.erase(found);
        residency_.Remove(hash);
        evicted_.insert(hash);
        ++gone;
    }
    if (!gone) return;
    evictions_ += gone;
    texpack::NoteEviction(gone, before - residency_.bytes());
    static std::atomic<uint32_t> logged{0};
    if (logged.fetch_add(1) < kLoggedEvictions)
        LOGI("Texture pack (Direct3D 12): took %zu replacements out of video memory (%.1f MiB); %zu left, %.1f MiB of %.1f MiB, "
             "at frame %llu",
             gone, double(before - residency_.bytes()) / 1048576.0, residency_.count(),
             double(residency_.bytes()) / 1048576.0, double(limits_.max_bytes) / 1048576.0,
             static_cast<unsigned long long>(presents_));
}

bool D3D12HdTextures::EnsureSlot(ID3D12Device* device, UploadSlot& slot) {
    if (slot.buffer) return true;
    D3D12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_UPLOAD};
    const D3D12_RESOURCE_DESC desc = BufferDescription(kUploadSlotBytes);
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                               IID_PPV_ARGS(&slot.buffer))))
        return false;
    if (FAILED(slot.buffer->Map(0, nullptr, reinterpret_cast<void**>(&slot.mapped)))) {
        slot.buffer.Reset();
        return false;
    }
    return true;
}

void D3D12HdTextures::Warm(ID3D12Device* device) {
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();
    uploads_.resize(kUploadSlots);
    for (auto& slot : uploads_) EnsureSlot(device, slot);
    if (heaps_.empty()) AddHeap(device, kHeapBytes);
    LOGI("Texture pack (Direct3D 12): %zu upload buffers and a texture heap made ahead in %.1f ms", uploads_.size(),
         double(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count()) / 1000.0);
}

// Room for the rows of a texture in the upload buffer of the current frame. A texture that does not fit what is left of it
// gets a buffer of its own.
bool D3D12HdTextures::Stage(ID3D12Device* device, uint64_t size, Staging& out) {
    if (uploads_.empty()) uploads_.resize(kUploadSlots);
    UploadSlot& slot = uploads_[presents_ % kUploadSlots];
    if (slot.frame != presents_) {
        slot.frame = presents_;
        slot.used = 0;
    }
    D3D12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_UPLOAD};
    const uint64_t offset = RoundUp(slot.used, kStagingAlignment);
    if (offset + size <= kUploadSlotBytes) {
        if (!EnsureSlot(device, slot)) return false;
        slot.used = offset + size;
        out.resource = slot.buffer.Get();
        out.offset = offset;
        out.mapped = slot.mapped + offset;
        return true;
    }
    const D3D12_RESOURCE_DESC desc = BufferDescription(size);
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                               IID_PPV_ARGS(&out.dedicated))))
        return false;
    if (FAILED(out.dedicated->Map(0, nullptr, reinterpret_cast<void**>(&out.mapped)))) return false;
    out.resource = out.dedicated.Get();
    out.offset = 0;
    return true;
}

// Makes a texture of the levels the loader read, placed in a heap, and records their copy through the upload buffer. The
// texture ends in the pixel shader state.
D3D12HdTextures::Sent D3D12HdTextures::Send(gpu::Context& context, const texpack::Loaded& loaded) {
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();
    const texpack::SourceEntry& entry = loaded.entry;
    const uint32_t first = loaded.first_level;
    if (first >= entry.levels) return Sent::Data;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = std::max(1u, uint32_t(entry.width) >> first);
    desc.Height = std::max(1u, uint32_t(entry.height) >> first);
    desc.DepthOrArraySize = 1;
    desc.MipLevels = UINT16(entry.levels - first);
    desc.Format = HostFormat(entry.format);
    desc.SampleDesc.Count = 1;

    ID3D12Device* device = context.Device();
    const UINT levels = desc.MipLevels;
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(levels);
    std::vector<UINT> rows(levels);
    std::vector<UINT64> row_sizes(levels);
    UINT64 total = 0;
    device->GetCopyableFootprints(&desc, 0, levels, 0, footprints.data(), rows.data(), row_sizes.data(), &total);

    // The blocks must hold every level before anything is created or recorded.
    uint64_t needed = 0;
    for (UINT l = 0; l < levels; l++) needed += row_sizes[l] * rows[l];
    if (needed > loaded.blocks.size()) return Sent::Data;
    const auto t_footprints = Clock::now();

    Placement placement;
    D3D12_RESOURCE_DESC placed{};
    if (!Place(device, desc, placed, placement)) return Sent::Memory;
    const auto t_texture = Clock::now();
    Staging staging;
    if (!Stage(device, total, staging)) {
        // Nothing was recorded, so the place is free at once.
        heaps_[placement.heap]->ranges.Free(placement.offset, placement.size);
        return Sent::Memory;
    }
    const auto t_buffer = Clock::now();

    // Rows are tightly packed in the pack and padded to the row pitch in the upload buffer.
    size_t offset = 0;
    for (UINT l = 0; l < levels; l++) {
        for (UINT r = 0; r < rows[l]; r++) {
            std::memcpy(staging.mapped + footprints[l].Offset + size_t(r) * footprints[l].Footprint.RowPitch,
                        loaded.blocks.data() + offset + size_t(r) * row_sizes[l], size_t(row_sizes[l]));
        }
        offset += size_t(row_sizes[l]) * rows[l];
    }
    const auto t_rows = Clock::now();

    ID3D12GraphicsCommandList* list = context.List();
    for (UINT l = 0; l < levels; l++) {
        D3D12_TEXTURE_COPY_LOCATION destination{placement.resource.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
        destination.SubresourceIndex = l;
        D3D12_TEXTURE_COPY_LOCATION source{staging.resource, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
        source.PlacedFootprint = footprints[l];
        source.PlacedFootprint.Offset += staging.offset;
        list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    }
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COPY_DEST;
    context.Transition(placement.resource.Get(), state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    if (staging.dedicated) context.Release(std::move(staging.dedicated));

    auto image = std::make_unique<HdImage>();
    image->resource = std::move(placement.resource);
    image->format = desc.Format;
    image->levels = levels;
    image->bytes = loaded.blocks.size();
    image->heap = placement.heap;
    image->offset = placement.offset;
    image->allocated = placement.size;
    image->counted = std::make_unique<texpack::Resident>(loaded.blocks.size());
    images_[loaded.hash] = std::move(image);
    resident_bytes_ += placement.size;
    ++sent_;
    sent_bytes_ += loaded.blocks.size();
    const auto t_end = Clock::now();
    const uint64_t upload_us = uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(t_end - start).count());
    texpack::NoteLoad(loaded.read_us, upload_us, true, false);
    texpack::NoteWait(loaded.wait_us);
    const auto ms = [](Clock::time_point from, Clock::time_point to) {
        return double(std::chrono::duration_cast<std::chrono::microseconds>(to - from).count()) / 1000.0;
    };
    stage_ms_[0] += ms(start, t_footprints);
    stage_ms_[1] += ms(t_footprints, t_texture);
    stage_ms_[2] += ms(t_texture, t_buffer);
    stage_ms_[3] += ms(t_buffer, t_rows);
    stage_ms_[4] += ms(t_rows, t_end);
    static std::atomic<uint32_t> logged{0};
    if (logged.fetch_add(1) < kLoggedSends || upload_us >= 5000)
        LOGI("Texture pack (Direct3D 12): sent %016llx %llux%u, %u levels, %.2f MiB: %.2f ms on the render thread "
             "(footprints %.2f, texture %.2f, staging %.2f, rows %.2f, record %.2f), read %.1f ms, waited %.1f ms",
             static_cast<unsigned long long>(loaded.hash), static_cast<unsigned long long>(desc.Width), desc.Height, levels,
             double(loaded.blocks.size()) / 1048576.0, double(upload_us) / 1000.0, ms(start, t_footprints),
             ms(t_footprints, t_texture), ms(t_texture, t_buffer), ms(t_buffer, t_rows), ms(t_rows, t_end),
             double(loaded.read_us) / 1000.0, double(loaded.wait_us) / 1000.0);
    return Sent::Ok;
}

// Once a frame, the replacements that have been read go to the GPU, as many as the frame's budget allows. The rest wait.
void D3D12HdTextures::Pump(gpu::Context& context) {
    using Clock = std::chrono::steady_clock;
    for (auto& image : retired_) context.Release(std::move(image));
    retired_.clear();
    for (size_t i = 0; i < deferred_.size();) {
        if (deferred_[i].due <= presents_) {
            heaps_[deferred_[i].heap]->ranges.Free(deferred_[i].offset, deferred_[i].size);
            deferred_.erase(deferred_.begin() + i);
        } else {
            ++i;
        }
    }
    PrefetchHeap(context.Device());
    const texpack::ResidencyLimits limits = Limits(context.Device());
    // The budget fell, or an earlier frame left the replacements over it.
    Evict(residency_.Plan(presents_, limits).drop);
    loader_.TakeReady(waiting_);
    if (waiting_.empty()) return;
    // What a draw is waiting for goes first. The menus' preload, which nothing draws yet, would otherwise hold up a texture
    // that is on screen: a frame sends only as many as its time allows, and a game that shows one picture and then loads for
    // seconds presents just a few frames.
    std::stable_partition(waiting_.begin(), waiting_.end(), [&](const texpack::Loaded& loaded) {
        const auto asked = wanted_.find(loaded.hash);
        return asked != wanted_.end() && presents_ - asked->second <= kWantedFrames;
    });
    uint64_t held = 0;
    for (const auto& loaded : waiting_) held += loaded.blocks.size();
    while (held > kMaxWaitingBytes && !waiting_.empty()) {  // the least wanted go; a texture that is drawn asks again
        held -= waiting_.back().blocks.size();
        loader_.Recycle(std::move(waiting_.back().blocks));
        waiting_.pop_back();
    }
    const auto start = Clock::now();
    uint64_t bytes = 0;
    size_t used = 0;
    for (; used < waiting_.size() && bytes < kBytesPerFrame && Clock::now() - start < kTimePerFrame; ++used) {
        texpack::Loaded& r = waiting_[used];
        if (!r.ok) {
            // The texture asks again after a while, so a read that failed once gets another try.
            if (++read_failures_[r.hash] >= kFailuresAllowed) bad_.insert(r.hash);
            texpack::NoteLoad(r.read_us, 0, false, false);
            continue;
        }
        read_failures_.erase(r.hash);
        if (images_.count(r.hash)) {
            loader_.Recycle(std::move(r.blocks));
            continue;
        }
        // Makes room for it. When nothing can be taken out it stays the game's own picture and asks again later.
        const texpack::ResidencyPlan plan = residency_.Plan(presents_, limits, r.blocks.size(), 1);
        Evict(plan.drop);
        if (!plan.fits) {
            texpack::NoteNoRoom();
            ++no_room_count_;
            static std::atomic<uint32_t> logged{0};
            if (logged.fetch_add(1) < kLoggedNoRoom)
                LOGI("Texture pack (Direct3D 12): no room for %016llx (%zu bytes): %.1f MiB of %.1f MiB in use and all of it drawn lately",
                     static_cast<unsigned long long>(r.hash), r.blocks.size(), double(residency_.bytes()) / 1048576.0,
                     double(limits.max_bytes) / 1048576.0);
            loader_.Recycle(std::move(r.blocks));
            continue;
        }
        bytes += r.blocks.size();
        switch (Send(context, r)) {
            case Sent::Ok:
                residency_.Add(r.hash, r.blocks.size(), presents_);
                memory_failures_.erase(r.hash);
                PrefetchHeap(context.Device());  // a burst of sends can fill a heap within one frame
                if (evicted_.erase(r.hash)) {
                    texpack::NoteReload();
                    ++reloads_;
                }
                break;
            case Sent::Data: bad_.insert(r.hash); break;
            case Sent::Memory: {
                // The device had no memory for it just now, which a replacement that is out of use can give back: a quarter
                // goes, and the texture asks again; only after three tries is it given up.
                texpack::NoteMemoryFailure();
                if (++memory_failures_[r.hash] >= kFailuresAllowed) bad_.insert(r.hash);
                const uint64_t held = residency_.bytes();
                Evict(residency_.Plan(presents_, {held / 4 * 3, kNoLimitOnImages}).drop);
                LOGI("Texture pack (Direct3D 12): no memory for %016llx (try %u); %.1f MiB in use",
                     static_cast<unsigned long long>(r.hash), unsigned(memory_failures_[r.hash]), double(held) / 1048576.0);
                break;
            }
        }
        loader_.Recycle(std::move(r.blocks));
    }
    waiting_.erase(waiting_.begin(), waiting_.begin() + used);
}

const HdImage* D3D12HdTextures::Use(gpu::Context&, HdTexState& state) {
    if (!state.rep) return nullptr;
    const auto found = images_.find(state.rep_hash);
    if (found != images_.end()) {
        residency_.Touch(state.rep_hash, presents_);  // drawn now: the last to be taken out of video memory
        if (!state.shown) {
            state.shown = true;
            if (!state.first_request) texpack::NoteReadyAtFirstUse();
            else texpack::NotePopIn(state.waited_frames);
            static std::atomic<uint32_t> drawn{0};
            if (drawn.fetch_add(1) < kLoggedUses)
                LOGI("Texture pack (Direct3D 12): drawing %016llx at frame %llu, %llu frames as the game's own before it",
                     static_cast<unsigned long long>(state.rep_hash), static_cast<unsigned long long>(presents_),
                     static_cast<unsigned long long>(state.first_request ? state.waited_frames : 0));
        }
        return found->second.get();
    }
    if (state.shown) {  // taken out of video memory: asked for again as the first time
        state.shown = false;
        state.requested = state.first_request = state.waited_frames = state.last_wait = 0;
    }
    if (bad_.count(state.rep_hash)) {
        state.rep.reset();
        return nullptr;
    }
    wanted_[state.rep_hash] = presents_;
    // A texture not drawn for a second has not been waiting for its replacement in that time.
    const uint64_t frame = presents_ + 1;
    if (state.last_wait && frame - state.last_wait > 60) state.first_request = state.waited_frames = 0;
    if (state.last_wait != frame) ++state.waited_frames;
    state.last_wait = frame;
    if (!state.requested || frame - state.requested >= kAskAgainEvery) {
        if (!state.requested) {
            static std::atomic<uint32_t> asked{0};
            if (asked.fetch_add(1) < kLoggedUses)
                LOGI("Texture pack (Direct3D 12): asking for %016llx at frame %llu",
                     static_cast<unsigned long long>(state.rep_hash), static_cast<unsigned long long>(presents_));
        }
        loader_.Request(state.rep_hash);
        state.requested = frame;
        if (!state.first_request) state.first_request = frame;
    }
    return nullptr;
}

// What the texture pack costs, every five seconds, and where the player marked the scenes (Ctrl+F9). The first line has the
// figures of the Vulkan renderer's log, so the two can be read side by side.
void D3D12HdTextures::EndFrame(uint64_t presents) {
    loader_.AdvanceFrame();
    texpack::EndFrame();
    texpack::Interval span;
    if (texpack::TakeInterval(kReportEvery, span) && (span.resident || span.loads || span.failures)) {
        const double loads = double(std::max<uint64_t>(span.loads, 1));
        LOGI("Texture pack (Direct3D 12): %llu frames: %llu loads, %llu failed, read %.2f ms and upload %.2f ms per load, worst frame "
             "%.2f ms of loading, waited %.1f ms, pop-in %llu textures %.1f frames on average and %llu at most in this stretch, %llu "
             "ready at their first draw, %llu taken out (%.1f MiB), %llu back, %llu no room, budget %.0f MiB, %llu hashed (%.1f MiB), "
             "%u textures %.0f MiB",
             static_cast<unsigned long long>(span.frames), static_cast<unsigned long long>(span.loads),
             static_cast<unsigned long long>(span.failures), double(span.disk_us) / 1000.0 / loads,
             double(span.upload_us) / 1000.0 / loads, double(span.worst_frame_us) / 1000.0,
             double(span.wait_us) / 1000.0 / double(std::max<uint64_t>(span.waits, 1)),
             static_cast<unsigned long long>(span.pop_ins),
             double(span.pop_frames) / double(std::max<uint64_t>(span.pop_ins, 1)),
             static_cast<unsigned long long>(span.pop_max), static_cast<unsigned long long>(span.ready),
             static_cast<unsigned long long>(span.evictions), double(span.evicted_bytes) / 1048576.0,
             static_cast<unsigned long long>(span.reloads), static_cast<unsigned long long>(span.no_room),
             double(span.budget_bytes) / 1048576.0, static_cast<unsigned long long>(span.hashes),
             double(span.hashed_bytes) / 1048576.0, span.resident, double(span.resident_bytes) / 1048576.0);
    }
    uint32_t mark = 0;
    if (texpack::TakeMarker(mark)) {
        const auto stats = texpack::GetStats();
        LOGI("Texture pack (Direct3D 12): marker %u: %u textures %.0f MiB, %llu loads in all, %llu taken out, %llu back, at frame %llu",
             mark, stats.resident, double(stats.resident_bytes) / 1048576.0, static_cast<unsigned long long>(stats.loads),
             static_cast<unsigned long long>(stats.evictions), static_cast<unsigned long long>(stats.reloads),
             static_cast<unsigned long long>(presents));
    }
    if (!source_ || presents % kReportEvery != 0) return;
    const texpack::LoaderStats stats = loader_.GetStats();
    LOGI("Texture pack (Direct3D 12): %llu lookups, %llu with a replacement (%zu distinct), %.1f MiB hashed; %llu sent, %.1f MiB of "
         "data, %.1f MiB placed in %zu heaps, %zu resident; sends took %.1f ms on the render thread (footprints %.1f, texture %.1f, "
         "staging %.1f, rows %.1f, record %.1f); loader: %u pending, %u ready, %llu failed; at frame %llu",
         static_cast<unsigned long long>(lookups_), static_cast<unsigned long long>(matches_), distinct_.size(),
         double(hashed_bytes_) / 1048576.0, static_cast<unsigned long long>(sent_), double(sent_bytes_) / 1048576.0,
         double(resident_bytes_) / 1048576.0, heaps_.size(), images_.size(),
         stage_ms_[0] + stage_ms_[1] + stage_ms_[2] + stage_ms_[3] + stage_ms_[4], stage_ms_[0], stage_ms_[1], stage_ms_[2],
         stage_ms_[3], stage_ms_[4], stats.pending, stats.ready, static_cast<unsigned long long>(stats.failed),
         static_cast<unsigned long long>(presents));
}
}
