#include <array>
#include <bit>
#include <cmath>
#include <mutex>

namespace {
// The original scene constructs one render group per source identity. Replace
// each binding on reconstruction, and erase reused owners for unknown content.
std::array<uint32_t, openshao::BackgroundStripSourceCount> background_owners{};
std::mutex background_owners_mutex;

uint32_t SourceObjectFlags(uint32_t chunk, unsigned& background_source) {
    if (!chunk || kernel::R32(chunk + 8) != 6) return 0;
    const uint32_t count = kernel::R32(chunk + 16), vertices = kernel::R32(chunk + 64);
    if (!count || count >= 16384 || !vertices || vertices > 0xFFFFFFFFu - count * 16) return 0;
    uint32_t node = kernel::R32(chunk + 68);
    for (unsigned n = 0; node && n < 256; ++n) {
        if (kernel::R32(node + 4) == 16) {
            const uint32_t words = kernel::R32(node + 12), source = kernel::R32(node + 16);
            if (!source || !words || words >= 65536 || source > 0xFFFFFFFFu - words * 2) return 0;
            const std::span<const uint8_t> vertex_data{
                static_cast<const uint8_t*>(kernel::HostPtr(vertices)), count * 16};
            const std::span<const uint8_t> index_data{
                static_cast<const uint8_t*>(kernel::HostPtr(source)), words * 2};
            background_source = openshao::KnownBackgroundStripSourceId(vertex_data, index_data);
            return (openshao::KnownRoomStripSourceNeedsTwoSides(kernel::R32(0x505970), vertex_data, index_data) ? 0x80000000u : 0) |
                   (openshao::KnownRoomStripSourceNeedsNoDepthWrite(kernel::R32(0x505970),vertex_data,index_data) ? 1u : 0);
        }
        node = kernel::R32(node);
    }
    return 0;
}
}

X86_EXTERN_FUNC(__imp__sub_00133110);
X86_FUNC(sub_00133110) {
    unsigned background_source = 0;
    const uint32_t flags = SourceObjectFlags(kernel::R32(ctx.esp + 4), background_source);
    __imp__sub_00133110(ctx, base);
    if (ctx.eax && ctx.eax <= 0xFFFFFFEFu) {
        std::lock_guard lock(background_owners_mutex);
        const uint32_t owner = ctx.eax + 0x10;
        for (auto& bound : background_owners) if (bound == owner) bound = 0;
        if (background_source && background_source <= background_owners.size())
            background_owners[background_source - 1] = owner;
    }
    if ((flags & 0x80000000u) && ctx.eax) {
        // The original draw-unit renderer handles this existing object flag itself.
        kernel::W32(ctx.eax + 0x48, kernel::R32(ctx.eax + 0x48) | 0x80000000u);
        LOG_FIRST(8, "openshao graphics: applied source object's two-sided geometry metadata");
    }
    if ((flags & 1u) && ctx.eax) {
        // Preserve depth testing; the original renderer disables only writes for this object flag.
        kernel::W32(ctx.eax + 0x4C, kernel::R32(ctx.eax + 0x4C) | 1u);
        LOG_FIRST(16, "openshao graphics: restored transparent source object's depth metadata");
    }
}

