#pragma once
// HLSL to SPIR-V via glslang; D3D registers map to one descriptor set:
// b0-b1 -> bindings 0-1, t0-t3 -> 2-5, s0-s3 -> 6-9.
#include <cstdint>
#include <string>
#include <vector>

namespace vkshader {

enum class Stage { Vertex, Geometry, Fragment };

constexpr uint32_t kUboBase = 0;
constexpr uint32_t kTextureBase = 2;
constexpr uint32_t kSamplerBase = 6;

bool Compile(const std::string& hlsl, const char* entry, Stage stage, std::vector<uint32_t>& spirv, std::string& error);

}
