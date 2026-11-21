#include "texture_range_allocator.h"

#include <iterator>

namespace openshao::texpack {
RangeAllocator::RangeAllocator(uint64_t capacity) : capacity_(capacity), free_bytes_(capacity) {
    if (capacity) free_.emplace(0, capacity);
}

std::optional<uint64_t> RangeAllocator::Allocate(uint64_t size, uint64_t alignment) {
    if (!size) return std::nullopt;
    const uint64_t step = alignment ? alignment : 1;
    for (auto it = free_.begin(); it != free_.end(); ++it) {
        const uint64_t start = it->first, length = it->second;
        const uint64_t aligned = (start + step - 1) / step * step;
        if (aligned < start || aligned - start > length || length - (aligned - start) < size) continue;
        // The range is cut into what comes before the allocation and what comes after it.
        const uint64_t before = aligned - start;
        const uint64_t after = length - before - size;
        free_.erase(it);
        if (before) free_.emplace(start, before);
        if (after) free_.emplace(aligned + size, after);
        free_bytes_ -= size;
        return aligned;
    }
    return std::nullopt;
}

void RangeAllocator::Free(uint64_t offset, uint64_t size) {
    if (!size || offset + size > capacity_) return;
    free_bytes_ += size;
    auto next = free_.lower_bound(offset);
    // Joins the range that ends where this one starts.
    if (next != free_.begin()) {
        auto previous = std::prev(next);
        if (previous->first + previous->second == offset) {
            offset = previous->first;
            size += previous->second;
            free_.erase(previous);
        }
    }
    // Joins the range that starts where this one ends.
    if (next != free_.end() && offset + size == next->first) {
        size += next->second;
        free_.erase(next);
    }
    free_.emplace(offset, size);
}
}
