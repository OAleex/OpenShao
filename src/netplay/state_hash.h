#pragma once
#include <cstdint>
#include <functional>
#include <vector>

namespace openshao::netplay {
uint64_t Xxh64(const void* data, size_t size, uint64_t seed = 0);

enum class RegionKind : uint8_t { Image, Virtual, Kernel, Physical, System, Stack };
const char* KindName(RegionKind kind);

inline constexpr uint32_t kHashBlock = 0x10000;
inline constexpr uint32_t kHashPage = 0x1000;

struct BlockHash {
    uint32_t va;
    RegionKind kind;
    uint64_t hash;
};

// Guest memory as the hash sees it. The physical window at 0x80000000 is hashed once;
// its 0xF0000000 alias is never scanned.
struct MemoryView {
    std::function<const uint8_t*(uint32_t page_va)> page;  // nullptr for an uncommitted page
    uint32_t image_begin = 0, image_end = 0;
};

// One hash per 64 KiB block that has a committed page. The set of committed pages is part of
// each block hash, so a commit or decommit is a difference even when the bytes match.
std::vector<BlockHash> HashBlocks(const MemoryView& view);
uint64_t Combine(const std::vector<BlockHash>& blocks);
}  // namespace openshao::netplay
