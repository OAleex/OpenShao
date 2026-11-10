// Synthetic data only: exercise the assembler used by the D3D12 backend.
#include <cstdio>
#include <cstdlib>
#include <new>
#include <cstring>
#include <memory>
#include "gpu/vertex.h"
#include "kernel/kernel.h"

namespace kernel { uint8_t* g_base = nullptr; }

namespace {
bool measure_allocations = false;
unsigned allocations = 0;
}
void* operator new(size_t bytes) {
    if (measure_allocations) ++allocations;
    if (auto* ptr = std::malloc(bytes ? bytes : 1)) return ptr;
    throw std::bad_alloc();
}
void operator delete(void* ptr) noexcept { std::free(ptr); }
void operator delete(void* ptr, size_t) noexcept { std::free(ptr); }


int main() {
    if (!mem::Init()) return 2;
    kernel::g_base = mem::Base();
    const uint32_t data = mem::AllocContiguous(3 * 48, 0, 0xFFFFFFFF, 16);
    const uint32_t header = mem::AllocSystem(20);
    if (!data || !header) return 2;
    const float vertices[36] = {
        40, 60, .5f, 1, 1, 1, 1, 1, 0, 0, 0, 1,
        80, 60, .5f, 1, 1, 1, 1, 1, 1, 0, 0, 1,
        40, 90, .5f, 1, 1, 1, 1, 1, 0, 1, 0, 1};
    std::memcpy(kernel::HostPtr(data), vertices, sizeof(vertices));
    kernel::W32(header + 4, mem::PhysicalFromVirtual(data));
    gpu::State state{};
    state.vs = std::make_shared<gpu::VertexShader>();
    state.vs->inputs[0] = {0, 0, 0x42};
    state.vs->inputs[3] = {0, 16, 0x42};
    state.vs->inputs[9] = {0, 32, 0x42};
    state.vs->stream_stride[0] = 48;
    state.streams[0] = {header, 48};
    gpu::Assembled assembled;
    auto check = [&](uint32_t handle, bool screen, const char* label) {
        state.vs_handle = handle;
        if (!gpu::Assemble(state, 5, 0, 3, nullptr, assembled) || assembled.pretransformed != screen) {
            std::fprintf(stderr, "%s: expected screen=%d, got %d\n", label, screen, assembled.pretransformed);
            return false;
        }
        return assembled.vertex_count == 3 && assembled.regs == ((1u << 0) | (1u << 3) | (1u << 9));
    };
    // EndPrimitive preserves the source FVF bits and tags its synthetic FLOAT4
    // declaration with bit zero. The original 0x144 FVF means XYZRHW.
    if (!check(0x145, true, "immediate XYZRHW")) return 1;
    if (assembled.buffers[0].stride != 48 || std::memcmp(assembled.buffers[0].src, vertices, sizeof(vertices))) return 1;
    if (!check(0x143, false, "immediate XYZ")) return 1;
    if (!check(0xB0000001, false, "declaration-only shader")) return 1;
    state.vs->program = {0, 0, 0, 0};
    if (!check(0x145, false, "programmable shader")) return 1;
    // Three SHORTN components occupy six bytes, so the raw eight-byte host
    // format cannot read them at this stride. Exercise real conversion.
    const auto packed = mem::AllocContiguous(128 * 6, 0, 0xFFFFFFFF, 16);
    if (!packed) return 2;
    auto* values = static_cast<int16_t*>(kernel::HostPtr(packed));
    for (unsigned i = 0; i < 128; ++i) {
        values[3*i] = 0;
        values[3*i+1] = (i & 1) ? 32767 : 0;
        values[3*i+2] = 32767;
    }
    kernel::W32(header + 4, mem::PhysicalFromVirtual(packed));
    state.vs_handle = 0xB0000001;
    state.vs->inputs = {};
    state.vs->inputs[0] = {0, 0, 0x31};
    state.streams[0] = {header, 6};
    const uint16_t dense[] = {4, 2, 3}, sparse[] = {101, 2, 0};
    auto check_converted = [&](const uint16_t* indices, uint32_t count, uint32_t first, uint32_t second, uint32_t third) {
        if (!gpu::Assemble(state, 5, 0, 3, reinterpret_cast<const uint8_t*>(indices), assembled)) return false;
        return assembled.vertex_count == count && assembled.indices.size() == 3 &&
            assembled.indices[0] == first && assembled.indices[1] == second && assembled.indices[2] == third &&
            assembled.vertices[4 * first + 1] == float(indices[0] & 1) &&
            assembled.vertices[4 * second + 1] == float(indices[1] & 1) &&
            assembled.vertices[4 * third + 1] == float(indices[2] & 1);
    };
    if (!check_converted(sparse, 3, 0, 1, 2) || !check_converted(dense, 3, 2, 0, 1)) return 1;
    measure_allocations = true;
    for (unsigned i = 0; i < 64; ++i)
        if (!check_converted(dense, 3, 2, 0, 1) || !check_converted(sparse, 3, 0, 1, 2)) return 1;
    measure_allocations = false;
    if (allocations) {
        std::fprintf(stderr, "Converted draws allocated %u times after warm-up\n", allocations);
        return 1;
    }
    mem::FreeContiguous(packed);
    mem::FreeSystem(header);
    mem::FreeContiguous(data);
    std::puts("PASS: immediate XYZRHW selects screen coordinates without changing vertex payload; XYZ/programs retain their pipeline");
}