X86_EXTERN_FUNC(__imp__sub_0017AA40);
X86_EXTERN_FUNC(sub_0017A6B0);
X86_FUNC(sub_0017AA40) {
    // Load phase 3 invokes the parser callback after decompression. Render-group
    // construction is too late: the parser has already translated material state.
    const uint32_t room = kernel::R32(0x505970);
    bool parsing = kernel::R32(0x695304) == 3;
    static thread_local bool pipeline_wait = false;
    static thread_local openshao::pipelines::LoadGate pipeline_gate;
    static thread_local uint32_t waiting_record = 0;
    const uint32_t load_record = kernel::R32(0x695310);
    if (pipeline_wait) {
        if (load_record == waiting_record && parsing && openshao::pipelines::PipelineLoadPending()) {
            // The parser already completed. Keep servicing the loading presentation.
            auto progress = ctx;
            sub_0017A6B0(progress, base);
            pipeline_gate.ReturnPending(ctx.esp);
            return;
        }
        openshao::pipelines::EndPipelineLoad();
        pipeline_wait = false;
        pipeline_gate.Reset();
        if (load_record == waiting_record && parsing) kernel::W32(0x695304, 4);
        parsing = kernel::R32(0x695304) == 3;
    }
    if (parsing && !pipeline_wait) openshao::pipelines::BeginPipelineLoad();
    // The room's recorded pipelines go to the front of the precompile queue while its assets load.
    openshao::pipelines::RoomLoading(room);
    if ((room == 0xA9 || room == 0xAA || room == 0xBB) && parsing) {
        const uint32_t record = kernel::R32(0x695310);
        if (record && record <= 0xFFFFFFEFu) {
            const uint32_t data = kernel::R32(record + 8), size = kernel::R32(record + 12);
            if (data && size && data <= 0xFFFFFFFFu - size) {
                const std::span<uint8_t> loaded{static_cast<uint8_t*>(kernel::HostPtr(data)), size};
                if (room == 0xA9 && openshao::RestoreKnownArenaSoulMaterials(room, loaded))
                    LOG_FIRST(4, "openshao graphics: restored original soul-layer state before scene parsing");
                if ((room == 0xAA || room == 0xBB) && openshao::RestoreKnownWastelandHouseOpacity(room, loaded))
                    LOG_FIRST(4, "openshao graphics: restored original Wastelands house opacity before texture parsing");
            }
        }
    }
    __imp__sub_0017AA40(ctx, base);
    if (parsing && kernel::R32(0x695304) == 4) {
        openshao::pipelines::SealPipelineLoad();
        if (openshao::pipelines::PipelineLoadPending()) {
            pipeline_wait = true;
            pipeline_gate.Parsed(load_record);
            waiting_record = load_record;
            kernel::W32(0x695304, 3);
        } else openshao::pipelines::EndPipelineLoad();
    }
}

X86_EXTERN_FUNC(__imp__sub_00120790);
X86_FUNC(sub_00120790) {
    // The original alpha-list Add takes (cPxAlphaUnit*, float distance).
    // Types1/2 contain plain/transformed draw units with a group owner at+158.
    bool background = false;
    if (kernel::R32(0x505970) == 0xA7) {
        const uint32_t alpha = kernel::R32(ctx.esp + 4);
        if (alpha) {
            const uint32_t type = kernel::R32(alpha), unit = kernel::R32(alpha + 4);
            if ((type == 1 || type == 2) && unit && unit <= 0xFFFFFEA7u) {
                const uint32_t owner = kernel::R32(unit + 0x158);
                std::lock_guard lock(background_owners_mutex);
                for (const auto bound : background_owners) background |= owner && owner == bound;
            }
        }
    }
    const uint32_t argument = ctx.esp + 8;
    const uint32_t previous = background ? kernel::R32(argument) : 0;
    const float distance = std::bit_cast<float>(previous);
    if (!background || !std::isfinite(distance) || distance < 0 || distance > 1.0e12f) {
        __imp__sub_00120790(ctx, base);
        return;
    }
    // A finite priority band above the supported scene's squared distances
    // retains its relative sky order without changing alpha, depth or geometry.
    const float priority = 1.0e12f + distance * 1.0e4f;
    kernel::W32(argument, std::bit_cast<uint32_t>(priority));
    LOG_FIRST(8, "openshao graphics: background source sort distance %.7g -> %.7g", distance, priority);
    __imp__sub_00120790(ctx, base);
    kernel::W32(argument, previous);
}

X86_EXTERN_FUNC(__imp__sub_00131070);
X86_FUNC(sub_00131070) {
    // Repair metadata before index-buffer sizing; the native count and write passes must share it.
    const uint32_t builder = ctx.ecx;
    const uint32_t strip = kernel::R32(ctx.esp + 8);
    const uint32_t chunk = kernel::R32(builder + 4);
    if (chunk && strip && kernel::R32(chunk + 8) == 6) {
        const uint32_t count = kernel::R32(chunk + 16), vertices = kernel::R32(chunk + 64);
        const uint32_t words = kernel::R32(strip + 12), source = kernel::R32(strip + 16);
        if (vertices && source && count && count < 16384 && words && words < 65536 &&
            vertices <= 0xFFFFFFFFu - count * 16 && source <= 0xFFFFFFFFu - words * 2) {
            if (openshao::RepairKnownRoomStripOrientation(kernel::R32(0x505970),
                    {static_cast<const uint8_t*>(kernel::HostPtr(vertices)), count * 16},
                    {static_cast<uint8_t*>(kernel::HostPtr(source)), words * 2}))
                LOG_FIRST(8, "openshao graphics: restored source strip orientation metadata (%u words)", words);
        }
    }
    __imp__sub_00131070(ctx, base);
}
