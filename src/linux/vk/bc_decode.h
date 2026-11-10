#pragma once
// BC1/BC2/BC3 to BGRA8 when textureCompressionBC is missing (Khronos Data Format layouts).
#include <cstdint>
#include <vector>

namespace vkbc {

// blocks: ceil(h/4) rows of block_pitch bytes; out: BGRA rows, size rounded up to whole blocks.
void Decode(int bc, const uint8_t* blocks, uint32_t block_pitch, uint32_t width, uint32_t height, uint32_t slices,
            std::vector<uint8_t>& out, uint32_t& out_pitch);

}
