// Synthetic declarations and geometry: compare prediction with the real draw assembler.
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>

#include <dxgiformat.h>

#include "gpu/vertex.h"
#include "kernel/kernel.h"

namespace kernel { uint8_t* g_base = nullptr; }
namespace gpu { bool DescribePipelineLayout(const State&, uint32_t, Assembled&); }

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

gpu::State Declaration(uint32_t stride) {
    gpu::State s{};
    s.vs_handle = 0xB0000001;
    s.vs = std::make_shared<gpu::VertexShader>();
    s.vs->inputs[0] = {0, 0, 0x35};  // SHORT3
    s.vs->inputs[3] = {0, 6, 0x40};  // D3DCOLOR
    s.vs->inputs[9] = {0, 10, 0x21}; // NORMSHORT2
    s.vs->stream_stride[0] = stride;
    s.streams[0] = {1, stride};     // sentinel only; prediction must not dereference it
    return s;
}

void RequireLayoutEqual(const gpu::Assembled& predicted, const gpu::Assembled& drawn) {
    Require(predicted.regs == drawn.regs, "predicted input registers match the actual draw");
    Require(predicted.decode == drawn.decode, "predicted vertex decoding matches the actual draw");
    Require(predicted.topology == drawn.topology, "predicted topology matches the actual draw");
    Require(predicted.strip == drawn.strip, "predicted strip/list choice matches the actual draw");
    Require(predicted.pretransformed == drawn.pretransformed, "predicted position space matches the actual draw");
    Require(predicted.buffer_count == drawn.buffer_count, "predicted stream compaction matches the actual draw");
    for (unsigned r = 0; r < 16; ++r) {
        if (!(drawn.regs & (1u << r))) continue;
        Require(predicted.elements[r].format == drawn.elements[r].format, "predicted host format matches the actual draw");
        Require(predicted.elements[r].buffer == drawn.elements[r].buffer, "predicted element buffer matches the actual draw");
        Require(predicted.elements[r].offset == drawn.elements[r].offset, "predicted element offset matches the actual draw");
    }
    for (unsigned b = 0; b < drawn.buffer_count; ++b) {
        Require(predicted.buffers[b].stride == drawn.buffers[b].stride, "predicted host stride matches the actual draw");
        Require(!predicted.buffers[b].src && !predicted.buffers[b].bytes, "prediction retains no geometry pointers or byte spans");
    }
    Require(predicted.vertices.empty() && predicted.indices.empty(), "prediction does not convert vertices or indices");
}
}

