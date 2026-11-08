#pragma once
#include <cstdint>
#include <map>
#include <optional>

// Hands out aligned ranges of a block of memory of fixed size and takes them back. It only does the bookkeeping: what the
// ranges mean (a graphics heap, a buffer) is up to the caller, so it is tested without a GPU.
namespace openshao::texpack {
class RangeAllocator {
public:
    explicit RangeAllocator(uint64_t capacity);

    // The offset of a free range of `size` bytes that starts at a multiple of `alignment`, the lowest that fits. Empty when
    // none does. `alignment` of 0 or 1 means any offset.
    std::optional<uint64_t> Allocate(uint64_t size, uint64_t alignment);

    // Gives a range back. The range must have been returned by Allocate with the same size. Adjacent free ranges join.
    void Free(uint64_t offset, uint64_t size);

    uint64_t capacity() const { return capacity_; }
    uint64_t free_bytes() const { return free_bytes_; }
    // True when nothing is allocated.
    bool empty() const { return free_bytes_ == capacity_; }

private:
    uint64_t capacity_ = 0;
    uint64_t free_bytes_ = 0;
    std::map<uint64_t, uint64_t> free_;  // offset to size, ordered, never adjacent
};
}
