#include "netplay/state_hash.h"
#include <cstring>

namespace openshao::netplay {
namespace {
constexpr uint64_t P1 = 0x9E3779B185EBCA87ull, P2 = 0xC2B2AE3D27D4EB4Full, P3 = 0x165667B19E3779F9ull,
                   P4 = 0x85EBCA77C2B2AE63ull, P5 = 0x27D4EB2F165667C5ull;
uint64_t Rotl(uint64_t v, int r) { return v << r | v >> (64 - r); }
uint64_t Read64(const uint8_t* p) { uint64_t v; std::memcpy(&v, p, 8); return v; }
uint32_t Read32(const uint8_t* p) { uint32_t v; std::memcpy(&v, p, 4); return v; }
uint64_t Round(uint64_t acc, uint64_t input) { return Rotl(acc + input * P2, 31) * P1; }
uint64_t Merge(uint64_t acc, uint64_t value) { return (acc ^ Round(0, value)) * P1 + P4; }
struct Range { uint32_t begin, end; RegionKind kind; };
constexpr Range kRanges[] = {
    {0x00010000u, 0x7FFE0000u, RegionKind::Virtual},
    {0x80000000u, 0x80100000u, RegionKind::Kernel},
    {0x80100000u, 0x84000000u, RegionKind::Physical},
    {0xB0000000u, 0xC0000000u, RegionKind::System},
    {0xD0000000u, 0xE0000000u, RegionKind::Stack},
};
}  // namespace

uint64_t Xxh64(const void* data, size_t size, uint64_t seed) {
    const auto* p = static_cast<const uint8_t*>(data);
    const uint8_t* end = p + size;
    uint64_t h;
    if (size >= 32) {
        uint64_t v1 = seed + P1 + P2, v2 = seed + P2, v3 = seed, v4 = seed - P1;
        for (const uint8_t* limit = end - 32; p <= limit; p += 32) {
            v1 = Round(v1, Read64(p)); v2 = Round(v2, Read64(p + 8));
            v3 = Round(v3, Read64(p + 16)); v4 = Round(v4, Read64(p + 24));
        }
        h = Rotl(v1, 1) + Rotl(v2, 7) + Rotl(v3, 12) + Rotl(v4, 18);
        h = Merge(h, v1); h = Merge(h, v2); h = Merge(h, v3); h = Merge(h, v4);
    } else {
        h = seed + P5;
    }
    h += uint64_t(size);
    for (; p + 8 <= end; p += 8) h = Rotl(h ^ Round(0, Read64(p)), 27) * P1 + P4;
    if (p + 4 <= end) { h = Rotl(h ^ uint64_t(Read32(p)) * P1, 23) * P2 + P3; p += 4; }
    for (; p < end; ++p) h = Rotl(h ^ uint64_t(*p) * P5, 11) * P1;
    h ^= h >> 33; h *= P2; h ^= h >> 29; h *= P3; h ^= h >> 32;
    return h;
}

const char* KindName(RegionKind kind) {
    switch (kind) {
        case RegionKind::Image: return "image";
        case RegionKind::Virtual: return "virtual";
        case RegionKind::Kernel: return "kernel";
        case RegionKind::Physical: return "physical";
        case RegionKind::System: return "system";
        case RegionKind::Stack: return "stack";
    }
    return "unknown";
}

std::vector<BlockHash> HashBlocks(const MemoryView& view) {
    std::vector<BlockHash> out;
    for (const auto& range : kRanges) {
        for (uint64_t block = range.begin; block < range.end; block += kHashBlock) {
            uint64_t h = 0, mask = 0;
            bool any = false;
            for (uint32_t i = 0; i < kHashBlock / kHashPage; ++i) {
                const auto page = uint32_t(block + uint64_t(i) * kHashPage);
                const uint8_t* data = page < range.end ? view.page(page) : nullptr;
                if (!data) continue;
                any = true;
                mask |= 1ull << i;
                h = Xxh64(data, kHashPage, h ^ (uint64_t(i) + 1) * P5);
            }
            if (!any) continue;
            auto kind = range.kind;
            if (kind == RegionKind::Virtual && block < view.image_end && block + kHashBlock > view.image_begin) kind = RegionKind::Image;
            out.push_back({uint32_t(block), kind, Xxh64(&mask, 8, h)});
        }
    }
    return out;
}

uint64_t Combine(const std::vector<BlockHash>& blocks) {
    uint64_t h = 0;
    for (const auto& b : blocks) {
        uint8_t entry[12];
        std::memcpy(entry, &b.va, 4);
        std::memcpy(entry + 4, &b.hash, 8);
        h = Xxh64(entry, sizeof(entry), h);
    }
    return h;
}
}  // namespace openshao::netplay
