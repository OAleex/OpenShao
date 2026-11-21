#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <unordered_map>

#include "pipeline_precompile.h"

namespace openshao::pipelines {

// These are the combiner fields copied from D3DPIXELSHADERDEF by MakePixelKey.
// Texture modes and bindings are effective draw state and can vary for one program.
inline void RestorePixelProgram(gpu::PixelKey& pixel, const gpu::PixelKey& program) {
    pixel.count = program.count;
    std::memcpy(pixel.rgb_in, program.rgb_in, sizeof(pixel.rgb_in));
    std::memcpy(pixel.rgb_out, program.rgb_out, sizeof(pixel.rgb_out));
    std::memcpy(pixel.a_in, program.a_in, sizeof(pixel.a_in));
    std::memcpy(pixel.a_out, program.a_out, sizeof(pixel.a_out));
    pixel.final_abcd = program.final_abcd;
    pixel.final_efg = program.final_efg;
    pixel.dot_mapping = program.dot_mapping;
    pixel.input_texture = program.input_texture;
    pixel.compare_mode = program.compare_mode;
}

inline uint64_t PixelProgramFingerprint(const gpu::PixelKey& pixel) {
    if (!pixel.programmable) return 0;
    const auto combiner = gpu::Hash(&pixel.count, offsetof(gpu::PixelKey, tex_modes) - offsetof(gpu::PixelKey, count));
    return gpu::Hash(&pixel.dot_mapping, offsetof(gpu::PixelKey, ff) - offsetof(gpu::PixelKey, dot_mapping), combiner) | 1;
}

using PixelPrograms = std::unordered_map<uint64_t, gpu::PixelKey>;

struct CatalogEntry {
    std::array<uint32_t, 8> rooms{};
    uint64_t pixel_program = 0;
    uint64_t vs_source = 0;
    uint64_t ps_source = 0;
    VertexRecipe vertex{};
    gpu::PixelKey pixel{};
    uint32_t key_size = 0;
    std::array<uint32_t, kMaxPipelineKey / sizeof(uint32_t)> key{};
};

}

namespace d3d {
void SnapshotPipelinePixelPrograms(openshao::pipelines::PixelPrograms&);
}
