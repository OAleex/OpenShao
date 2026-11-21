#include "texture_range_allocator.h"
#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using namespace openshao::texpack;

static void Check(bool ok, const std::string& text) {
    if (!ok) throw std::runtime_error(text);
}

int main() try {
    // A new block is one free range
    {
        RangeAllocator a(1000);
        Check(a.capacity() == 1000 && a.free_bytes() == 1000 && a.empty(), "a new block is empty");
        Check(!RangeAllocator(0).Allocate(1, 1).has_value(), "a block of no bytes holds nothing");
    }

    // Allocations are placed from the lowest offset and the free bytes follow
    {
        RangeAllocator a(1000);
        const auto first = a.Allocate(100, 1);
        const auto second = a.Allocate(200, 1);
        Check(first && *first == 0, "the first allocation starts the block");
        Check(second && *second == 100, "the second follows the first");
        Check(a.free_bytes() == 700 && !a.empty(), "the free bytes are what is left");
        Check(!a.Allocate(0, 1).has_value(), "no allocation of no bytes");
    }

    // Alignment skips the bytes before an aligned offset and keeps them free
    {
        RangeAllocator a(1 << 20);
        Check(*a.Allocate(100, 1) == 0, "an unaligned allocation");
        const auto aligned = a.Allocate(4096, 65536);
        Check(aligned && *aligned == 65536, "the next 64 KiB boundary");
        // The 65436 bytes before it can still be used
        const auto gap = a.Allocate(60000, 1);
        Check(gap && *gap == 100, "the bytes skipped by alignment are reused");
        Check(a.free_bytes() == (1u << 20) - 100 - 4096 - 60000, "free bytes after the three");
    }

    // Exhaustion and exact fit
    {
        RangeAllocator a(256);
        Check(a.Allocate(256, 1).has_value(), "an exact fit");
        Check(a.free_bytes() == 0, "nothing left");
        Check(!a.Allocate(1, 1).has_value(), "no room");
        a.Free(0, 256);
        Check(a.empty() && a.Allocate(257, 1) == std::nullopt, "a block cannot give more than it has");
    }

    // Allocation that cannot be aligned inside the only free range
    {
        RangeAllocator a(1000);
        Check(a.Allocate(10, 1).has_value(), "setup");
        Check(!a.Allocate(950, 512).has_value(), "the aligned start leaves too little");
        Check(a.Allocate(400, 512).has_value(), "but a smaller one fits at 512");
    }

    // Freed ranges join their neighbours, in any order
    {
        RangeAllocator a(1000);
        const auto x = *a.Allocate(100, 1), y = *a.Allocate(100, 1), z = *a.Allocate(100, 1);
        a.Free(y, 100);
        a.Free(x, 100);  // joins y on its right
        a.Free(z, 100);  // joins the joined range and the tail
        Check(a.empty(), "everything is free");
        Check(a.Allocate(1000, 1) == std::optional<uint64_t>(0), "one range of the whole block again");
    }

    // A freed gap is reused first (the lowest range that fits)
    {
        RangeAllocator a(1000);
        const auto x = *a.Allocate(100, 1);
        const auto y = *a.Allocate(100, 1);
        (void)y;
        a.Free(x, 100);
        Check(*a.Allocate(50, 1) == 0, "the lowest free range is used");
        Check(*a.Allocate(60, 1) == 200, "a range too small is passed over");
    }

    // Many random allocations and frees never overlap and always restore the block
    {
        std::mt19937 random(7);
        RangeAllocator a(1 << 24);
        struct Live { uint64_t offset, size; };
        std::vector<Live> live;
        const uint64_t alignments[] = {1, 512, 4096, 65536};
        for (int step = 0; step < 20000; ++step) {
            if (live.empty() || (random() % 3) != 0) {
                const uint64_t size = 1 + random() % 200000, alignment = alignments[random() % 4];
                if (const auto offset = a.Allocate(size, alignment)) {
                    Check(*offset % alignment == 0, "the offset is aligned");
                    Check(*offset + size <= a.capacity(), "the range is inside the block");
                    for (const auto& other : live)
                        Check(*offset + size <= other.offset || other.offset + other.size <= *offset, "ranges do not overlap");
                    live.push_back({*offset, size});
                }
            } else {
                const size_t index = random() % live.size();
                a.Free(live[index].offset, live[index].size);
                live.erase(live.begin() + index);
            }
            uint64_t used = 0;
            for (const auto& l : live) used += l.size;
            Check(a.free_bytes() == a.capacity() - used, "the free bytes match the live ranges");
        }
        for (const auto& l : live) a.Free(l.offset, l.size);
        Check(a.empty() && a.Allocate(1 << 24, 1) == std::optional<uint64_t>(0), "freeing everything restores the block");
    }
    std::cout << "texture range allocator: ok\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr << "FAIL: " << e.what() << "\n";
    return 1;
}
