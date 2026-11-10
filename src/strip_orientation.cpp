#include "strip_orientation.h"
#include <algorithm>

namespace {
#include "strip_orientation_data.inc"
struct RoomStripRepair {
    uint32_t room;
    openshao::StripRepair repair;
};
#include "strip_orientation_room_data.inc"
#include "wasteland_house_opacity_data.inc"
// The extra arena exposes the reverse of these original cliff surfaces.
constexpr openshao::StripSource kWastelandCliffSources[] = {
    {0x5F539BCF36D08ED6ull, 3968, 381},
    {0x70AD88FD9386CD31ull, 6656, 546},
    {0xE30A4C944574BB12ull, 8672, 667},
    {0xF7D9D7CC6C124650ull, 5008, 386},
};
// Translucent light shafts must not leave opaque depth silhouettes over effects.
constexpr openshao::StripSource kOniArenaLightShaftSources[] = {
    {0xE385A42B48A7ECF7ull,448,42},
};
// Original A9 material words whose matching Tombs layers retain bit 0x80000.
constexpr uint32_t kArenaSoulMaterialOffsets[] = {
    73368, 73728, 123024, 124256, 125060, 125864, 126668,
    127472, 128892, 191008, 192620, 193424, 194228,
};
// Original VS8 cloud dome: keep its alpha fade, but submit it before scene effects.
constexpr openshao::StripSource kBackgroundStripSources[] = {
    {0x204C74A4D24063EBull, 576, 60},
    {0x28CB1F6D79A9856Full, 576, 60},
    {0x3EEC98E922C5C2A4ull, 576, 66},
    {0x7E9316C5E728A13Cull, 576, 60},
};
static_assert(std::size(kBackgroundStripSources) == openshao::BackgroundStripSourceCount);
uint64_t Signature(std::span<const uint8_t> vertices, std::span<const uint8_t> source) {
    uint64_t signature = 14695981039346656037ull;
    for (auto bytes : {vertices, source})
        for (uint8_t byte : bytes) signature = (signature ^ byte) * 1099511628211ull;
    return signature;
}
bool MatchesSource(std::span<const uint8_t> vertices, std::span<const uint8_t> source,
                   std::span<const openshao::StripSource> sources) {
    if (vertices.empty() || source.empty() || source.size() % 2) return false;
    const uint64_t signature = Signature(vertices, source);
    const auto match = std::lower_bound(sources.begin(), sources.end(), signature,
        [](const openshao::StripSource& entry, uint64_t value) { return entry.signature < value; });
    return match != sources.end() && match->signature == signature &&
           match->vertex_bytes == vertices.size() && match->source_words == source.size() / 2;
}
}