int main() {
    try {
        gpu::Assembled predicted;
        auto s = Declaration(14);
        // Wrongly using the original 14-byte layout would miss the real converted PSO.
        Require(gpu::DescribePipelineLayout(s, 6, predicted), "a loaded packed declaration can be predicted without vertex memory");
        Require(predicted.regs == 0x209, "all three declared input registers are retained");
        Require(predicted.buffer_count == 1 && predicted.buffers[0].stride == 48, "unaligned packed vertices use a compact float4 host layout");
        Require(predicted.elements[0].format == DXGI_FORMAT_R32G32B32A32_FLOAT &&
                predicted.elements[3].format == DXGI_FORMAT_R32G32B32A32_FLOAT &&
                predicted.elements[9].format == DXGI_FORMAT_R32G32B32A32_FLOAT,
                "CPU conversion changes all declared host elements to float4");
        Require(predicted.elements[0].offset == 0 && predicted.elements[3].offset == 16 &&
                predicted.elements[9].offset == 32, "converted elements are compacted by input-register order");
        Require(predicted.decode == 0 && !predicted.strip && predicted.topology == gpu::Topology::Triangles,
                "CPU conversion expands strips to triangle lists without shader decoding");

        s.vs->inputs[0] = {5, 0, 0x16}; // NORMPACKED3
        s.vs->inputs[3] = {2, 0, 0x40};
        s.vs->inputs[9] = {5, 4, 0x22};
        s.streams[0] = {};
        s.streams[5] = {1, 12};
        s.streams[2] = {1, 0};
        s.vs->stream_stride[2] = 4;
        Require(gpu::DescribePipelineLayout(s, 6, predicted), "raw multi-stream declaration is predictable");
        Require(predicted.buffer_count == 2 && predicted.buffers[0].stride == 12 &&
                predicted.buffers[1].stride == 4, "raw stream slots follow first register use, with declaration stride fallback");
        Require(predicted.elements[0].buffer == 0 && predicted.elements[3].buffer == 1 &&
                predicted.elements[9].buffer == 0, "sparse original streams are compacted consistently");
        Require(predicted.elements[0].format == DXGI_FORMAT_R32_UINT && predicted.decode == gpu::kDecodePacked,
                "packed normals keep their raw integer format and shader decode");
        Require(predicted.strip && predicted.topology == gpu::Topology::Triangles, "raw native triangle strips retain their topology");
        Require(!predicted.buffers[0].src && predicted.vertices.empty() && predicted.indices.empty(),
                "metadata prediction performs no guest-memory extraction");

        for (uint32_t prim = 1; prim <= 10; ++prim) {
            Require(gpu::DescribePipelineLayout(s, prim, predicted), "each valid original primitive has a metadata layout");
            Require(predicted.topology == (prim == 1 ? gpu::Topology::Points : prim <= 4 ? gpu::Topology::Lines : gpu::Topology::Triangles),
                    "primitive class is preserved for points, lines and triangles");
            Require(predicted.strip == (prim == 4 || prim == 6), "only native strip primitives retain strip topology");
        }
        Require(!gpu::DescribePipelineLayout(s, 0, predicted) && !gpu::DescribePipelineLayout(s, 11, predicted),
                "invalid primitive codes cannot submit predicted work");
        s.vs->inputs[0].stream = 16;
        Require(!gpu::DescribePipelineLayout(s, 5, predicted), "out-of-range declaration streams cannot index captured host state");
        s.vs->inputs[0].stream = 5;
        s.streams[2] = s.streams[5] = {};
        Require(!gpu::DescribePipelineLayout(s, 5, predicted), "unbound declarations cannot claim a complete pipeline layout");
        s.vs.reset();
        Require(!gpu::DescribePipelineLayout(s, 5, predicted), "a missing declaration cannot claim readiness");

        Require(mem::Init(), "synthetic guest memory initializes");
        kernel::g_base = mem::Base();
        const uint32_t bytes = mem::AllocContiguous(256, 0, 0xFFFFFFFF, 16);
        const uint32_t header = mem::AllocSystem(20);
        Require(bytes && header, "synthetic draw buffers allocate");
        std::memset(kernel::HostPtr(bytes), 0, 256);
        kernel::W32(header + 4, mem::PhysicalFromVirtual(bytes));
        gpu::Assembled drawn;
        for (uint32_t stride : {14u, 16u}) {
            s = Declaration(stride);
            if (stride == 16) {
                s.vs->inputs[3].offset = 8;
                s.vs->inputs[9].offset = 12;
            }
            s.streams[0].vb = header;
            for (uint32_t prim = 1; prim <= 10; ++prim) {
                Require(gpu::DescribePipelineLayout(s, prim, predicted), "prediction accepts synthetic draw layout");
                Require(gpu::Assemble(s, prim, 0, 6, nullptr, drawn), "actual draw assembles synthetic vertices");
                RequireLayoutEqual(predicted, drawn);
            }
        }
        for (uint32_t handle : {0x144u, 0x145u, 0x143u}) {
            s = Declaration(48);
            s.vs_handle = handle;
            s.vs->inputs[0] = {0, 0, 0x42};
            s.vs->inputs[3] = {0, 16, 0x42};
            s.vs->inputs[9] = {0, 32, 0x42};
            s.streams[0].vb = header;
            Require(gpu::DescribePipelineLayout(s, 5, predicted), "FVF position metadata can be predicted");
            Require(gpu::Assemble(s, 5, 0, 3, nullptr, drawn), "FVF synthetic vertices assemble");
            RequireLayoutEqual(predicted, drawn);
        }
        mem::FreeSystem(header);
        mem::FreeContiguous(bytes);
        std::puts("pipeline layout: packed/raw/FVF metadata matches actual draw assembly without geometry access");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "pipeline layout: %s\n", error.what());
        return 1;
    }
}