namespace openshao {
bool HasTwoSidedStripSource(std::span<const uint8_t> vertices, std::span<const uint8_t> source,
                           std::span<const StripSource> sources) {
    return MatchesSource(vertices, source, sources);
}
bool KnownStripSourceNeedsTwoSides(std::span<const uint8_t> vertices, std::span<const uint8_t> source) {
    return HasTwoSidedStripSource(vertices, source, kTwoSidedStripSources);
}
bool KnownRoomStripSourceNeedsTwoSides(uint32_t room, std::span<const uint8_t> vertices,
                                     std::span<const uint8_t> source) {
    return (room == 0xAA && MatchesSource(vertices, source, kWastelandCliffSources)) ||
           KnownStripSourceNeedsTwoSides(vertices, source);
}
bool KnownStripSourceNeedsNoDepthWrite(std::span<const uint8_t> vertices, std::span<const uint8_t> source) {
    return MatchesSource(vertices, source, kNoDepthWriteStripSources);
}
bool KnownRoomStripSourceNeedsNoDepthWrite(uint32_t room,std::span<const uint8_t> vertices,
                                         std::span<const uint8_t> source) {
    return (room==0xA3 && MatchesSource(vertices,source,kOniArenaLightShaftSources)) ||
           KnownStripSourceNeedsNoDepthWrite(vertices,source);
}
unsigned KnownBackgroundStripSourceId(std::span<const uint8_t> vertices, std::span<const uint8_t> source) {
    if (vertices.size() != 576 || (source.size() != 120 && source.size() != 132)) return 0;
    const uint64_t signature = Signature(vertices, source);
    for (unsigned i = 0; i < BackgroundStripSourceCount; ++i) {
        const auto& entry = kBackgroundStripSources[i];
        if (entry.signature == signature && entry.source_words == source.size() / 2) return i + 1;
    }
    return 0;
}
bool RestoreKnownArenaSoulMaterials(uint32_t room, std::span<uint8_t> scene) {
    // Bind the offsets to the complete original export before changing loaded bytes.
    if (room != 0xA9 || scene.size() != 1075420 ||
        Signature(scene, {}) != 0x9540B410076B07E5ull) return false;
    for (auto at : kArenaSoulMaterialOffsets)
        if (scene[at] || scene[at + 1] != 4 || scene[at + 2] || scene[at + 3]) return false;
    for (auto at : kArenaSoulMaterialOffsets) scene[at + 2] = 8;
    return true;
}
bool RestoreKnownWastelandHouseOpacity(uint32_t room, std::span<uint8_t> archive) {
    size_t offset = 0;
    if (room == 0xAA && archive.size() == 2746368 &&
        Signature(archive, {}) == 0x21C618FFC8ACC1B5ull) offset = 2254848;
    else if (room == 0xBB && archive.size() == 3166208 &&
             Signature(archive, {}) == 0x958183704587BBF9ull) offset = 2080768;
    else return false;
    auto texture = archive.subspan(offset, 16384);
    for (const auto& repair : kWastelandHouseOpacity) {
        if (!repair.mask || repair.block >= texture.size() / 16) return false;
        const size_t at = size_t(repair.block) * 16;
        if (texture[at] != 255 && texture[at + 1] != 255 && texture[at] > texture[at + 1]) return false;
    }
    for (const auto& repair : kWastelandHouseOpacity) {
        const size_t at = size_t(repair.block) * 16;
        // BC3 six-alpha palette: selector 7 is opaque when neither endpoint is.
        const uint64_t opaque = texture[at] == 255 ? 0 : texture[at + 1] == 255 ? 1 : 7;
        uint64_t selectors = 0;
        for (unsigned i = 0; i < 6; ++i) selectors |= uint64_t(texture[at + 2 + i]) << (i * 8);
        for (unsigned i = 0; i < 16; ++i) {
            if (repair.mask & (1u << i))
                selectors = (selectors & ~(7ull << (i * 3))) | (opaque << (i * 3));
        }
        for (unsigned i = 0; i < 6; ++i) texture[at + 2 + i] = uint8_t(selectors >> (i * 8));
    }
    return true;
}
bool RepairKnownStripOrientation(std::span<const uint8_t> vertices, std::span<uint8_t> source) {
    return RepairStripOrientation(vertices, source, kStripRepairs, kStripRepairOffsets);
}
bool RepairKnownRoomStripOrientation(uint32_t room, std::span<const uint8_t> vertices,
                                    std::span<uint8_t> source) {
    // Arena meshes also used in campaign rooms: prefer the arena's facing metadata.
    for (const auto& entry : kRoomStripRepairs) {
        if (entry.room == room && entry.repair.vertex_bytes == vertices.size() &&
            size_t(entry.repair.source_words) * 2 == source.size() && RepairStripOrientation(
                vertices, source, {&entry.repair, 1}, kRoomStripRepairOffsets)) return true;
    }
    return RepairKnownStripOrientation(vertices, source);
}

bool RepairStripOrientation(std::span<const uint8_t> vertices, std::span<uint8_t> source,
                           std::span<const StripRepair> repairs, std::span<const uint16_t> offsets) {
    if (source.empty() || source.size() % 2 || vertices.empty()) return false;
    const uint64_t signature = Signature(vertices, source);
    const auto match = std::lower_bound(repairs.begin(), repairs.end(), signature,
        [](const StripRepair& repair, uint64_t value) { return repair.signature < value; });
    if (match == repairs.end() || match->signature != signature ||
        match->vertex_bytes != vertices.size() || match->source_words != source.size() / 2 ||
        !match->offset_count || match->offset_begin > offsets.size() ||
        match->offset_count > offsets.size() - match->offset_begin) return false;
    const auto changes = offsets.subspan(match->offset_begin, match->offset_count);
    // Validate the complete repair before changing the original loaded buffer.
    for (size_t i = 0; i < changes.size(); ++i) {
        const size_t word = changes[i];
        if (word >= source.size() / 2 || (i && changes[i - 1] >= word) ||
            (source[word * 2 + 1] & 0xC0) != 0x80) return false;
    }
    for (uint16_t word : changes) source[size_t(word) * 2 + 1] |= 0x40;
    return true;
}
}
