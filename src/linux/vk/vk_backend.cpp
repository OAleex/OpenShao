#include "linux/vk/vk_api.h"
#include "linux/vk/vk_window.h"
#ifdef _WIN32
#include <backends/imgui_impl_win32.h>
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <mutex>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <dxgiformat.h>
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>

#include "app/app.h"
#include "core/diag.h"
#include "core/log.h"
#include "gpu/backend.h"
#include "gpu/shaders.h"
#include "gpu/texture.h"
#include "gpu/vertex.h"
#include "host.h"
#include "launcher.h"
#include "aspect_ratio.h"
#include "bloom_effect_runtime.h"
#include "kernel/kernel.h"
#include "linux/vk/bc_decode.h"
#include "linux/vk/vk_shader.h"
#include "pipeline_precompile.h"
#include "pipeline_assets.h"
#include "texture_pack.h"
#include "texture_pack_stats.h"
#include "pipeline_source_cache.h"
#include "raster_compat.h"
#include "runtime_paths.h"
#include "sfd_filter_compat.h"

using vkapi::vk;

namespace texpack = openshao::texpack;

namespace gpu {

namespace {

constexpr int kFrames = 3;
constexpr uint64_t kStaleFrames = 8;  // frames a target may go unused and still be read back
constexpr uint64_t kUploadSize = 64ull << 20;
// Wait for the GPU before wrapping a frame ring with insufficient space for the next draw.
constexpr uint64_t kUploadReserve = 8ull << 20;
constexpr VkFormat kBgra = VK_FORMAT_B8G8R8A8_UNORM;
constexpr uint32_t kQueries = 8192;  // occlusion queries in flight, several frames of counted draws
constexpr uint32_t kNoQuery = ~0u;
constexpr uint32_t kSetsPerPool = 4096;
constexpr size_t kSamplersKept = 2048;  // the sampler cache starts over past this many

using PerfClock = std::chrono::steady_clock;
double PerfMs(PerfClock::time_point t) {
    return std::chrono::duration<double, std::milli>(PerfClock::now() - t).count();
}
struct Perf {
    double draw = 0, assemble = 0, bind = 0, sig = 0, texup = 0, compile = 0, clear = 0, copy = 0, present = 0, wait = 0,
           swap = 0, sync = 0;
    uint64_t draws = 0, verts = 0, upload = 0, overflow = 0, sigs = 0, sig_bytes = 0, texups = 0, texup_bytes = 0,
             newtex = 0, targets = 0, rtups = 0, shrinks = 0, syncs = 0, psos = 0, shaders = 0, raw = 0, lates = 0;
};
Perf g_perf;
struct PerfScope {
    double& acc;
    PerfClock::time_point t = PerfClock::now();
    ~PerfScope() { acc += PerfMs(t); }
};

const char* kBlitHlsl = R"(
cbuffer B : register(b1) { float4 uvrect; float4 opts; };
Texture2D img : register(t0);
Texture2D lut : register(t1);
SamplerState ls : register(s0);
SamplerState ps : register(s1);
struct O { float4 p : SV_Position; float2 uv : TEXCOORD0; };
O vs(uint id : SV_VertexID) {
    O o;
    float2 t = float2((id << 1) & 2, id & 2);
    o.p = float4(t * float2(2, -2) + float2(-1, 1), 0, 1);
    o.uv = uvrect.xy + t * uvrect.zw;
    return o;
}
float4 psmain(O i) : SV_Target {
    float4 c = 0;
    int n = max((int)opts.z, 1);
    float2 px = fwidth(i.uv);
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) c += img.SampleLevel(ls, i.uv + px * ((float2(x, y) + 0.5) / n - 0.5), 0);
    c /= n * n;
    c.rgb *= opts.w;
    if (opts.x > 0) c.rgb = float3(lut.Sample(ps, float2(c.r, 0.5)).r, lut.Sample(ps, float2(c.g, 0.5)).g, lut.Sample(ps, float2(c.b, 0.5)).b);
    return float4(c.rgb, opts.y > 0 ? c.a : 1);
}
)";

const char* kDepthHlsl = R"(
Texture2D<float> img : register(t0);
SamplerState ps : register(s1);
struct O { float4 p : SV_Position; float2 uv : TEXCOORD0; };
O vs(uint id : SV_VertexID) {
    O o;
    o.uv = float2((id << 1) & 2, id & 2);
    o.p = float4(o.uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return o;
}
float psmain(O i) : SV_Depth { return img.SampleLevel(ps, i.uv, 0); }
)";

float Bits(uint32_t v) {
    float f;
    std::memcpy(&f, &v, 4);
    return f;
}

void Color(uint32_t argb, float* o) {
    o[0] = ((argb >> 16) & 255) / 255.0f;
    o[1] = ((argb >> 8) & 255) / 255.0f;
    o[2] = (argb & 255) / 255.0f;
    o[3] = (argb >> 24) / 255.0f;
}

// D3DBLEND, the console uses GL values
constexpr uint32_t kBlendDestColor = 0x306;  // D3DBLEND_DESTCOLOR

VkBlendFactor Blend(uint32_t v, bool alpha) {
    switch (v) {
        case 0: return VK_BLEND_FACTOR_ZERO;
        case 1: return VK_BLEND_FACTOR_ONE;
        case 0x300: return alpha ? VK_BLEND_FACTOR_SRC_ALPHA : VK_BLEND_FACTOR_SRC_COLOR;
        case 0x301: return alpha ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA : VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case 0x302: return VK_BLEND_FACTOR_SRC_ALPHA;
        case 0x303: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case 0x304: return VK_BLEND_FACTOR_DST_ALPHA;
        case 0x305: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case kBlendDestColor: return alpha ? VK_BLEND_FACTOR_DST_ALPHA : VK_BLEND_FACTOR_DST_COLOR;
        case 0x307: return alpha ? VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA : VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case 0x308: return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        case 0x8001: case 0x8003: return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case 0x8002: case 0x8004: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        default: return VK_BLEND_FACTOR_ONE;
    }
}

// D3DBLENDOP, GL values
VkBlendOp BlendOp(uint32_t v) {
    switch (v) {
        case 0x800A: return VK_BLEND_OP_SUBTRACT;
        case 0x800B: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case 0x8007: return VK_BLEND_OP_MIN;
        case 0x8008: return VK_BLEND_OP_MAX;
        default: return VK_BLEND_OP_ADD;
    }
}

// D3DCMP_NEVER 0x200 to D3DCMP_ALWAYS 0x207: 0 to 7, Vulkan's order, which the shaders' constants use too
uint32_t CmpIndex(uint32_t v) { return v >= 0x200 && v <= 0x207 ? v - 0x200 : 7; }
VkCompareOp Cmp(uint32_t v) { return VkCompareOp(CmpIndex(v)); }

// D3DSTENCILOP, GL values
VkStencilOp StencilOp(uint32_t v) {
    switch (v) {
        case 0: return VK_STENCIL_OP_ZERO;
        case 0x1E01: return VK_STENCIL_OP_REPLACE;
        case 0x1E02: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case 0x1E03: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case 0x150A: return VK_STENCIL_OP_INVERT;
        case 0x8507: return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case 0x8508: return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        default: return VK_STENCIL_OP_KEEP;
    }
}

// D3DTADDRESS: 1 wrap, 2 mirror, 3 clamp, 4 border, 5 clamp to edge
VkSamplerAddressMode Address(uint32_t v) {
    switch (v) {
        case 2: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case 3: case 5: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case 4: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

// the vertex formats gpu/vertex.cpp hands over, as DXGI values
VkFormat VertexFormat(uint32_t dxgi) {
    switch (dxgi) {
        case DXGI_FORMAT_R32G32B32A32_FLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case DXGI_FORMAT_R32G32B32_FLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
        case DXGI_FORMAT_R16G16B16A16_SNORM: return VK_FORMAT_R16G16B16A16_SNORM;
        case DXGI_FORMAT_R16G16B16A16_SINT: return VK_FORMAT_R16G16B16A16_SINT;
        case DXGI_FORMAT_R32G32_FLOAT: return VK_FORMAT_R32G32_SFLOAT;
        case DXGI_FORMAT_R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
        case DXGI_FORMAT_R16G16_SNORM: return VK_FORMAT_R16G16_SNORM;
        case DXGI_FORMAT_R16G16_SINT: return VK_FORMAT_R16G16_SINT;
        case DXGI_FORMAT_R32_FLOAT: return VK_FORMAT_R32_SFLOAT;
        case DXGI_FORMAT_R32_UINT: return VK_FORMAT_R32_UINT;
        case DXGI_FORMAT_R8G8_UNORM: return VK_FORMAT_R8G8_UNORM;
        case DXGI_FORMAT_R16_SNORM: return VK_FORMAT_R16_SNORM;
        case DXGI_FORMAT_R16_SINT: return VK_FORMAT_R16_SINT;
        case DXGI_FORMAT_R8_UNORM: return VK_FORMAT_R8_UNORM;
        case DXGI_FORMAT_B8G8R8A8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
        default: return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
}

constexpr uint32_t kFmtD16 = 0x2C;     // D3DFMT_D16
constexpr uint32_t kFmtLinD16 = 0x30;  // D3DFMT_LIN_D16

uint8_t ShadowDepthKind(uint32_t format) { return format == kFmtD16 || format == kFmtLinD16 ? 2 : 1; }

// bytes per row of a 32 bit surface in guest memory, rounded to 64
uint32_t GuestPitch(uint32_t width) { return (width * 4 + 63) & ~63u; }

bool IsDepthFormat(VkFormat f) {
    return f == VK_FORMAT_D24_UNORM_S8_UINT || f == VK_FORMAT_D32_SFLOAT_S8_UINT || f == VK_FORMAT_D32_SFLOAT ||
           f == VK_FORMAT_D16_UNORM;
}
bool HasStencil(VkFormat f) { return f == VK_FORMAT_D24_UNORM_S8_UINT || f == VK_FORMAT_D32_SFLOAT_S8_UINT; }
bool IsBlockFormat(VkFormat f) {
    return f == VK_FORMAT_BC1_RGBA_UNORM_BLOCK || f == VK_FORMAT_BC2_UNORM_BLOCK || f == VK_FORMAT_BC3_UNORM_BLOCK;
}

// Hash small surfaces fully; sample larger surfaces at evenly spaced positions.
constexpr uint32_t kSigWholeMax = 65536;
constexpr uint32_t kSigSamples = 64;
constexpr uint32_t kSigSampleBytes = 64;

uint64_t MemSig(uint32_t data, uint32_t total) {
    const uint8_t* p = static_cast<const uint8_t*>(kernel::HostPtr(data));
    uint64_t sig = Hash(&total, sizeof(total));
    g_perf.sigs++;
    g_perf.sig_bytes += total <= kSigWholeMax ? total : kSigSamples * kSigSampleBytes;
    if (total <= kSigWholeMax) return Hash(p, total, sig);
    for (uint32_t i = 0; i < kSigSamples; i++) {
        const uint8_t* sample = p + uint64_t(total - kSigSampleBytes) * i / (kSigSamples - 1);
        sig = Hash(sample, kSigSampleBytes, sig);
    }
    return sig;
}


uint32_t g_memory_objects = 0;  // live vkAllocateMemory allocations
VkPhysicalDeviceMemoryProperties g_memory_props{};

bool AllocateMemory(const VkMemoryRequirements& req, VkMemoryPropertyFlags need, VkMemoryPropertyFlags prefer,
                    VkDeviceMemory& out) {
    for (int pass = 0; pass < 2; pass++) {
        VkMemoryPropertyFlags want = pass == 0 ? need | prefer : need;
        for (uint32_t i = 0; i < g_memory_props.memoryTypeCount; i++) {
            if (!(req.memoryTypeBits & (1u << i))) continue;
            if ((g_memory_props.memoryTypes[i].propertyFlags & want) != want) continue;
            VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            ai.allocationSize = req.size;
            ai.memoryTypeIndex = i;
            if (vk.vkAllocateMemory(vkapi::device, &ai, nullptr, &out) == VK_SUCCESS) {
                g_memory_objects++;
                return true;
            }
        }
    }
    return false;
}

struct Buffer {
    VkBuffer buf = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    uint8_t* map = nullptr;
    VkDeviceSize size = 0;
    ~Buffer() {
        if (buf) vk.vkDestroyBuffer(vkapi::device, buf, nullptr);
        if (mem) {
            vk.vkFreeMemory(vkapi::device, mem, nullptr);
            g_memory_objects--;
        }
    }
};
using BufferP = std::shared_ptr<Buffer>;

BufferP CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, bool readback) {
    auto b = std::make_shared<Buffer>();
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vk.vkCreateBuffer(vkapi::device, &bi, nullptr, &b->buf) != VK_SUCCESS) return nullptr;
    VkMemoryRequirements req;
    vk.vkGetBufferMemoryRequirements(vkapi::device, b->buf, &req);
    VkMemoryPropertyFlags need = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    VkMemoryPropertyFlags prefer = readback ? VK_MEMORY_PROPERTY_HOST_CACHED_BIT : 0;
    if (!AllocateMemory(req, need, prefer, b->mem)) return nullptr;
    vk.vkBindBufferMemory(vkapi::device, b->buf, b->mem, 0);
    void* p = nullptr;
    if (vk.vkMapMemory(vkapi::device, b->mem, 0, VK_WHOLE_SIZE, 0, &p) != VK_SUCCESS) return nullptr;
    b->map = static_cast<uint8_t*>(p);
    b->size = size;
    return b;
}

struct Image {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;    // sampled: colour, or a depth format's depth
    VkImageView attach = VK_NULL_HANDLE;  // rendered to: colour, or depth and stencil
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t w = 1, h = 1, depth = 1, levels = 1, layers = 1;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    bool owned = true;  // false: a swap chain's
    ~Image() {
        if (attach && attach != view) vk.vkDestroyImageView(vkapi::device, attach, nullptr);
        if (view) vk.vkDestroyImageView(vkapi::device, view, nullptr);
        if (owned && image) vk.vkDestroyImage(vkapi::device, image, nullptr);
        if (mem) {
            vk.vkFreeMemory(vkapi::device, mem, nullptr);
            g_memory_objects--;
        }
    }
    VkImageAspectFlags Aspects() const {
        if (!IsDepthFormat(format)) return VK_IMAGE_ASPECT_COLOR_BIT;
        return HasStencil(format) ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;
    }
};
using ImageP = std::shared_ptr<Image>;

enum class ImageKind { Tex2D, Tex3D, Cube };

struct ImageSpec {
    VkFormat format = kBgra;
    uint32_t w = 1, h = 1, depth = 1, levels = 1;
    ImageKind kind = ImageKind::Tex2D;
    VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
};

ImageP CreateImage(const ImageSpec& s) {
    auto img = std::make_shared<Image>();
    VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ci.imageType = s.kind == ImageKind::Tex3D ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
    ci.format = s.format;
    ci.extent = {s.w, s.h, s.kind == ImageKind::Tex3D ? s.depth : 1};
    ci.mipLevels = s.levels;
    ci.arrayLayers = s.kind == ImageKind::Cube ? 6 : 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = s.usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (s.kind == ImageKind::Cube) ci.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    if (vk.vkCreateImage(vkapi::device, &ci, nullptr, &img->image) != VK_SUCCESS) return nullptr;
    VkMemoryRequirements req;
    vk.vkGetImageMemoryRequirements(vkapi::device, img->image, &req);
    if (!AllocateMemory(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, img->mem) &&
        !AllocateMemory(req, 0, 0, img->mem)) {
        return nullptr;
    }
    vk.vkBindImageMemory(vkapi::device, img->image, img->mem, 0);
    img->format = s.format;
    img->w = s.w;
    img->h = s.h;
    img->depth = ci.extent.depth;
    img->levels = s.levels;
    img->layers = ci.arrayLayers;
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = img->image;
    vi.viewType = s.kind == ImageKind::Tex3D ? VK_IMAGE_VIEW_TYPE_3D
                  : s.kind == ImageKind::Cube ? VK_IMAGE_VIEW_TYPE_CUBE
                                              : VK_IMAGE_VIEW_TYPE_2D;
    vi.format = s.format;
    bool depth = IsDepthFormat(s.format);
    vi.subresourceRange = {depth ? VkImageAspectFlags(VK_IMAGE_ASPECT_DEPTH_BIT) : VkImageAspectFlags(VK_IMAGE_ASPECT_COLOR_BIT),
                           0, s.levels, 0, ci.arrayLayers};
    if (vk.vkCreateImageView(vkapi::device, &vi, nullptr, &img->view) != VK_SUCCESS) return nullptr;
    img->attach = img->view;
    if (depth && HasStencil(s.format) && (s.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        if (vk.vkCreateImageView(vkapi::device, &vi, nullptr, &img->attach) != VK_SUCCESS) return nullptr;
    }
    return img;
}

struct Target {
    ImageP res;
    bool depth = false;
    uint32_t gw = 0, gh = 0, w = 0, h = 0;
    uint32_t data = 0, bytes = 0;
    uint64_t epoch = 0, sig = 0;  // last lock seen, guest memory as last matched
    bool drawn = false;           // the GPU wrote it since the last readback
    std::vector<uint8_t> read;    // what Sync left in guest memory
    ImageP fb;                    // copy read by draws that sample the target they render to
    VkFormat fmt = kBgra;
    float range = 1;
};

// Sampled hashes miss small text writes; compare every byte after Sync.
bool Changed(const Target& t) {
    if (t.drawn || t.read.empty()) return false;
    uint32_t row = t.gw * 4;
    uint32_t pitch = GuestPitch(t.gw);
    const uint8_t* p = static_cast<const uint8_t*>(kernel::HostPtr(t.data));
    for (uint32_t y = 0; y < t.gh; y++) {
        if (std::memcmp(p + size_t(y) * pitch, &t.read[size_t(y) * row], row)) return true;
    }
    return false;
}

// Read locks do not imply writes; upload only changed bytes.
bool LatchCpuWrite(Target& t) {
    uint64_t e = DirtyEpoch(t.data);
    if (e <= t.epoch) return false;
    t.epoch = e;
    uint64_t sig = MemSig(t.data, t.bytes);
    if (sig == t.sig && !Changed(t)) return false;
    t.sig = sig;
    return true;
}

struct Tex {
    ImageP res;
    TextureDesc d;
    VkFormat fmt = VK_FORMAT_UNDEFINED;
    int bc = 0;  // 1 to 3: BC blocks decoded on the CPU, the device lacks them
    uint32_t levels = 1;
    uint64_t sig = 0;
    uint64_t frame = 0;
    uint64_t palette = 0;  // hash of the colours a P8 texture was decoded with
    std::optional<texpack::Entry> rep;  // texture pack replacement this image holds instead of the game's texture
    uint64_t rep_hash = 0;
    std::shared_ptr<texpack::Resident> resident;  // keeps the replacement counted while this image holds it
};

struct PsoKey {
    uint32_t atlas_guard;
    uint64_t vs;
    uint64_t ps;
    uint32_t regs;
    uint32_t topo;
    uint32_t strip;
    uint32_t has_rt;
    uint32_t has_ds;
    uint32_t elements[16];  // per register: DXGI format, buffer << 8, offset << 16
    uint32_t buffers;
    uint32_t strides[16];
    uint32_t blend;
    uint32_t src;
    uint32_t dst;
    uint32_t op;
    uint32_t wmask;
    uint32_t zen;
    uint32_t zwrite;
    uint32_t zfunc;
    uint32_t sten;
    uint32_t sfunc;
    uint32_t smask;
    uint32_t swmask;
    uint32_t sfail;
    uint32_t szfail;
    uint32_t spass;
    uint32_t cull;
    uint32_t ccw;
    uint32_t fill;
    int32_t zbias;
    float zslope;
    uint32_t rtfmt;
};

struct BlitOptions {
    bool gamma = false;    // through the gamma lookup
    bool alpha = false;    // the source's alpha, else 1
    uint32_t taps = 1;     // n x n box filter per pixel
    float mul = 1;         // colour scale
    VkFormat rtv = kBgra;  // the target's format
    bool over = false;     // premultiplied over the target, which keeps its alpha
};

struct ShrinkOptions {
    uint32_t taps = 1;
    ImageP into;  // a copy from an earlier call to draw over again
    float mul = 1;
};

struct DrawTextures {
    TexInfo info[4];
    VkImageView view[4] = {};
    uint32_t levels[4] = {1, 1, 1, 1};
    bool bound[4] = {};
    bool rt[4] = {};            // stage samples a render target
    float k[4] = {1, 1, 1, 1};  // texels per render scale texel
    float wide[4] = {};         // host texels per console texel of a target read into a smaller one, 0 otherwise
};

struct DrawShaders {
    bool prog = false;  // a vertex program, else the fixed function pipeline
    bool quad = false;
    bool fixed_transform = false;  // fixed function on untransformed vertices: the shader reads matrices and lights
    FixedKey fk;
    uint64_t vkey = 0;
    uint64_t vh = 0;
    uint64_t pkey = 0;
    uint64_t ph = 0;
    VkShaderModule vs_code = VK_NULL_HANDLE;
    VkShaderModule ps_code = VK_NULL_HANDLE;
};

// the render target's console size in pixels, or the depth buffer's without one
struct GuestExtent {
    uint32_t w;
    uint32_t h;
};

struct Bindings {
    VkBuffer ubo[2] = {};
    VkDeviceSize offset[2] = {};
    VkDeviceSize range[2] = {};
    VkImageView image[4] = {};
    VkSampler sampler[4] = {};
};

struct Attachments {
    Image* color = nullptr;
    Image* depth = nullptr;
    uint32_t w = 0, h = 0;
    bool operator==(const Attachments& o) const {
        return color == o.color && depth == o.depth && w == o.w && h == o.h;
    }
};

class VulkanBackend final : public Backend, public openshao::pipelines::Builder {
public:
    ~VulkanBackend() override;
    bool BuildPipeline(const openshao::pipelines::Recipe& r, std::span<const uint32_t> program) override;
    bool DescribeCandidate(const openshao::pipelines::PipelineCandidate& c, openshao::pipelines::Recipe& r);
    bool Init(uint32_t width, uint32_t height) override;
    void Clear(const State& s, uint32_t count, const uint8_t* rects, uint32_t flags, uint32_t color, float z,
               uint32_t stencil) override;
    void Draw(const State& s, uint32_t prim, uint32_t start, uint32_t count, const uint8_t* indices) override;
    void Copy(const Surface& src, const Surface& dst, uint32_t count, uint32_t src_rects, uint32_t dst_points) override;
    void Present(const Surface& front) override;
    void Sync(uint32_t data, uint32_t size) override;
    void SyncLate(uint32_t data) override;
    bool DeliverLate(uint32_t data) override;
    void RecordLate(uint32_t data) override;
    void Flush() override { Submit(true); }
    void WaitGpuFrame(uint64_t frame) override {
        const uint64_t fence = present_fences_[frame % (kFrames + 1)].load(std::memory_order_acquire);
        if (fence) WaitFence(fence);
    }
    void SetGamma(const uint8_t* ramp) override;
    void ClearReport() override { counted_.clear(); }
    // Release only after the fence covering the recorded commands completes.
    void GetReport(uint32_t report) override { reports_.push_back({report, counted_, fence_value_ + 1}); }

private:
    struct Frame {
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        BufferP upload;
        uint64_t used = 0;
        uint64_t fence = 0;
        std::vector<std::shared_ptr<void>> garbage;
        std::vector<VkDescriptorPool> pools;
        size_t pool_next = 0;  // the descriptor pool sets come from
        std::vector<VkSampler> dead_samplers;
        VkSemaphore acquire = VK_NULL_HANDLE;
    };
    struct Alloc {
        VkBuffer buf;
        VkDeviceSize off;
        uint8_t* cpu;
    };

    Frame& CurrentFrame() { return frames_[frame_ % kFrames]; }
    VkCommandBuffer Cmd() { return CurrentFrame().cmd; }
    Alloc Upload(uint64_t size, uint64_t align = 256);
    void Transition(Image& img, VkImageLayout want);
    void Begin();
    void Submit(bool wait, VkSemaphore wait_sem = VK_NULL_HANDLE, VkSemaphore signal_sem = VK_NULL_HANDLE);
    void WaitFence(uint64_t value);
    uint64_t CompletedFence();
    void ResetFrameObjects(Frame& f);
    // Begin rendering just before drawing; end it before transitions or copies.
    void SetAttachments(Image* color, Image* depth, uint32_t w, uint32_t h) { pending_ = {color, depth, w, h}; }
    void EnsureRendering();
    void CloseRendering();
    void SetViewport(float x, float y, float w, float h);
    void SetScissor(int32_t x0, int32_t y0, int32_t x1, int32_t y1);
    void SetFullViewport(uint32_t w, uint32_t h);
    void SetBlitTarget(Image& img, uint32_t w, uint32_t h);
    VkDescriptorSet AllocSet();
    void BindSet(const Bindings& b);
    void Blit(Image& src, const float uvrect[4], const BlitOptions& o);
    void DrawFullScreen(VkPipeline pso, const Alloc& cb, const VkImageView views[4], const VkSampler samplers[4]);
    ImageP Feedback(Target& t);
    bool DepthCopy(const State& s);
    VkPipeline EnsureDepthCopyPipeline();
    void DrawDepthCopy(Target& src, Target& dst, VkPipeline pso);
    bool AssembleDraw(const State& s, uint32_t prim, uint32_t start, uint32_t count, const uint8_t* indices,
                      Assembled& a);
    bool BindTargets(const State& s);
    void EnsureVertexShader(const State& s, const Assembled& a, DrawShaders& sh);
    bool BindStageTexturesAndTargets(const State& s, DrawTextures& stages);
    void BindTexture(const State& s, int stage, DrawTextures& stages);
    void BindTargetTexture(const State& s, int stage, const TextureDesc& d, Target& t, bool box, DrawTextures& stages);
    void BindGuestTexture(const State& s, int stage, const TextureDesc& d, const FormatInfo& fi, DrawTextures& stages);
    bool CreateTexture(Tex& tx, const TextureDesc& d, const FormatInfo& fi, uint64_t key,
                       std::optional<texpack::Entry> rep = std::nullopt, uint64_t rep_hash = 0);
    bool UploadReplacement(Tex& tx);
    void UploadTexture(Tex& tx, const TextureDesc& d, const uint8_t* p, const uint32_t* colours);
    void UploadTextureLevel(Tex& tx, uint32_t f, uint32_t l, const std::vector<uint8_t>& px, uint32_t pitch);
    void EnsurePixelShader(const PixelKey& pk, DrawShaders& sh);
    void BuildPipelineKey(const State& s, const Assembled& a, uint64_t vh, uint64_t ph, PsoKey& k);
    void KeyDepthStencil(const State& s, const Assembled& a, PsoKey& k);
    VkPipeline EnsurePipeline(const PsoKey& k, VkShaderModule vs, VkShaderModule ps);
    // Thread safe: the render thread and the precompile workers share it.
    VkPipeline MakePipeline(const PsoKey& k, VkShaderModule vs, VkShaderModule ps, VkShaderModule points,
                            VkShaderModule atlas) const;
    float PointScale() const;
    void UploadConstants(const State& s, const DrawShaders& sh, const PsoKey& k, const DrawTextures& stages,
                         GuestExtent size, DrawConstants& c, Bindings& b);
    void FillTextureConstants(const State& s, const DrawTextures& stages, DrawConstants& c);
    void FillTexelConstants(int i, const TextureDesc& d, const DrawTextures& stages, DrawConstants& c);
    void SetTextureViewsAndSamplers(const State& s, const Assembled& a, DrawTextures& stages, Bindings& b);
    void EnsureStageSamplers(const State& s, const DrawTextures& stages, bool clamp, VkSampler out[4]);
    VkSampler EnsureSampler(const VkSamplerCreateInfo& ci);
    void SetViewportScissor(const State& s, uint32_t gw);
    void SetBlendFactorStencilRef(const State& s);
    void DrawGeometry(const Assembled& a);
    void UploadVertexBuffers(const Assembled& a);
    bool EnsureQueries();
    uint32_t BeginCountedDraw(const State& s);
    void EndCountedDraw(uint32_t query);
    float SamplesPerPixel() const;
    void DeliverReports();
    VkShaderModule Compile(const std::string& src, const char* entry, vkshader::Stage stage, uint64_t& hash);
    // VK_FORMAT_UNDEFINED means no color or depth attachment.
    struct PipelineParts {
        VkShaderModule vs = VK_NULL_HANDLE, gs = VK_NULL_HANDLE, ps = VK_NULL_HANDLE;
        const char* vs_entry = "main";
        const char* ps_entry = "main";
        VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        VkPipelineColorBlendAttachmentState blend{};
        VkFormat color = VK_FORMAT_UNDEFINED;
        VkFormat depth = VK_FORMAT_UNDEFINED;
        PipelineParts() {
            raster.polygonMode = VK_POLYGON_MODE_FILL;
            raster.cullMode = VK_CULL_MODE_NONE;
            raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
            raster.lineWidth = 1.0f;
            blend.colorWriteMask = 0xF;
        }
    };
    VkPipeline CreatePipeline(const PipelineParts& p) const;
    VkPipeline EnsureBlitPipeline(VkFormat rtv, bool over = false);
    bool CreateInstance();
    bool PickDevice();
    bool CreateDevice();
    bool CreateSwapchain();
    void DestroySwapchain();
    bool CreateLayouts();
    bool CreateFrames();
    bool CreateFixedImages();
    bool InitUi();
    void LoadPipelineCache();
    void SavePipelineCache();
    void ResizeSwap();
    void UploadGammaRamp();
    void DrawFrame(Target* t, uint32_t bi);
    void BlitFrame(Target& t);
    void RenderUi();
    void CaptureFrame(Target& target);
    void ReleaseOldTextures();
    void ResetFrame();
    void CountFps();
    void ApplySettings();
    ImageP Shrink(Image& src, uint32_t w, uint32_t h, const ShrinkOptions& o = {});
    bool ReadBack(Image& img, uint32_t w, uint32_t h, std::vector<uint8_t>& out);
    struct TargetSlot {
        uint32_t data;
        uint64_t key;
        uint32_t gw;
        uint32_t gh;
        std::unordered_map<uint64_t, Target>::iterator it;
        bool sized;  // the entry is there at the surface's size
    };
    Target* FindTarget(const Surface& s, bool depth);
    Target* EnsureTarget(const Surface& s, bool depth);
    TargetSlot FindTargetSlot(const Surface& s, bool depth);
    uint64_t SizedTargetKey(uint32_t data, bool depth, uint32_t gw, uint32_t gh) const;
    bool TargetStale(uint32_t data) const;
    void ReleaseTarget(std::unordered_map<uint64_t, Target>::iterator it);
    bool CreateTargetResource(Target& t);
    void UploadTarget(Target& t, const Surface& s);
    ImageP CreatePixelTexture(const std::vector<uint8_t>& px, uint32_t pitch, uint32_t w, uint32_t h);
    struct ClearArgs {
        uint32_t count;
        const uint8_t* rects;
        uint32_t flags;
        uint32_t color;
        float z;
        uint32_t stencil;
    };
    void ClearTarget(Target& t, const State& s, const ClearArgs& c, std::vector<VkClearRect>& rs);
    struct CopyList {
        uint32_t count;
        uint32_t src_rects;
        uint32_t dst_points;
    };
    void CopyByDraw(Target& src_t, Target& dst_t, const CopyList& l);
    void CopyByRegion(Target& src_t, Target& dst_t, const CopyList& l);
    struct CopySource {
        Image* res;
        ImageP down;
    };
    CopySource PrepareCopySource(Target& t, const ShrinkOptions& o);
    struct Late;
    void ResetLate(Late& l, const Target& t);
    Image* PrepareLateSource(Target& t, Late& l);
    void RecordLateCopy(Late& l, uint32_t slot, Image& src);

    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice gpu_ = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties gpu_props_{};
    uint32_t queue_family_ = 0;
    VkQueue queue_ = VK_NULL_HANDLE;
    VkDevice dev_ = VK_NULL_HANDLE;
    bool bc_ = false;          // textureCompressionBC
    bool aniso_ok_ = false;    // samplerAnisotropy
    bool wire_ok_ = false;     // fillModeNonSolid
    bool gs_ok_ = false;       // geometryShader
    bool precise_ok_ = false;  // occlusionQueryPrecise
    bool dynamic_ext_ = false;  // VK_KHR_dynamic_rendering enabled
    VkFormat depth_format_ = VK_FORMAT_D24_UNORM_S8_UINT;
    bool shadow_target_linear_ = false;
    bool shadow_texture_linear_ = false;
    VkSemaphore timeline_ = VK_NULL_HANDLE;
    uint64_t fence_value_ = 0;
    std::atomic<uint64_t> present_fences_[kFrames + 1]{};
    Frame frames_[kFrames];
    uint64_t frame_ = 0;
    VkSwapchainKHR swap_ = VK_NULL_HANDLE;
    VkFormat swap_format_ = kBgra;
    std::vector<ImageP> swap_images_;
    std::vector<VkSemaphore> swap_done_;  // per image: rendered, may be presented
    bool swap_dirty_ = false;
    bool swap_vsync_ = true;
    uint32_t bb_w_ = 0;
    uint32_t bb_h_ = 0;
    VkDescriptorSetLayout set_layout_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipelineCache cache_ = VK_NULL_HANDLE;
    uint64_t cache_saved_psos_ = 0;
    std::unordered_map<uint64_t, VkShaderModule> blobs_;
    std::unordered_map<uint64_t, uint64_t> shader_src_;  // vertex and pixel program keys to source hash
    std::unordered_map<uint64_t, VkPipeline> psos_;
    std::unordered_map<uint64_t, VkPipeline> blit_psos_;
    // Modules and pipelines built by precompile workers; pre_modules_ also lists the render thread's.
    std::mutex pre_mutex_;
    std::unordered_map<uint64_t, VkShaderModule> pre_modules_;
    std::unordered_map<uint64_t, VkPipeline> pre_psos_;
    std::unordered_set<uint64_t> pre_done_;  // pipelines made on either side
    openshao::pipelines::SourceHashes candidate_sources_;
    openshao::pipelines::InFlight pre_shader_flight_, pre_pso_flight_;
    bool pso_first_use_ = false;
    bool pso_compiled_on_draw_ = false;
    VkShaderModule CompileModule(const std::string& src, const char* entry, vkshader::Stage stage) const;
    VkShaderModule PrecompileModule(const std::string& src, const char* entry, vkshader::Stage stage, uint64_t& hash);
    void RecordPipeline(const State& s, const Assembled& a, const DrawShaders& sh, const PixelKey& pk, const PsoKey& k);
    std::unordered_map<uint64_t, VkSampler> samplers_;
    VkSampler blit_samplers_[4] = {};
    Attachments pending_;
    Attachments open_;
    bool rendering_ = false;
    std::unordered_map<uint64_t, Target> targets_;
    std::unordered_map<uint32_t, uint64_t> target_used_;  // target address to the frame it was last used
    std::unordered_map<uint64_t, Tex> textures_;  // by data address, a P8 texture's with its palette's above
    ImageP dummy_[4];  // views for stages without a texture, by TexKind
    ImageP lut_;
    uint8_t ramp_[768] = {};
    bool ramp_dirty_ = false;
    bool gamma_ = false;
    Target* bound_rt_ = nullptr;
    Target* bound_ds_ = nullptr;
    Target* draw_rt_ = nullptr;  // the draw's targets, the bound ones go when a texture's copy is drawn
    Target* draw_ds_ = nullptr;
    uint32_t guest_w_ = 640;
    uint32_t guest_h_ = 480;
    float scale_ = 1.0f;
    // For asynchronous locks, deliver only the newest completed GPU copy.
    struct Late {
        BufferP rb[kFrames];
        ImageP down;
        uint64_t fence[kFrames] = {};
        uint64_t frame[kFrames] = {};
        uint64_t delivered = 0;
        uint32_t gw = 0;
        uint32_t gh = 0;
        uint32_t next = 0;
    };
    std::unordered_map<uint32_t, Late> late_;
    std::mutex late_lock_;                    // DeliverLate runs on the title's thread
    std::atomic<uint64_t> presents_seen_{0};  // presents_ for DeliverLate
    Assembled asm_;                           // kept between draws for its buffers
    // Sum per-draw occlusion queries into guest report memory after GPU completion.
    struct Query {
        uint32_t index;
        float samples_per_pixel;  // host samples one console pixel covers
    };
    struct Report {
        uint32_t va;
        std::vector<Query> queries;
        uint64_t fence;
    };
    VkQueryPool query_pool_ = VK_NULL_HANDLE;
    bool queries_failed_ = false;
    uint32_t query_next_ = 0;
    std::vector<Query> counted_;  // since the last ClearReport
    std::deque<Report> reports_;
    VkDescriptorPool ui_pool_ = VK_NULL_HANDLE;
    bool ui_ = false;
    PerfClock::time_point ui_t_ = PerfClock::now();
    uint64_t presents_ = 0;
    double fps_ = 0;
    PerfClock::time_point fps_t_ = PerfClock::now();
    uint64_t fps_n_ = 0;
};


VulkanBackend::~VulkanBackend() {
    openshao::pipelines::ReportAssetPipelines();
    openshao::pipelines::ClearCandidateDescriber();
    openshao::pipelines::Shutdown();
    if (!dev_) {
        if (surface_) vk.vkDestroySurfaceKHR(instance_, surface_, nullptr);
        if (instance_) vk.vkDestroyInstance(instance_, nullptr);
        return;
    }
    vk.vkDeviceWaitIdle(dev_);
    SavePipelineCache();
    if (ui_) ImGui_ImplVulkan_Shutdown();
    targets_.clear();
    textures_.clear();
    late_.clear();
    for (auto& d : dummy_) d.reset();
    lut_.reset();
    for (auto& f : frames_) {
        f.garbage.clear();
        f.upload.reset();
        for (VkSampler s : f.dead_samplers) vk.vkDestroySampler(dev_, s, nullptr);
        for (VkDescriptorPool p : f.pools) vk.vkDestroyDescriptorPool(dev_, p, nullptr);
        if (f.pool) vk.vkDestroyCommandPool(dev_, f.pool, nullptr);
        if (f.acquire) vk.vkDestroySemaphore(dev_, f.acquire, nullptr);
    }
    for (auto& [k, p] : psos_) if (p) vk.vkDestroyPipeline(dev_, p, nullptr);
    for (auto& [k, p] : pre_psos_) if (p) vk.vkDestroyPipeline(dev_, p, nullptr);
    for (auto& [k, p] : blit_psos_) if (p) vk.vkDestroyPipeline(dev_, p, nullptr);
    std::unordered_set<VkShaderModule> modules;
    for (auto& [k, m] : blobs_) if (m) modules.insert(m);
    for (auto& [k, m] : pre_modules_) if (m) modules.insert(m);
    for (VkShaderModule m : modules) vk.vkDestroyShaderModule(dev_, m, nullptr);
    for (auto& [k, s] : samplers_) vk.vkDestroySampler(dev_, s, nullptr);
    for (VkSampler s : blit_samplers_) if (s) vk.vkDestroySampler(dev_, s, nullptr);
    if (query_pool_) vk.vkDestroyQueryPool(dev_, query_pool_, nullptr);
    if (ui_pool_) vk.vkDestroyDescriptorPool(dev_, ui_pool_, nullptr);
    if (cache_) vk.vkDestroyPipelineCache(dev_, cache_, nullptr);
    if (layout_) vk.vkDestroyPipelineLayout(dev_, layout_, nullptr);
    if (set_layout_) vk.vkDestroyDescriptorSetLayout(dev_, set_layout_, nullptr);
    DestroySwapchain();
    if (timeline_) vk.vkDestroySemaphore(dev_, timeline_, nullptr);
    vk.vkDestroyDevice(dev_, nullptr);
    vk.vkDestroySurfaceKHR(instance_, surface_, nullptr);
    vk.vkDestroyInstance(instance_, nullptr);
}

bool VulkanBackend::Init(uint32_t width, uint32_t height) {
    guest_w_ = width;
    guest_h_ = height;
    if (!vkapi::LoadGlobal() || !CreateInstance() || !PickDevice() || !CreateDevice()) return false;
    if (!CreateLayouts() || !CreateFrames() || !CreateFixedImages()) return false;
    swap_vsync_ = app::Get().vsync;
    if (!CreateSwapchain()) return false;
    LoadPipelineCache();
    InitUi();
    scale_ = app::RenderScale(guest_h_);
    Begin();
    LOGI("vulkan: %ux%u guest, %ux%u window, render scale %.3f", width, height, bb_w_, bb_h_, scale_);
    openshao::pipelines::Start(openshao::paths::Cache() / "vulkan_recipes.bin", 0x5C0000000000ull | sizeof(PsoKey), this);
    openshao::pipelines::SetCandidateDescriber([this](const auto& c, auto& r) { return DescribeCandidate(c, r); });
    openshao::pipelines::SetPipelineRenderScale(scale_);
    openshao::pipelines::WarmCommonScreenPipelines();
    return true;
}

bool VulkanBackend::CreateInstance() {
    auto ext=vkplatform::SurfaceExtensions();
    if(ext.empty())return false;
    uint32_t api = VK_API_VERSION_1_0;
    if (auto enumerate = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
            vk.vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"))) {
        enumerate(&api);
    }
    if (api < VK_API_VERSION_1_1) {
        LOGE("vulkan: the Vulkan loader is older than 1.1");
        return false;
    }
    VkApplicationInfo app_info{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app_info.pApplicationName = "OpenShao";
    app_info.pEngineName = "XboxXBERecomp";
    app_info.apiVersion = std::min<uint32_t>(api, VK_API_VERSION_1_3);
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app_info;
    ci.enabledExtensionCount = uint32_t(ext.size());
    ci.ppEnabledExtensionNames = ext.data();
    const char* layer = "VK_LAYER_KHRONOS_validation";
    const char* validate = std::getenv("OPENSHAO_VK_VALIDATION");
    VkResult r = VK_ERROR_INITIALIZATION_FAILED;
    if (validate && *validate == '1') {
        ci.enabledLayerCount = 1;
        ci.ppEnabledLayerNames = &layer;
        r = vk.vkCreateInstance(&ci, nullptr, &instance_);
        if (r != VK_SUCCESS) LOGI("vulkan: validation layer unavailable");
        else LOGI("vulkan: validation layer on");
    }
    if (r != VK_SUCCESS) {
        ci.enabledLayerCount = 0;
        r = vk.vkCreateInstance(&ci, nullptr, &instance_);
    }
    if (r != VK_SUCCESS) {
        LOGE("vulkan: vkCreateInstance failed: %s", vkapi::ResultName(r));
        return false;
    }
    if (!vkapi::LoadInstance(instance_)) return false;
    if(!vkplatform::CreateSurface(instance_,surface_))return false;
    return true;
}

bool HasExtension(const std::vector<VkExtensionProperties>& list, const char* name) {
    for (const auto& e : list) {
        if (!std::strcmp(e.extensionName, name)) return true;
    }
    return false;
}

bool VulkanBackend::PickDevice() {
    uint32_t n = 0;
    vk.vkEnumeratePhysicalDevices(instance_, &n, nullptr);
    std::vector<VkPhysicalDevice> devices(n);
    vk.vkEnumeratePhysicalDevices(instance_, &n, devices.data());
    const char* want = std::getenv("OPENSHAO_VK_DEVICE");  // part of a device name
    int best_score = -1;
    for (VkPhysicalDevice d : devices) {
        VkPhysicalDeviceProperties p;
        vk.vkGetPhysicalDeviceProperties(d, &p);
        uint32_t en = 0;
        vk.vkEnumerateDeviceExtensionProperties(d, nullptr, &en, nullptr);
        std::vector<VkExtensionProperties> ext(en);
        vk.vkEnumerateDeviceExtensionProperties(d, nullptr, &en, ext.data());
        bool dynamic = p.apiVersion >= VK_API_VERSION_1_3 || HasExtension(ext, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        bool usable = p.apiVersion >= VK_API_VERSION_1_2 && dynamic && HasExtension(ext, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        uint32_t qn = 0;
        vk.vkGetPhysicalDeviceQueueFamilyProperties(d, &qn, nullptr);
        std::vector<VkQueueFamilyProperties> qf(qn);
        vk.vkGetPhysicalDeviceQueueFamilyProperties(d, &qn, qf.data());
        int family = -1;
        for (uint32_t i = 0; i < qn && family < 0; i++) {
            VkBool32 present = VK_FALSE;
            vk.vkGetPhysicalDeviceSurfaceSupportKHR(d, i, surface_, &present);
            if ((qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) family = int(i);
        }
        LOGI("vulkan: device %s, Vulkan %u.%u.%u%s", p.deviceName, VK_API_VERSION_MAJOR(p.apiVersion),
             VK_API_VERSION_MINOR(p.apiVersion), VK_API_VERSION_PATCH(p.apiVersion),
             usable && family >= 0 ? "" : " (not usable: needs Vulkan 1.2, dynamic rendering and a present queue)");
        if (!usable || family < 0) continue;
        int score = 0;
        switch (p.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: score = 40; break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: score = 30; break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: score = 20; break;
            default: score = 10; break;
        }
        if (want && std::strstr(p.deviceName, want)) score += 100;
        if (score > best_score) {
            best_score = score;
            gpu_ = d;
            gpu_props_ = p;
            queue_family_ = uint32_t(family);
            dynamic_ext_ = HasExtension(ext, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        }
    }
    if (!gpu_) {
        LOGE("vulkan: no usable device");
        return false;
    }
    LOGR("Vulkan device: %s (driver %08X)", gpu_props_.deviceName, gpu_props_.driverVersion);
    return true;
}

bool VulkanBackend::CreateDevice() {
    VkPhysicalDeviceDynamicRenderingFeatures dyn{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES};
    VkPhysicalDeviceTimelineSemaphoreFeatures timeline{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES};
    timeline.pNext = &dyn;
    VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    f2.pNext = &timeline;
    vk.vkGetPhysicalDeviceFeatures2(gpu_, &f2);
    if (!timeline.timelineSemaphore || !dyn.dynamicRendering) {
        LOGE("vulkan: %s lacks timeline semaphores or dynamic rendering", gpu_props_.deviceName);
        return false;
    }
    const VkPhysicalDeviceFeatures& have = f2.features;
    VkPhysicalDeviceFeatures on{};
    on.geometryShader = gs_ok_ = have.geometryShader;
    on.samplerAnisotropy = aniso_ok_ = have.samplerAnisotropy;
    on.fillModeNonSolid = wire_ok_ = have.fillModeNonSolid;
    on.textureCompressionBC = have.textureCompressionBC;
    on.occlusionQueryPrecise = precise_ok_ = have.occlusionQueryPrecise;
    VkFormatProperties fp;
    vk.vkGetPhysicalDeviceFormatProperties(gpu_, VK_FORMAT_BC3_UNORM_BLOCK, &fp);
    bc_ = have.textureCompressionBC && (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
    if (!gs_ok_) LOGE("vulkan: no geometry shaders, points and sprite edges draw as the shaders give them");
    vk.vkGetPhysicalDeviceFormatProperties(gpu_, VK_FORMAT_D24_UNORM_S8_UINT, &fp);
    const VkFormatFeatureFlags depth_need =
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    depth_format_ = (fp.optimalTilingFeatures & depth_need) == depth_need ? VK_FORMAT_D24_UNORM_S8_UINT
                                                                          : VK_FORMAT_D32_SFLOAT_S8_UINT;
    vk.vkGetPhysicalDeviceFormatProperties(gpu_, depth_format_, &fp);
    shadow_target_linear_ = (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
    vk.vkGetPhysicalDeviceFormatProperties(gpu_, VK_FORMAT_D32_SFLOAT, &fp);
    shadow_texture_linear_ = (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
    dyn.pNext = nullptr;
    timeline.pNext = &dyn;
    f2.pNext = &timeline;
    f2.features = on;
    float priority = 1.0f;
    VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qi.queueFamilyIndex = queue_family_;
    qi.queueCount = 1;
    qi.pQueuePriorities = &priority;
    std::vector<const char*> ext = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    if (dynamic_ext_) ext.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    VkDeviceCreateInfo ci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    ci.pNext = &f2;
    ci.queueCreateInfoCount = 1;
    ci.pQueueCreateInfos = &qi;
    ci.enabledExtensionCount = uint32_t(ext.size());
    ci.ppEnabledExtensionNames = ext.data();
    VkResult r = vk.vkCreateDevice(gpu_, &ci, nullptr, &dev_);
    if (r != VK_SUCCESS) {
        LOGE("vulkan: vkCreateDevice failed: %s", vkapi::ResultName(r));
        return false;
    }
    if (!vkapi::LoadDevice(dev_)) return false;
    vk.vkGetDeviceQueue(dev_, queue_family_, 0, &queue_);
    vk.vkGetPhysicalDeviceMemoryProperties(gpu_, &g_memory_props);
    VkSemaphoreTypeCreateInfo st{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
    st.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    si.pNext = &st;
    if (vk.vkCreateSemaphore(dev_, &si, nullptr, &timeline_) != VK_SUCCESS) return false;
    LOGI("vulkan: depth %s, BC textures %s, anisotropy %s", depth_format_ == VK_FORMAT_D24_UNORM_S8_UINT ? "D24S8" : "D32FS8",
         bc_ ? "on the GPU" : "decoded on the CPU", aniso_ok_ ? "yes" : "no");
    return true;
}

bool VulkanBackend::CreateSwapchain() {
    int pw = 0, ph = 0;
    vkplatform::WindowSize(pw,ph);
    VkSurfaceCapabilitiesKHR caps;
    if (vk.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu_, surface_, &caps) != VK_SUCCESS) return false;
    VkExtent2D extent = caps.currentExtent;
    if (extent.width == 0xFFFFFFFFu) {
        extent.width = std::clamp<uint32_t>(uint32_t(std::max(pw, 1)), caps.minImageExtent.width, caps.maxImageExtent.width);
        extent.height = std::clamp<uint32_t>(uint32_t(std::max(ph, 1)), caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    if (!extent.width || !extent.height) {
        swap_dirty_ = true;  // minimised: made once the window has a size again
        return true;
    }
    uint32_t n = 0;
    vk.vkGetPhysicalDeviceSurfaceFormatsKHR(gpu_, surface_, &n, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(n);
    vk.vkGetPhysicalDeviceSurfaceFormatsKHR(gpu_, surface_, &n, formats.data());
    VkSurfaceFormatKHR format = formats.empty() ? VkSurfaceFormatKHR{kBgra, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR} : formats[0];
    for (VkFormat want : {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM}) {
        auto it = std::find_if(formats.begin(), formats.end(), [&](const VkSurfaceFormatKHR& f) {
            return f.format == want && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        });
        if (it != formats.end()) {
            format = *it;
            break;
        }
    }
    vk.vkGetPhysicalDeviceSurfacePresentModesKHR(gpu_, surface_, &n, nullptr);
    std::vector<VkPresentModeKHR> modes(n);
    vk.vkGetPhysicalDeviceSurfacePresentModesKHR(gpu_, surface_, &n, modes.data());
    auto has = [&](VkPresentModeKHR m) { return std::find(modes.begin(), modes.end(), m) != modes.end(); };
    VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;
    if (!swap_vsync_) {
        if (has(VK_PRESENT_MODE_MAILBOX_KHR)) mode = VK_PRESENT_MODE_MAILBOX_KHR;
        else if (has(VK_PRESENT_MODE_IMMEDIATE_KHR)) mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    }
    uint32_t images = std::max<uint32_t>(caps.minImageCount + 1, 3);
    if (caps.maxImageCount) images = std::min(images, caps.maxImageCount);
    VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (!(caps.supportedCompositeAlpha & alpha)) {
        for (uint32_t bit = 1; bit; bit <<= 1) {
            if (caps.supportedCompositeAlpha & bit) {
                alpha = VkCompositeAlphaFlagBitsKHR(bit);
                break;
            }
        }
    }
    VkSwapchainKHR old = swap_;
    VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface = surface_;
    ci.minImageCount = images;
    ci.imageFormat = format.format;
    ci.imageColorSpace = format.colorSpace;
    ci.imageExtent = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = alpha;
    ci.presentMode = mode;
    ci.clipped = VK_TRUE;
    ci.oldSwapchain = old;
    VkResult r = vk.vkCreateSwapchainKHR(dev_, &ci, nullptr, &swap_);
    if (old) {
        swap_images_.clear();
        for (VkSemaphore s : swap_done_) vk.vkDestroySemaphore(dev_, s, nullptr);
        swap_done_.clear();
        vk.vkDestroySwapchainKHR(dev_, old, nullptr);
    }
    if (r != VK_SUCCESS) {
        swap_ = VK_NULL_HANDLE;
        LOGE("vulkan: no swap chain: %s", vkapi::ResultName(r));
        return false;
    }
    swap_format_ = format.format;
    bb_w_ = extent.width;
    bb_h_ = extent.height;
    vk.vkGetSwapchainImagesKHR(dev_, swap_, &n, nullptr);
    std::vector<VkImage> list(n);
    vk.vkGetSwapchainImagesKHR(dev_, swap_, &n, list.data());
    for (VkImage handle : list) {
        auto img = std::make_shared<Image>();
        img->owned = false;
        img->image = handle;
        img->format = swap_format_;
        img->w = bb_w_;
        img->h = bb_h_;
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = handle;
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = swap_format_;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vk.vkCreateImageView(dev_, &vi, nullptr, &img->view);
        img->attach = img->view;
        swap_images_.push_back(img);
        VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkSemaphore s = VK_NULL_HANDLE;
        vk.vkCreateSemaphore(dev_, &si, nullptr, &s);
        swap_done_.push_back(s);
    }
    swap_dirty_ = false;
    LOGI("vulkan: swap chain %ux%u, %u images, %s", bb_w_, bb_h_, n,
         mode == VK_PRESENT_MODE_FIFO_KHR ? "vsync" : mode == VK_PRESENT_MODE_MAILBOX_KHR ? "mailbox" : "immediate");
    return true;
}

void VulkanBackend::DestroySwapchain() {
    swap_images_.clear();
    for (VkSemaphore s : swap_done_) vk.vkDestroySemaphore(dev_, s, nullptr);
    swap_done_.clear();
    if (swap_) vk.vkDestroySwapchainKHR(dev_, swap_, nullptr);
    swap_ = VK_NULL_HANDLE;
}

bool VulkanBackend::CreateLayouts() {
    VkDescriptorSetLayoutBinding b[10] = {};
    for (uint32_t i = 0; i < 10; i++) {
        b[i].binding = i;
        b[i].descriptorCount = 1;
        if (i < 2) {
            b[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            b[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            if (gs_ok_) b[i].stageFlags |= VK_SHADER_STAGE_GEOMETRY_BIT;
        } else {
            b[i].descriptorType = i < 6 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER;
            b[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        }
    }
    VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    li.bindingCount = 10;
    li.pBindings = b;
    if (vk.vkCreateDescriptorSetLayout(dev_, &li, nullptr, &set_layout_) != VK_SUCCESS) return false;
    VkPipelineLayoutCreateInfo pi{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pi.setLayoutCount = 1;
    pi.pSetLayouts = &set_layout_;
    if (vk.vkCreatePipelineLayout(dev_, &pi, nullptr, &layout_) != VK_SUCCESS) return false;
    // four clamp samplers for the full screen passes: linear at slot 0, point at slots 1 to 3
    for (int i = 0; i < 4; i++) {
        VkSamplerCreateInfo sc{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sc.magFilter = sc.minFilter = i == 0 ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        sc.mipmapMode = i == 0 ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sc.addressModeU = sc.addressModeV = sc.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sc.maxLod = VK_LOD_CLAMP_NONE;
        if (vk.vkCreateSampler(dev_, &sc, nullptr, &blit_samplers_[i]) != VK_SUCCESS) return false;
    }
    return true;
}

bool VulkanBackend::CreateFrames() {
    for (auto& f : frames_) {
        VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pi.queueFamilyIndex = queue_family_;
        if (vk.vkCreateCommandPool(dev_, &pi, nullptr, &f.pool) != VK_SUCCESS) return false;
        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool = f.pool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        if (vk.vkAllocateCommandBuffers(dev_, &ai, &f.cmd) != VK_SUCCESS) return false;
        f.upload = CreateBuffer(kUploadSize,
                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                false);
        if (!f.upload) {
            LOGE("vulkan: no upload memory");
            return false;
        }
        VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        if (vk.vkCreateSemaphore(dev_, &si, nullptr, &f.acquire) != VK_SUCCESS) return false;
    }
    return true;
}

bool VulkanBackend::CreateFixedImages() {
    VkCommandBuffer cmd = frames_[0].cmd;
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vk.vkBeginCommandBuffer(cmd, &bi);
    ImageSpec spec;
    dummy_[kTexNone] = dummy_[kTex2D] = CreateImage(spec);
    spec.kind = ImageKind::Tex3D;
    dummy_[kTex3D] = CreateImage(spec);
    spec.kind = ImageKind::Cube;
    dummy_[kTexCube] = CreateImage(spec);
    spec = ImageSpec{};
    spec.w = 256;
    lut_ = CreateImage(spec);
    for (Image* img : {dummy_[kTex2D].get(), dummy_[kTex3D].get(), dummy_[kTexCube].get(), lut_.get()}) {
        if (!img) return false;
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = img->image;
        b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS};
        b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vk.vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                                nullptr, 1, &b);
        VkClearColorValue zero{};
        vk.vkCmdClearColorImage(cmd, img->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &zero, 1, &b.subresourceRange);
        b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vk.vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
                                nullptr, 1, &b);
        img->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    vk.vkEndCommandBuffer(cmd);
    VkTimelineSemaphoreSubmitInfo ts{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    uint64_t value = ++fence_value_;
    ts.signalSemaphoreValueCount = 1;
    ts.pSignalSemaphoreValues = &value;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.pNext = &ts;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &timeline_;
    if (vk.vkQueueSubmit(queue_, 1, &si, VK_NULL_HANDLE) != VK_SUCCESS) return false;
    WaitFence(value);
    vk.vkResetCommandPool(dev_, frames_[0].pool, 0);
    return true;
}

PFN_vkVoidFunction UiLoader(const char* name, void* user) {
    auto instance = static_cast<VkInstance>(user);
    PFN_vkVoidFunction f = vk.vkGetInstanceProcAddr(instance, name);
    // the overlay asks for the KHR names of functions core in 1.3
    if (!f) {
        std::string core = name;
        if (core.size() > 3 && core.compare(core.size() - 3, 3, "KHR") == 0) {
            core.resize(core.size() - 3);
            f = vk.vkGetInstanceProcAddr(instance, core.c_str());
        }
    }
    return f;
}

bool VulkanBackend::InitUi() {
    if (openshao::options.background) return false;
    if (!ImGui::GetCurrentContext()) {
        ImGui::CreateContext();
        ImGui::GetIO().IniFilename = nullptr;
    }
    VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16};
    VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pi.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pi.maxSets = 16;
    pi.poolSizeCount = 1;
    pi.pPoolSizes = &size;
    if (vk.vkCreateDescriptorPool(dev_, &pi, nullptr, &ui_pool_) != VK_SUCCESS) return false;
    if (!ImGui_ImplVulkan_LoadFunctions(UiLoader, instance_)) {
        LOGE("vulkan: overlay functions missing, no overlay");
        return false;
    }
    ImGui_ImplVulkan_InitInfo ii{};
    ii.Instance = instance_;
    ii.PhysicalDevice = gpu_;
    ii.Device = dev_;
    ii.QueueFamily = queue_family_;
    ii.Queue = queue_;
    ii.DescriptorPool = ui_pool_;
    ii.MinImageCount = 2;
    ii.ImageCount = std::max<uint32_t>(uint32_t(swap_images_.size()), 2);
    ii.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ii.PipelineCache = cache_;
    ii.UseDynamicRendering = true;
    ii.PipelineRenderingCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    ii.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    ii.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swap_format_;
    if (!ImGui_ImplVulkan_Init(&ii)) return false;
    ImGui_ImplVulkan_CreateFontsTexture();
    ui_ = true;
    return true;
}

std::filesystem::path PipelineCachePath() { return openshao::paths::Cache() / "vulkan_pipelines.bin"; }

void VulkanBackend::LoadPipelineCache() {
    std::vector<char> data;
    std::error_code ec;
    std::filesystem::path path = PipelineCachePath();
    if (std::filesystem::exists(path, ec)) {
        std::ifstream in(path, std::ios::binary);
        data.assign(std::istreambuf_iterator<char>(in), {});
    }
    VkPipelineCacheCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    // Reject caches from other drivers or devices and start with an empty cache.
    ci.initialDataSize = data.size();
    ci.pInitialData = data.empty() ? nullptr : data.data();
    if (vk.vkCreatePipelineCache(dev_, &ci, nullptr, &cache_) != VK_SUCCESS) {
        ci.initialDataSize = 0;
        ci.pInitialData = nullptr;
        vk.vkCreatePipelineCache(dev_, &ci, nullptr, &cache_);
    }
}

void VulkanBackend::SavePipelineCache() {
    if (!cache_) return;
    size_t size = 0;
    if (vk.vkGetPipelineCacheData(dev_, cache_, &size, nullptr) != VK_SUCCESS || !size) return;
    std::vector<char> data(size);
    if (vk.vkGetPipelineCacheData(dev_, cache_, &size, data.data()) != VK_SUCCESS) return;
    std::error_code ec;
    std::filesystem::path path = PipelineCachePath();
    std::filesystem::create_directories(path.parent_path(), ec);
    std::filesystem::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out.write(data.data(), std::streamsize(size));
        if (!out) return;
    }
    std::filesystem::rename(tmp, path, ec);
}


void VulkanBackend::Begin() {
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vk.vkBeginCommandBuffer(Cmd(), &bi);
    bound_rt_ = bound_ds_ = nullptr;
    pending_ = open_ = {};
    rendering_ = false;
}

uint64_t VulkanBackend::CompletedFence() {
    uint64_t v = 0;
    vk.vkGetSemaphoreCounterValue(dev_, timeline_, &v);
    return v;
}

void VulkanBackend::WaitFence(uint64_t value) {
    VkSemaphoreWaitInfo wi{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
    wi.semaphoreCount = 1;
    wi.pSemaphores = &timeline_;
    wi.pValues = &value;
    VkResult r = vk.vkWaitSemaphores(dev_, &wi, UINT64_MAX);
    if (r != VK_SUCCESS) LOG_FIRST(4, "vulkan: fence wait failed: %s", vkapi::ResultName(r));
}

void VulkanBackend::ResetFrameObjects(Frame& f) {
    vk.vkResetCommandPool(dev_, f.pool, 0);
    for (VkDescriptorPool p : f.pools) vk.vkResetDescriptorPool(dev_, p, 0);
    f.pool_next = 0;
    f.garbage.clear();
    for (VkSampler s : f.dead_samplers) vk.vkDestroySampler(dev_, s, nullptr);
    f.dead_samplers.clear();
    f.used = 0;
}

void VulkanBackend::Submit(bool wait, VkSemaphore wait_sem, VkSemaphore signal_sem) {
    CloseRendering();
    Frame& f = CurrentFrame();
    // A completed fence makes GPU readbacks and query results visible to the CPU.
    VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    mb.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vk.vkCmdPipelineBarrier(f.cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, nullptr,
                            0, nullptr);
    vk.vkEndCommandBuffer(f.cmd);
    uint64_t values[2] = {++fence_value_, 0};
    VkSemaphore signals[2] = {timeline_, signal_sem};
    uint64_t wait_value = 0;
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkTimelineSemaphoreSubmitInfo ts{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    ts.signalSemaphoreValueCount = signal_sem ? 2 : 1;
    ts.pSignalSemaphoreValues = values;
    ts.waitSemaphoreValueCount = wait_sem ? 1 : 0;
    ts.pWaitSemaphoreValues = &wait_value;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.pNext = &ts;
    si.waitSemaphoreCount = wait_sem ? 1 : 0;
    si.pWaitSemaphores = &wait_sem;
    si.pWaitDstStageMask = &wait_stage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &f.cmd;
    si.signalSemaphoreCount = signal_sem ? 2 : 1;
    si.pSignalSemaphores = signals;
    VkResult r = vk.vkQueueSubmit(queue_, 1, &si, VK_NULL_HANDLE);
    if (r != VK_SUCCESS) LOG_FIRST(4, "vulkan: submit failed: %s", vkapi::ResultName(r));
    f.fence = fence_value_;
    if (wait) {
        WaitFence(fence_value_);
        ResetFrameObjects(f);
        Begin();
    }
}

VulkanBackend::Alloc VulkanBackend::Upload(uint64_t size, uint64_t align) {
    Frame& f = CurrentFrame();
    g_perf.upload += size;
    uint64_t o = (f.used + align - 1) & ~(align - 1);
    if (o + size <= kUploadSize) {
        f.used = o + size;
        return {f.upload->buf, o, f.upload->map + o};
    }
    // too big for the ring: a buffer of its own, released with the frame
    g_perf.overflow++;
    BufferP b = CreateBuffer(std::max<uint64_t>(size, 256),
                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                             false);
    if (!b) {
        LOG_FIRST(4, "vulkan: upload of %llu bytes failed", (unsigned long long)size);
        static std::vector<uint8_t> sink;
        sink.resize(size);
        return {f.upload->buf, 0, sink.data()};
    }
    f.garbage.push_back(b);
    return {b->buf, 0, b->map};
}

void VulkanBackend::Transition(Image& img, VkImageLayout want) {
    if (img.layout == want) return;
    CloseRendering();
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    b.oldLayout = img.layout;
    b.newLayout = want;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img.image;
    b.subresourceRange = {img.Aspects(), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS};
    vk.vkCmdPipelineBarrier(Cmd(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr,
                            0, nullptr, 1, &b);
    img.layout = want;
}

void VulkanBackend::EnsureRendering() {
    if (rendering_ && open_ == pending_) return;
    CloseRendering();
    // Insert a barrier when later rendering instances reuse written attachments.
    VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    mb.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    mb.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                       VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    VkPipelineStageFlags stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    vk.vkCmdPipelineBarrier(Cmd(), stages, stages, 0, 1, &mb, 0, nullptr, 0, nullptr);
    VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    VkRenderingAttachmentInfo depth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
    ri.renderArea = {{0, 0}, {pending_.w, pending_.h}};
    ri.layerCount = 1;
    if (pending_.color) {
        color.imageView = pending_.color->attach;
        color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        ri.colorAttachmentCount = 1;
        ri.pColorAttachments = &color;
    }
    if (pending_.depth) {
        depth.imageView = pending_.depth->attach;
        depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        ri.pDepthAttachment = &depth;
        if (HasStencil(pending_.depth->format)) ri.pStencilAttachment = &depth;
    }
    vk.vkCmdBeginRendering(Cmd(), &ri);
    rendering_ = true;
    open_ = pending_;
}

void VulkanBackend::CloseRendering() {
    if (!rendering_) return;
    vk.vkCmdEndRendering(Cmd());
    rendering_ = false;
}

// the D3D viewport, flipped: a negative height keeps D3D's clip space and winding
void VulkanBackend::SetViewport(float x, float y, float w, float h) {
    VkViewport vp{x, y + h, w, -h, 0, 1};
    vk.vkCmdSetViewport(Cmd(), 0, 1, &vp);
}

void VulkanBackend::SetScissor(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    VkRect2D r{{std::max(x0, 0), std::max(y0, 0)}, {uint32_t(std::max(x1 - std::max(x0, 0), 0)),
                                                    uint32_t(std::max(y1 - std::max(y0, 0), 0))}};
    vk.vkCmdSetScissor(Cmd(), 0, 1, &r);
}

void VulkanBackend::SetFullViewport(uint32_t w, uint32_t h) {
    SetViewport(0, 0, float(w), float(h));
    SetScissor(0, 0, int32_t(w), int32_t(h));
}

void VulkanBackend::SetBlitTarget(Image& img, uint32_t w, uint32_t h) {
    Transition(img, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    SetAttachments(&img, nullptr, w, h);
    bound_rt_ = bound_ds_ = nullptr;
    SetFullViewport(w, h);
}

VkDescriptorSet VulkanBackend::AllocSet() {
    Frame& f = CurrentFrame();
    for (;;) {
        if (f.pool_next == f.pools.size()) {
            VkDescriptorPoolSize sizes[3] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 * kSetsPerPool},
                                             {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4 * kSetsPerPool},
                                             {VK_DESCRIPTOR_TYPE_SAMPLER, 4 * kSetsPerPool}};
            VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            pi.maxSets = kSetsPerPool;
            pi.poolSizeCount = 3;
            pi.pPoolSizes = sizes;
            VkDescriptorPool p = VK_NULL_HANDLE;
            if (vk.vkCreateDescriptorPool(dev_, &pi, nullptr, &p) != VK_SUCCESS) return VK_NULL_HANDLE;
            f.pools.push_back(p);
        }
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = f.pools[f.pool_next];
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &set_layout_;
        VkDescriptorSet set = VK_NULL_HANDLE;
        if (vk.vkAllocateDescriptorSets(dev_, &ai, &set) == VK_SUCCESS) return set;
        f.pool_next++;  // full: the next pool, made when there is none
    }
}

void VulkanBackend::BindSet(const Bindings& b) {
    VkDescriptorSet set = AllocSet();
    if (!set) return;
    VkDescriptorBufferInfo bufs[2];
    VkDescriptorImageInfo imgs[4];
    VkDescriptorImageInfo smps[4];
    VkWriteDescriptorSet w[10];
    for (uint32_t i = 0; i < 10; i++) {
        w[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w[i].dstSet = set;
        w[i].dstBinding = i;
        w[i].descriptorCount = 1;
        if (i < 2) {
            bufs[i] = {b.ubo[i], b.offset[i], b.range[i]};
            w[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            w[i].pBufferInfo = &bufs[i];
        } else if (i < 6) {
            VkImageView v = b.image[i - 2] ? b.image[i - 2] : dummy_[kTex2D]->view;
            imgs[i - 2] = {VK_NULL_HANDLE, v, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            w[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            w[i].pImageInfo = &imgs[i - 2];
        } else {
            smps[i - 6] = {b.sampler[i - 6] ? b.sampler[i - 6] : blit_samplers_[1], VK_NULL_HANDLE,
                           VK_IMAGE_LAYOUT_UNDEFINED};
            w[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            w[i].pImageInfo = &smps[i - 6];
        }
    }
    vk.vkUpdateDescriptorSets(dev_, 10, w, 0, nullptr);
    vk.vkCmdBindDescriptorSets(Cmd(), VK_PIPELINE_BIND_POINT_GRAPHICS, layout_, 0, 1, &set, 0, nullptr);
}


void VulkanBackend::ReleaseTarget(std::unordered_map<uint64_t, Target>::iterator it) {
    if (bound_rt_ == &it->second || bound_ds_ == &it->second) bound_rt_ = bound_ds_ = nullptr;
    Image* img = it->second.res.get();
    if (pending_.color == img || pending_.depth == img) pending_ = {};
    if (rendering_ && (open_.color == img || open_.depth == img)) CloseRendering();
    CurrentFrame().garbage.push_back(it->second.res);
    if (it->second.fb) CurrentFrame().garbage.push_back(it->second.fb);
    targets_.erase(it);
}

bool VulkanBackend::CreateTargetResource(Target& t) {
    t.w = std::max(1u, uint32_t(std::lround(t.gw * scale_)));
    t.h = std::max(1u, uint32_t(std::lround(t.gh * scale_)));
    ImageSpec spec;
    spec.w = t.w;
    spec.h = t.h;
    if (t.depth) {
        spec.format = depth_format_;
        spec.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    } else {
        spec.format = t.fmt;
        spec.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    t.res = CreateImage(spec);
    if (!t.res) return false;
    Image& img = *t.res;
    Transition(img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkImageSubresourceRange range{img.Aspects(), 0, 1, 0, 1};
    if (t.depth) {
        VkClearDepthStencilValue v{1.0f, 0};
        vk.vkCmdClearDepthStencilImage(Cmd(), img.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &v, 1, &range);
        Transition(img, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    } else {
        VkClearColorValue v{};
        vk.vkCmdClearColorImage(Cmd(), img.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &v, 1, &range);
        Transition(img, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }
    return true;
}

uint64_t TargetMapKey(uint32_t data, bool depth) { return uint64_t(data) << 1 | (depth ? 1 : 0); }

// a smaller depth surface's console size, above the address bits
constexpr int kKeyWidthShift = 34;
constexpr int kKeyHeightShift = 48;

// Smaller depth surfaces sharing the main depth allocation need independent targets.
uint64_t VulkanBackend::SizedTargetKey(uint32_t data, bool depth, uint32_t gw, uint32_t gh) const {
    uint64_t key = TargetMapKey(data, depth);
    bool small_depth = depth && (gw != guest_w_ || gh != guest_h_);
    if (!small_depth) return key;
    return key | uint64_t(gw) << kKeyWidthShift | uint64_t(gh) << kKeyHeightShift;
}

VulkanBackend::TargetSlot VulkanBackend::FindTargetSlot(const Surface& s, bool depth) {
    TargetSlot slot;
    slot.data = mem::VirtualFromPhysical(s.data);
    SurfaceSize(s, slot.gw, slot.gh);
    slot.key = SizedTargetKey(slot.data, depth, slot.gw, slot.gh);
    slot.it = targets_.find(slot.key);
    slot.sized = slot.it != targets_.end() && slot.it->second.gw == slot.gw && slot.it->second.gh == slot.gh;
    return slot;
}

Target* VulkanBackend::FindTarget(const Surface& s, bool depth) {
    if (!s.header) return nullptr;
    TargetSlot slot = FindTargetSlot(s, depth);
    if (!slot.sized) return nullptr;
    target_used_[slot.data] = frame_;
    return &slot.it->second;
}

Target* VulkanBackend::EnsureTarget(const Surface& s, bool depth) {
    if (!s.header) return nullptr;
    TargetSlot slot = FindTargetSlot(s, depth);
    target_used_[slot.data] = frame_;
    if (slot.sized) return &slot.it->second;
    if (slot.it != targets_.end()) ReleaseTarget(slot.it);
    Target t;
    t.depth = depth;
    t.gw = slot.gw;
    t.gh = slot.gh;
    if (!CreateTargetResource(t)) {
        LOG_FIRST(8, "vulkan: target %ux%u could not be made", t.w, t.h);
        return nullptr;
    }
    g_perf.targets++;
    t.data = slot.data;
    t.bytes = GuestPitch(slot.gw) * slot.gh;
    if (depth) t.bytes = slot.gw * slot.gh * 4;
    t.epoch = DirtyEpoch(slot.data);
    t.sig = MemSig(slot.data, t.bytes);
    auto& ref = targets_[slot.key] = std::move(t);
    return &ref;
}

bool VulkanBackend::BindTargets(const State& s) {
    Target* rt = EnsureTarget(s.rt, false);
    Target* ds = s.rs[kRsZEnable] || s.rs[kRsStencilEnable] ? EnsureTarget(s.zs, true) : nullptr;
    if (rt && ds && (rt->w != ds->w || rt->h != ds->h)) ds = nullptr;
    if (!rt && !ds) return false;
    if (rt) {
        if (LatchCpuWrite(*rt)) UploadTarget(*rt, s.rt);
        Transition(*rt->res, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        rt->drawn = true;
    }
    if (ds) {
        Transition(*ds->res, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        ds->drawn = true;
    }
    Target* size = rt ? rt : ds;
    SetAttachments(rt ? rt->res.get() : nullptr, ds ? ds->res.get() : nullptr, size->w, size->h);
    bound_rt_ = rt;
    bound_ds_ = ds;
    return true;
}

constexpr uint64_t kDepthCopyKey = ~0ull;  // DepthCopy's pipeline in blit_psos_

// DepthCopy's pipeline: writes depth only, always passing; null when it can't be made
VkPipeline VulkanBackend::EnsureDepthCopyPipeline() {
    auto& pso = blit_psos_[kDepthCopyKey];
    if (pso) return pso;
    uint64_t h;
    PipelineParts p;
    p.vs = Compile(kDepthHlsl, "vs", vkshader::Stage::Vertex, h);
    p.ps = Compile(kDepthHlsl, "psmain", vkshader::Stage::Fragment, h);
    if (!p.vs || !p.ps) return VK_NULL_HANDLE;
    p.vs_entry = "vs";
    p.ps_entry = "psmain";
    p.ds.depthTestEnable = VK_TRUE;
    p.ds.depthWriteEnable = VK_TRUE;
    p.ds.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    p.depth = depth_format_;
    pso = CreatePipeline(p);
    return pso;
}

void VulkanBackend::DrawDepthCopy(Target& src, Target& dst, VkPipeline pso) {
    Transition(*src.res, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    Transition(*dst.res, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    SetAttachments(nullptr, dst.res.get(), dst.w, dst.h);
    SetFullViewport(dst.w, dst.h);
    Alloc cb = Upload(256, 256);
    VkImageView views[4] = {src.res->view};
    VkSampler samplers[4] = {blit_samplers_[0], blit_samplers_[1], blit_samplers_[2], blit_samplers_[3]};
    DrawFullScreen(pso, cb, views, samplers);
    dst.drawn = true;
    bound_rt_ = bound_ds_ = nullptr;
}

// a depth buffer drawn as colour into another depth buffer's memory: copied as depth
bool VulkanBackend::DepthCopy(const State& s) {
    TextureDesc d;
    if (!s.rt.header || !s.textures[0] || !Describe(s.textures[0], d) || Format(d.format).depth || d.cube ||
        d.depth != 1) {
        return false;
    }
    uint32_t gw;
    uint32_t gh;
    SurfaceSize(s.rt, gw, gh);
    auto di = targets_.find(SizedTargetKey(mem::VirtualFromPhysical(s.rt.data), true, gw, gh));
    auto si = targets_.find(SizedTargetKey(d.data, true, d.width, d.height));
    if (di == targets_.end() || si == targets_.end() || di == si) return false;
    VkPipeline pso = EnsureDepthCopyPipeline();
    if (!pso) return false;
    DrawDepthCopy(si->second, di->second, pso);
    return true;
}

// Mark only CPU-modified pixels opaque; the remaining image preserves the GPU render.
std::vector<uint8_t> LatchCpuChanges(Target& t) {
    std::vector<uint8_t> px(t.read.size(), 0);
    uint32_t guest_pitch = GuestPitch(t.gw);
    const uint8_t* guest = static_cast<const uint8_t*>(kernel::HostPtr(t.data));
    for (uint32_t y = 0; y < t.gh; y++) {
        for (uint32_t x = 0; x < t.gw; x++) {
            const uint8_t* c = guest + size_t(y) * guest_pitch + x * 4;
            size_t i = (size_t(y) * t.gw + x) * 4;
            if (!std::memcmp(c, &t.read[i], 4)) continue;
            std::memcpy(&t.read[i], c, 4);
            std::memcpy(&px[i], c, 3);
            px[i + 3] = 255;
        }
    }
    return px;
}

ImageP VulkanBackend::CreatePixelTexture(const std::vector<uint8_t>& px, uint32_t pitch, uint32_t w, uint32_t h) {
    ImageSpec spec;
    spec.w = w;
    spec.h = h;
    ImageP tex = CreateImage(spec);
    if (!tex) return nullptr;
    Alloc a = Upload(uint64_t(w) * 4 * h, 16);
    for (uint32_t y = 0; y < h; y++) std::memcpy(a.cpu + size_t(y) * w * 4, &px[size_t(y) * pitch], w * 4);
    Transition(*tex, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy r{};
    r.bufferOffset = a.off;
    r.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    r.imageExtent = {w, h, 1};
    vk.vkCmdCopyBufferToImage(Cmd(), a.buf, tex->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &r);
    Transition(*tex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    CurrentFrame().garbage.push_back(tex);
    return tex;
}

void VulkanBackend::UploadTarget(Target& t, const Surface& s) {
    g_perf.rtups++;
    TextureDesc d;
    if (!Describe(s.header, d) || Format(d.format).depth) return;
    std::vector<uint8_t> px;
    uint32_t pitch;
    // after a Sync only the pixels the CPU changed go over the picture, which stays at render scale
    bool over = !t.drawn && d.width == t.gw && d.height == t.gh && t.read.size() == size_t(t.gw) * t.gh * 4;
    if (over) {
        LOG_FIRST(4, "vulkan: CPU pixels drawn over target %08X", t.data);
        pitch = t.gw * 4;
        px = LatchCpuChanges(t);
    } else {
        DecodeLevel(d, 0, 0, static_cast<const uint8_t*>(kernel::HostPtr(d.data)), nullptr, px, pitch);
    }
    ImageP tex = CreatePixelTexture(px, pitch, d.width, d.height);
    if (!tex) return;
    SetBlitTarget(*t.res, t.w, t.h);
    float uv[4] = {0, 0, 1, 1};
    Blit(*tex, uv, {.alpha = true, .mul = 1 / t.range, .rtv = t.fmt, .over = over});
    t.epoch = DirtyEpoch(t.data);
    t.sig = MemSig(t.data, t.bytes);
}

void VulkanBackend::DrawFullScreen(VkPipeline pso, const Alloc& cb, const VkImageView views[4],
                                   const VkSampler samplers[4]) {
    if (!pso) return;
    EnsureRendering();
    vk.vkCmdBindPipeline(Cmd(), VK_PIPELINE_BIND_POINT_GRAPHICS, pso);
    Bindings b;
    for (int i = 0; i < 2; i++) {
        b.ubo[i] = cb.buf;
        b.offset[i] = cb.off;
        b.range[i] = 256;
    }
    for (int i = 0; i < 4; i++) {
        b.image[i] = views[i];
        b.sampler[i] = samplers[i];
    }
    BindSet(b);
    vk.vkCmdDraw(Cmd(), 3, 1, 0, 0);
}

void VulkanBackend::Blit(Image& src, const float uvrect[4], const BlitOptions& o) {
    Alloc cb = Upload(256, 256);
    float c[8] = {uvrect[0], uvrect[1], uvrect[2], uvrect[3],
                  o.gamma ? 1.0f : 0.0f, o.alpha ? 1.0f : 0.0f, float(o.taps), o.mul};
    std::memcpy(cb.cpu, c, sizeof(c));
    VkImageView views[4] = {src.view, lut_->view};
    VkSampler samplers[4] = {blit_samplers_[0], blit_samplers_[1], blit_samplers_[2], blit_samplers_[3]};
    DrawFullScreen(EnsureBlitPipeline(o.rtv, o.over), cb, views, samplers);
}

// one source can hold several entry points
uint64_t ShaderKey(const std::string& src, const char* entry, vkshader::Stage stage) {
    uint64_t stage_seed = Hash(&stage, sizeof(stage));
    uint64_t entry_seed = Hash(entry, std::strlen(entry), stage_seed);
    return Hash(src.data(), src.size(), entry_seed);
}

VkShaderModule VulkanBackend::CompileModule(const std::string& src, const char* entry, vkshader::Stage stage) const {
    std::vector<uint32_t> spirv;
    std::string error;
    VkShaderModule module = VK_NULL_HANDLE;
    if (!vkshader::Compile(src, entry, stage, spirv, error)) {
        LOGE("vulkan: shader compile failed: %s", error.c_str());
        LOGE("%s", src.c_str());
        return VK_NULL_HANDLE;
    }
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = spirv.size() * sizeof(uint32_t);
    ci.pCode = spirv.data();
    if (vk.vkCreateShaderModule(dev_, &ci, nullptr, &module) != VK_SUCCESS) module = VK_NULL_HANDLE;
    return module;
}

VkShaderModule VulkanBackend::Compile(const std::string& src, const char* entry, vkshader::Stage stage, uint64_t& hash) {
    hash = ShaderKey(src, entry, stage);
    auto it = blobs_.find(hash);
    if (it != blobs_.end()) return it->second;
    // Wait for a worker compiling the same source, then take its module.
    openshao::pipelines::InFlightScope flight{pre_shader_flight_, hash};
    {
        std::lock_guard lock(pre_mutex_);
        auto ready = pre_modules_.find(hash);
        if (ready != pre_modules_.end()) return blobs_[hash] = ready->second;
    }
    PerfScope perf_compile{g_perf.compile};
    g_perf.shaders++;
    VkShaderModule module = CompileModule(src, entry, stage);
    if (module) {
        std::lock_guard lock(pre_mutex_);
        pre_modules_.try_emplace(hash, module);
    }
    blobs_[hash] = module;
    return module;
}

VkPipeline VulkanBackend::CreatePipeline(const PipelineParts& p) const {
    // glslang keeps the HLSL entry point's name
    VkPipelineShaderStageCreateInfo st[3];
    uint32_t n = 0;
    auto stage = [&](VkShaderStageFlagBits bit, VkShaderModule m, const char* entry) {
        st[n] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        st[n].stage = bit;
        st[n].module = m;
        st[n].pName = entry;
        n++;
    };
    stage(VK_SHADER_STAGE_VERTEX_BIT, p.vs, p.vs_entry);
    if (p.gs) stage(VK_SHADER_STAGE_GEOMETRY_BIT, p.gs, "main");
    stage(VK_SHADER_STAGE_FRAGMENT_BIT, p.ps, p.ps_entry);
    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = p.topology;
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = p.color != VK_FORMAT_UNDEFINED ? 1 : 0;
    cb.pAttachments = &p.blend;
    VkDynamicState dyn[4] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_BLEND_CONSTANTS,
                             VK_DYNAMIC_STATE_STENCIL_REFERENCE};
    VkPipelineDynamicStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    ds.dynamicStateCount = 4;
    ds.pDynamicStates = dyn;
    VkPipelineRenderingCreateInfo ri{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    ri.colorAttachmentCount = cb.attachmentCount;
    ri.pColorAttachmentFormats = &p.color;
    ri.depthAttachmentFormat = p.depth;
    ri.stencilAttachmentFormat = HasStencil(p.depth) ? p.depth : VK_FORMAT_UNDEFINED;
    VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    ci.pNext = &ri;
    ci.stageCount = n;
    ci.pStages = st;
    ci.pVertexInputState = &p.input;
    ci.pInputAssemblyState = &ia;
    ci.pViewportState = &vp;
    ci.pRasterizationState = &p.raster;
    ci.pMultisampleState = &ms;
    ci.pDepthStencilState = &p.ds;
    ci.pColorBlendState = &cb;
    ci.pDynamicState = &ds;
    ci.layout = layout_;
    VkPipeline pso = VK_NULL_HANDLE;
    VkResult r = vk.vkCreateGraphicsPipelines(dev_, cache_, 1, &ci, nullptr, &pso);
    if (r != VK_SUCCESS) {
        LOGE("vulkan: pipeline creation failed: %s", vkapi::ResultName(r));
        return VK_NULL_HANDLE;
    }
    return pso;
}

VkPipeline VulkanBackend::EnsureBlitPipeline(VkFormat rtv, bool over) {
    auto& pso = blit_psos_[uint64_t(rtv) | uint64_t(over) << 32];
    if (pso) return pso;
    uint64_t h;
    PipelineParts p;
    p.vs = Compile(kBlitHlsl, "vs", vkshader::Stage::Vertex, h);
    p.ps = Compile(kBlitHlsl, "psmain", vkshader::Stage::Fragment, h);
    if (!p.vs || !p.ps) return VK_NULL_HANDLE;
    p.vs_entry = "vs";
    p.ps_entry = "psmain";
    p.color = rtv;
    if (over) {
        // premultiplied colour, the target keeps its alpha
        p.blend.blendEnable = VK_TRUE;
        p.blend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        p.blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        p.blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        p.blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        p.blend.colorBlendOp = p.blend.alphaBlendOp = VK_BLEND_OP_ADD;
    }
    pso = CreatePipeline(p);
    return pso;
}

// a copy of the target to read while drawing to it
ImageP VulkanBackend::Feedback(Target& t) {
    if (!t.fb) {
        ImageSpec spec;
        spec.format = t.res->format;
        spec.w = t.w;
        spec.h = t.h;
        t.fb = CreateImage(spec);
        if (!t.fb) return nullptr;
    }
    Transition(*t.res, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    Transition(*t.fb, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkImageCopy r{};
    r.srcSubresource = r.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    r.extent = {t.w, t.h, 1};
    vk.vkCmdCopyImage(Cmd(), t.res->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, t.fb->image,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &r);
    Transition(*t.fb, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return t.fb;
}


VkPrimitiveTopology PrimitiveTopology(uint32_t topo, bool strip) {
    if (topo == uint32_t(Topology::Points)) return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    if (topo == uint32_t(Topology::Lines)) return strip ? VK_PRIMITIVE_TOPOLOGY_LINE_STRIP : VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    return strip ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

constexpr uint32_t kFillWireframe = 0x1B01;  // D3DFILL_WIREFRAME
constexpr uint32_t kCullCw = 0x900;          // D3DCULL_CW

float VulkanBackend::PointScale() const {
    const Target* t = bound_rt_ ? bound_rt_ : bound_ds_;
    if (!t || !t->gw) return 1;
    return float(t->w) / float(t->gw);
}

VkPipeline VulkanBackend::EnsurePipeline(const PsoKey& k, VkShaderModule vs, VkShaderModule ps) {
    pso_first_use_ = pso_compiled_on_draw_ = false;
    uint64_t h = Hash(&k, sizeof(k));
    auto it = psos_.find(h);
    if (it != psos_.end()) return it->second;
    openshao::pipelines::InFlightScope flight{pre_pso_flight_, h};
    {
        std::lock_guard lock(pre_mutex_);
        auto ready = pre_psos_.find(h);
        if (ready != pre_psos_.end()) {
            const VkPipeline pso = ready->second;
            pre_psos_.erase(ready);
            pso_first_use_ = pso != VK_NULL_HANDLE;
            return psos_[h] = pso;
        }
    }
    uint64_t hash;
    // Expand points to full console pixels without culling.
    VkShaderModule points = k.topo == uint32_t(Topology::Points) && gs_ok_
        ? Compile(PointHlsl(), "main", vkshader::Stage::Geometry, hash) : VK_NULL_HANDLE;
    VkShaderModule atlas = k.atlas_guard && gs_ok_
        ? Compile(openshao::AtlasBoundsHlsl(), "main", vkshader::Stage::Geometry, hash) : VK_NULL_HANDLE;
    if (k.atlas_guard && gs_ok_ && !atlas) return psos_[h] = VK_NULL_HANDLE;
    g_perf.psos++;
    VkPipeline pso = MakePipeline(k, vs, ps, points, atlas);
    psos_[h] = pso;
    {
        std::lock_guard lock(pre_mutex_);
        pre_done_.insert(h);
    }
    pso_first_use_ = pso_compiled_on_draw_ = pso != VK_NULL_HANDLE;
    return pso;
}

VkPipeline VulkanBackend::MakePipeline(const PsoKey& k, VkShaderModule vs, VkShaderModule ps, VkShaderModule points,
                                       VkShaderModule atlas) const {
    PipelineParts p;
    p.vs = vs;
    p.ps = ps;
    // Vertex attributes and shader inputs must share register order.
    VkVertexInputAttributeDescription attrs[16];
    VkVertexInputBindingDescription binds[16];
    uint32_t na = 0;
    for (uint32_t r = 0; r < 16; r++) {
        if (!((k.regs >> r) & 1)) continue;
        uint32_t e = k.elements[r];
        attrs[na] = {na, (e >> 8) & 0xFF, VertexFormat(e & 0xFF), e >> 16};
        na++;
    }
    for (uint32_t b = 0; b < k.buffers; b++) binds[b] = {b, k.strides[b], VK_VERTEX_INPUT_RATE_VERTEX};
    p.input.vertexAttributeDescriptionCount = na;
    p.input.pVertexAttributeDescriptions = attrs;
    p.input.vertexBindingDescriptionCount = na ? k.buffers : 0;
    p.input.pVertexBindingDescriptions = binds;
    p.topology = PrimitiveTopology(k.topo, k.strip);
    auto& b = p.blend;
    b.blendEnable = k.blend;
    b.srcColorBlendFactor = Blend(k.src, false);
    b.dstColorBlendFactor = Blend(k.dst, false);
    b.colorBlendOp = BlendOp(k.op);
    b.srcAlphaBlendFactor = Blend(k.src, true);
    b.dstAlphaBlendFactor = Blend(k.dst, true);
    b.alphaBlendOp = BlendOp(k.op);
    b.colorWriteMask = k.wmask;  // R G B A bits as D3D12's
    p.raster.polygonMode = k.fill == kFillWireframe && wire_ok_ ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    p.raster.cullMode = k.cull ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
    // the flipped viewport keeps D3D's screen: its winding names carry over
    p.raster.frontFace = k.ccw ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
    if (k.zbias || k.zslope != 0) {
        p.raster.depthBiasEnable = VK_TRUE;
        p.raster.depthBiasConstantFactor = float(k.zbias);
        p.raster.depthBiasSlopeFactor = k.zslope;
    }
    p.ds.depthTestEnable = k.zen;
    p.ds.depthWriteEnable = k.zwrite;
    p.ds.depthCompareOp = k.zen ? Cmp(k.zfunc) : VK_COMPARE_OP_ALWAYS;
    p.ds.stencilTestEnable = k.sten;
    VkStencilOpState so{StencilOp(k.sfail), StencilOp(k.spass), StencilOp(k.szfail), Cmp(k.sfunc), k.smask, k.swmask, 0};
    p.ds.front = p.ds.back = so;
    p.color = k.has_rt ? VkFormat(k.rtfmt) : VK_FORMAT_UNDEFINED;
    p.depth = k.has_ds ? depth_format_ : VK_FORMAT_UNDEFINED;
    if (points) {
        p.gs = points;
        p.raster.polygonMode = VK_POLYGON_MODE_FILL;
        p.raster.cullMode = VK_CULL_MODE_NONE;
        p.raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
    }
    if (k.atlas_guard && gs_ok_) {
        if (!atlas) return VK_NULL_HANDLE;
        p.gs = atlas;
    }
    return CreatePipeline(p);
}


TexKind TextureKind(const TextureDesc& d) {
    if (d.cube) return kTexCube;
    if (d.depth > 1) return kTex3D;
    return kTex2D;
}

constexpr float kUpscaledMin = 1.01f;  // host texels per console texel past rounding: the target is upscaled

void VulkanBackend::BindTargetTexture(const State& s, int stage, const TextureDesc& d, Target& t, bool box,
                                      DrawTextures& stages) {
    TexInfo& info = stages.info[stage];
    if (&t == draw_ds_) return;
    bool feedback = &t == draw_rt_;
    stages.rt[stage] = true;
    if (!t.depth && LatchCpuWrite(t)) {
        Surface su;
        su.header = s.textures[size_t(stage)];
        UploadTarget(t, su);
    }
    info.kind = kTex2D;
    // Average each downsample tap over a console texel to avoid skipped samples and ghosting.
    Target* rt = FindTarget(s.rt, false);
    float k = float(t.w) / (t.gw * stages.k[stage]);
    if (!t.depth && box && k > kUpscaledMin && rt && rt != &t && (t.gw > rt->gw || t.gh > rt->gh)) {
        stages.wide[stage] = k;
        info.flags |= kTexWide;
    }
    // the console reads the pixels it is about to overwrite: give the draw a copy
    if (feedback) {
        ImageP fb = Feedback(t);
        if (!fb) return;
        stages.view[stage] = fb->view;
        stages.bound[stage] = true;
        return;
    }
    Transition(*t.res, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (t.depth) info.shadow = ShadowDepthKind(d.format);
    stages.view[stage] = t.res->view;
    stages.bound[stage] = true;
}

bool SameTexture(const Tex& tx, const TextureDesc& d) {
    return tx.res && tx.d.format == d.format && tx.d.width == d.width && tx.d.height == d.height &&
           tx.d.depth == d.depth && tx.d.levels == d.levels && tx.d.cube == d.cube && tx.d.pitch == d.pitch;
}

// A replacement is BC3 with its own size and mip chain, whatever the game's texture is.
const FormatInfo kReplacementFormat{"BC3", 16, false, true, false, Host::BC3};
std::unordered_set<uint64_t> g_bad_replacements;

// The texture pack's replacement for a game texture, matched by the hash of its level 0 bytes
std::optional<texpack::Entry> FindReplacement(const TextureDesc& d, const FormatInfo& fi, const uint8_t* guest,
                                              uint64_t& hash) {
    constexpr uint32_t kFmtP8 = 0x0B;
    if (!texpack::Enabled() || d.cube || d.depth != 1 || d.linear || fi.depth || d.format == kFmtP8) return std::nullopt;
    if (fi.host != Host::BGRA8 && fi.host != Host::BC1 && fi.host != Host::BC2 && fi.host != Host::BC3) return std::nullopt;
    hash = Hash(guest, LevelSize(d, 0));
    if (g_bad_replacements.count(hash)) return std::nullopt;
    auto entry = texpack::Find(hash);
    // a replacement is a redraw of the same picture
    if (entry && uint64_t(entry->w) * d.height != uint64_t(entry->h) * d.width) return std::nullopt;
    return entry;
}

bool VulkanBackend::CreateTexture(Tex& tx, const TextureDesc& d, const FormatInfo& game_fi, uint64_t key,
                                  std::optional<texpack::Entry> rep, uint64_t rep_hash) {
    g_perf.newtex++;
    if (tx.res) CurrentFrame().garbage.push_back(tx.res);
    tx = Tex{};
    tx.d = d;
    tx.rep = rep;
    tx.rep_hash = rep_hash;
    const FormatInfo& fi = rep ? kReplacementFormat : game_fi;
    tx.levels = rep ? rep->levels : d.levels;
    switch (fi.host) {
        case Host::BC1: tx.fmt = VK_FORMAT_BC1_RGBA_UNORM_BLOCK; tx.bc = 1; break;
        case Host::BC2: tx.fmt = VK_FORMAT_BC2_UNORM_BLOCK; tx.bc = 2; break;
        case Host::BC3: tx.fmt = VK_FORMAT_BC3_UNORM_BLOCK; tx.bc = 3; break;
        case Host::R32F: tx.fmt = VK_FORMAT_D32_SFLOAT; break;
        default: tx.fmt = kBgra; break;
    }
    if (tx.bc && bc_) tx.bc = 0;
    else if (tx.bc) tx.fmt = kBgra;
    uint32_t w = rep ? rep->w : d.width;
    uint32_t h = rep ? rep->h : d.height;
    if (fi.compressed && ((w & 3) || (h & 3))) {
        w = (w + 3) & ~3u;
        h = (h + 3) & ~3u;
        tx.levels = 1;
    }
    ImageSpec spec;
    spec.format = tx.fmt;
    spec.w = w;
    spec.h = h;
    spec.depth = d.depth;
    spec.levels = tx.levels;
    spec.kind = d.cube ? ImageKind::Cube : d.depth > 1 ? ImageKind::Tex3D : ImageKind::Tex2D;
    tx.res = CreateImage(spec);
    if (!tx.res) {
        textures_.erase(key);
        return false;
    }
    return true;
}

void VulkanBackend::UploadTextureLevel(Tex& tx, uint32_t f, uint32_t l, const std::vector<uint8_t>& src_px,
                                       uint32_t src_pitch) {
    const Image& img = *tx.res;
    uint32_t lw = std::max(1u, img.w >> l);
    uint32_t lh = std::max(1u, img.h >> l);
    uint32_t slices = std::max(1u, img.depth >> l);
    const std::vector<uint8_t>* px = &src_px;
    uint32_t pitch = src_pitch;
    std::vector<uint8_t> decoded;
    if (tx.bc) {
        vkbc::Decode(tx.bc, src_px.data(), src_pitch, lw, lh, slices, decoded, pitch);
        px = &decoded;
    }
    bool blocks = IsBlockFormat(tx.fmt);
    uint32_t row_size = blocks ? (lw + 3) / 4 * (tx.fmt == VK_FORMAT_BC1_RGBA_UNORM_BLOCK ? 8 : 16) : lw * 4;
    uint32_t rows = blocks ? (lh + 3) / 4 : lh;
    Alloc a = Upload(uint64_t(row_size) * rows * slices, 16);
    size_t src_rows = pitch ? px->size() / pitch / slices : 0;
    for (uint32_t z = 0; z < slices; z++) {
        for (uint32_t y = 0; y < rows; y++) {
            uint8_t* dst = a.cpu + (size_t(z) * rows + y) * row_size;
            if (y < src_rows) std::memcpy(dst, &(*px)[(size_t(z) * src_rows + y) * pitch], std::min(pitch, row_size));
            else std::memset(dst, 0, row_size);
            if (pitch < row_size && y < src_rows) std::memset(dst + pitch, 0, row_size - pitch);
        }
    }
    VkBufferImageCopy r{};
    r.bufferOffset = a.off;
    r.imageSubresource = {IsDepthFormat(tx.fmt) ? VkImageAspectFlags(VK_IMAGE_ASPECT_DEPTH_BIT)
                                                : VkImageAspectFlags(VK_IMAGE_ASPECT_COLOR_BIT),
                          l, f, 1};
    r.imageExtent = {lw, lh, slices};
    vk.vkCmdCopyBufferToImage(Cmd(), a.buf, img.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &r);
}

bool VulkanBackend::UploadReplacement(Tex& tx) {
    using Clock = std::chrono::steady_clock;
    const auto micros = [](Clock::time_point from, Clock::time_point to) {
        return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(to - from).count());
    };
    const auto start = Clock::now();
    std::vector<uint8_t> blocks;
    const bool read = texpack::Load(tx.rep_hash, blocks);
    const auto read_done = Clock::now();
    if (!read) {
        texpack::NoteLoad(micros(start, read_done), 0, false);
        return false;
    }
    Transition(*tx.res, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    size_t offset = 0;
    std::vector<uint8_t> level;
    for (uint32_t l = 0; l < tx.levels; l++) {
        const uint32_t size = texpack::LevelBytes(tx.rep->w, tx.rep->h, l);
        level.assign(blocks.begin() + std::ptrdiff_t(offset), blocks.begin() + std::ptrdiff_t(offset + size));
        UploadTextureLevel(tx, 0, l, level, (std::max(1u, tx.rep->w >> l) + 3) / 4 * 16);
        offset += size;
    }
    Transition(*tx.res, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    const auto done = Clock::now();
    const uint64_t disk_us = micros(start, read_done), upload_us = micros(read_done, done);
    texpack::NoteLoad(disk_us, upload_us, true);
    tx.resident.reset();
    tx.resident = std::make_shared<texpack::Resident>(blocks.size());
    static std::atomic<uint32_t> slow_logged{0};
    if (disk_us + upload_us >= 5000 && slow_logged.fetch_add(1) < 100)
        LOGI("Texture pack: slow load %016llx %ux%u, %u levels, %zu bytes: read %.1f ms, upload %.1f ms",
             static_cast<unsigned long long>(tx.rep_hash), tx.rep->w, tx.rep->h, tx.levels, blocks.size(),
             double(disk_us) / 1000.0, double(upload_us) / 1000.0);
    return true;
}

void VulkanBackend::UploadTexture(Tex& tx, const TextureDesc& d, const uint8_t* p, const uint32_t* colours) {
    Transition(*tx.res, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    uint32_t faces = d.cube ? 6 : 1;
    std::vector<uint8_t> px;
    for (uint32_t f = 0; f < faces; f++) {
        for (uint32_t l = 0; l < tx.levels; l++) {
            uint32_t pitch;
            DecodeLevel(d, l, f, p, colours, px, pitch);
            UploadTextureLevel(tx, f, l, px, pitch);
        }
    }
    Transition(*tx.res, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

// Include palette address and content hash in the P8 texture cache key.
struct StagePalette {
    uint32_t colours[256] = {};
    uint32_t va = 0;
    uint64_t hash = 0;
};

// Decode P8 with the stage's palette size from the top two size-code bits.
void ReadStagePalette(const State& s, int stage, const TextureDesc& d, StagePalette& palette) {
    constexpr uint32_t kFmtP8 = 0x0B;       // D3DFMT_P8
    constexpr uint32_t kPaletteCommon = 0;  // D3DPalette: Common, its top two bits the size code
    constexpr uint32_t kPaletteData = 4;    // D3DPalette: Data, physical address of the colours
    constexpr uint32_t kPaletteSizeShift = 30;
    if (d.format != kFmtP8 || !s.palettes[size_t(stage)]) return;
    uint32_t header = s.palettes[size_t(stage)];
    palette.va = mem::VirtualFromPhysical(kernel::R32(header + kPaletteData));
    uint32_t size_code = kernel::R32(header + kPaletteCommon) >> kPaletteSizeShift;
    size_t bytes = (std::size(palette.colours) >> size_code) * sizeof(uint32_t);
    std::memcpy(palette.colours, kernel::HostPtr(palette.va), bytes);
    palette.hash = Hash(palette.colours, sizeof(palette.colours));
}

void VulkanBackend::BindGuestTexture(const State& s, int stage, const TextureDesc& d, const FormatInfo& fi,
                                     DrawTextures& stages) {
    TexInfo& info = stages.info[stage];
    StagePalette palette;
    ReadStagePalette(s, stage, d, palette);
    const uint64_t key = uint64_t(palette.va) << 32 | d.data;
    Tex& tx = textures_[key];
    uint32_t total = TotalSize(d);
    bool same = SameTexture(tx, d);
    uint64_t rep_hash = 0;
    if (!same) {
        const auto rep = FindReplacement(d, fi, static_cast<const uint8_t*>(kernel::HostPtr(d.data)), rep_hash);
        if (!CreateTexture(tx, d, fi, key, rep, rep ? rep_hash : 0)) return;
    }
    // Check dirty epochs and sampled bytes once per frame.
    if (tx.frame != presents_ + 1 || !same || tx.palette != palette.hash) {
        tx.frame = presents_ + 1;
        tx.palette = palette.hash;
        const uint8_t* p = static_cast<const uint8_t*>(kernel::HostPtr(d.data));
        auto perf_t = PerfClock::now();
        uint64_t sig = MemSig(d.data, total) ^ DirtyEpoch(d.data) ^ palette.hash;
        g_perf.sig += PerfMs(perf_t);
        if (sig != tx.sig || !same) {
            PerfScope perf_up{g_perf.texup};
            g_perf.texups++;
            g_perf.texup_bytes += total;
            tx.sig = sig;
            if (same && texpack::Enabled()) {
                // the game rewrote the memory: the replacement may no longer apply
                const auto rep = FindReplacement(d, fi, p, rep_hash);
                if (rep.has_value() != tx.rep.has_value() || (rep && rep_hash != tx.rep_hash)) {
                    if (!CreateTexture(tx, d, fi, key, rep, rep ? rep_hash : 0)) return;
                    tx.sig = sig;
                    tx.frame = presents_ + 1;
                    tx.palette = palette.hash;
                }
            }
            if (!tx.rep || !UploadReplacement(tx)) {
                if (tx.rep) {
                    g_bad_replacements.insert(tx.rep_hash);
                    if (!CreateTexture(tx, d, fi, key)) return;
                    tx.sig = sig;
                    tx.frame = presents_ + 1;
                    tx.palette = palette.hash;
                }
                UploadTexture(tx, d, p, palette.va ? palette.colours : nullptr);
            }
        }
    }
    stages.view[stage] = tx.res->view;
    stages.levels[stage] = tx.levels;
    stages.bound[stage] = true;
    if (fi.depth) info.shadow = ShadowDepthKind(d.format);
}

void VulkanBackend::BindTexture(const State& s, int stage, DrawTextures& stages) {
    TexInfo& info = stages.info[stage];
    info = {};
    stages.view[stage] = VK_NULL_HANDLE;
    stages.levels[stage] = 1;
    stages.bound[stage] = false;
    stages.rt[stage] = false;
    stages.k[stage] = 1;
    stages.wide[stage] = 0;
    TextureDesc d;
    if (!s.textures[size_t(stage)] || !Describe(s.textures[size_t(stage)], d)) return;
    const FormatInfo& fi = Format(d.format);
    info.kind = TextureKind(d);
    info.rect = d.linear;
    // Sample rendered targets directly, except when DXT data has replaced their guest memory.
    auto tit = targets_.find(SizedTargetKey(d.data, fi.depth, d.width, d.height));
    if (tit != targets_.end() && !d.cube && d.depth == 1 && !fi.compressed) {
        BindTargetTexture(s, stage, d, tit->second, true, stages);
        return;
    }
    BindGuestTexture(s, stage, d, fi, stages);
}

// D3DTEXF: 1 point, 2 linear, 3 anisotropic
constexpr uint32_t kTexfLinear = 2;
constexpr uint32_t kTexfAnisotropic = 3;
constexpr uint32_t kTaddressWrap = 1;
constexpr uint32_t kTaddressClamp = 3;

VkSampler VulkanBackend::EnsureSampler(const VkSamplerCreateInfo& ci) {
    // the fields past sType and pNext, which differ by nothing a sampler is made from
    uint64_t key = Hash(&ci.flags, sizeof(ci) - offsetof(VkSamplerCreateInfo, flags));
    auto it = samplers_.find(key);
    if (it != samplers_.end()) return it->second;
    if (samplers_.size() >= kSamplersKept) {
        // Retire samplers with their in-flight frames when resetting the cache.
        for (auto& [k, smp] : samplers_) CurrentFrame().dead_samplers.push_back(smp);
        samplers_.clear();
    }
    VkSampler smp = VK_NULL_HANDLE;
    if (vk.vkCreateSampler(dev_, &ci, nullptr, &smp) != VK_SUCCESS) return blit_samplers_[1];
    samplers_[key] = smp;
    return smp;
}

void VulkanBackend::EnsureStageSamplers(const State& s, const DrawTextures& stages, bool clamp, VkSampler out[4]) {
    uint32_t aniso = app::Get().aniso;
    float max_bias = gpu_props_.limits.maxSamplerLodBias;
    for (int i = 0; i < 4; i++) {
        VkSamplerCreateInfo sd{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        const auto& t = s.tss[size_t(i)];
        uint32_t mag = t[kTssMagFilter];
        uint32_t min = t[kTssMinFilter];
        uint32_t mip = t[kTssMipFilter];
        auto lin = [](uint32_t f) { return f >= kTexfLinear; };
        sd.minFilter = lin(min) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        sd.magFilter = lin(mag) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        sd.mipmapMode = mip == kTexfLinear ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
        bool anisotropic = min == kTexfAnisotropic || mag == kTexfAnisotropic;
        uint32_t an = anisotropic ? std::max<uint32_t>(t[kTssMaxAnisotropy], 1) : 1;
        // forced only where the title filters across mip levels and the texture has them
        if (aniso && lin(min) && mip && stages.levels[i] > 1 && !stages.info[i].shadow) an = aniso;
        if (an > 1 && aniso_ok_) {
            sd.anisotropyEnable = VK_TRUE;
            sd.maxAnisotropy = std::min<float>(float(std::min<uint32_t>(an, 16)), gpu_props_.limits.maxSamplerAnisotropy);
            sd.minFilter = sd.magFilter = VK_FILTER_LINEAR;
            sd.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        }
        // Unit coordinates cover one tile; clamp them to avoid wrapping into the opposite edge.
        sd.addressModeU = Address(clamp && t[kTssAddressU] == kTaddressWrap ? kTaddressClamp : t[kTssAddressU]);
        sd.addressModeV = Address(clamp && t[kTssAddressV] == kTaddressWrap ? kTaddressClamp : t[kTssAddressV]);
        sd.addressModeW = Address(t[kTssAddressW]);
        sd.mipLodBias = std::clamp(Bits(t[kTssMipLodBias]), -max_bias, max_bias);
        sd.minLod = float(t[kTssMaxMipLevel]);
        sd.maxLod = mip ? VK_LOD_CLAMP_NONE : sd.minLod;
        // Select the nearest supported Vulkan border: black, transparent black or white.
        float bc[4];
        Color(t[kTssBorderColor], bc);
        sd.borderColor = bc[3] < 0.5f ? VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK
                         : bc[0] + bc[1] + bc[2] > 1.5f ? VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
                                                        : VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        if (stages.info[i].shadow) {
            // the console tests texel OP r, Vulkan tests r OP texel
            static const uint8_t swap[8] = {0, 4, 2, 6, 1, 5, 3, 7};
            const bool linear = (lin(min) || lin(mag)) &&
                (stages.rt[i] ? shadow_target_linear_ : shadow_texture_linear_);
            sd.minFilter = sd.magFilter = linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
            sd.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            sd.compareEnable = VK_TRUE;
            sd.compareOp = VkCompareOp(swap[CmpIndex(s.rs[kRsShadowFunc])]);
            sd.anisotropyEnable = VK_FALSE;
            sd.maxAnisotropy = 0;
        }
        out[i] = EnsureSampler(sd);
    }
}

uint32_t SimpleState(const State& s, uint32_t method) { return s.simple[method >> 2]; }

constexpr uint32_t kQuadVertices = 6;  // a quad drawn as two triangles

DrawShaders ClassifyShaders(const State& s, const Assembled& a, uint32_t count) {
    DrawShaders sh;
    // a shader made from a declaration alone (no program) is the fixed function pipeline on those inputs
    sh.prog = (s.vs_handle & 1) && s.vs && !s.vs->program.empty();
    // Only small quads without depth testing qualify as full-target screen passes.
    bool points = a.topology == Topology::Points;
    sh.quad = count <= kQuadVertices && (!s.rs[kRsZEnable] || (!sh.prog && a.pretransformed)) && !points;
    sh.fixed_transform = !sh.prog && !a.pretransformed;
    if (!sh.prog) sh.fk = MakeFixedKey(s);
    return sh;
}

// the program's hash is cached in the guest shader object
void EnsureProgramHash(const State& s) {
    if (!s.vs->hash) s.vs->hash = Hash(s.vs->program.data(), s.vs->program.size() * 4) | 1;
}

constexpr uint64_t kDeclarationSalt = 0x10000;    // fixed function seed, above the 16 bit register mask
constexpr uint64_t kPretransformedSalt = 0x5A5A;  // screen space fixed function vertices, apart from untransformed
constexpr uint64_t kQuadSalt = 0xC3C30000;        // the QuadHlsl variant, apart from the plain shader

uint64_t VertexKey(const State& s, const Assembled& a, const DrawShaders& sh) {
    uint64_t seed = sh.prog ? s.vs->hash ^ a.regs : Hash(&s.vs_handle, 4, a.regs | kDeclarationSalt);
    uint64_t vkey = Hash(&a.decode, 8, seed);
    if (!sh.prog && a.pretransformed) vkey ^= kPretransformedSalt;
    if (sh.quad) vkey ^= kQuadSalt;
    if (!sh.prog) vkey = Hash(&sh.fk, sizeof(sh.fk), vkey);
    return vkey;
}

std::string VertexHlsl(const State& s, const Assembled& a, const DrawShaders& sh) {
    if (sh.prog) return VertexProgramHlsl(s.vs->program, a.regs, a.decode);
    return FixedVertexHlsl(a.regs, a.pretransformed, a.decode, sh.fk);
}

void VulkanBackend::EnsureVertexShader(const State& s, const Assembled& a, DrawShaders& sh) {
    auto vit = shader_src_.find(sh.vkey);
    if (vit != shader_src_.end()) {
        sh.vs_code = blobs_[vit->second];
        sh.vh = vit->second;
        return;
    }
    std::string src = VertexHlsl(s, a, sh);
    sh.vs_code = Compile(sh.quad ? QuadHlsl(std::move(src)) : src, "main", vkshader::Stage::Vertex, sh.vh);
    shader_src_[sh.vkey] = sh.vh;
}

bool VulkanBackend::BindStageTexturesAndTargets(const State& s, DrawTextures& stages) {
    auto perf_t = PerfClock::now();
    draw_rt_ = bound_rt_;
    draw_ds_ = bound_ds_;
    for (int i = 0; i < 4; i++) BindTexture(s, i, stages);
    g_perf.bind += PerfMs(perf_t);
    // textures may have recorded copies that unbound the targets
    return BindTargets(s);
}

constexpr uint64_t kPixelKeySalt = 0x9E3779B97F4A7C15ull;

void VulkanBackend::EnsurePixelShader(const PixelKey& pk, DrawShaders& sh) {
    // pixel shaders share the map with vertex shaders, the salt keeps their keys apart
    auto pit = shader_src_.find(sh.pkey ^ kPixelKeySalt);
    if (pit != shader_src_.end()) {
        sh.ps_code = blobs_[pit->second];
        sh.ph = pit->second;
        return;
    }
    sh.ps_code = Compile(PixelHlsl(pk), "main", vkshader::Stage::Fragment, sh.ph);
    shader_src_[sh.pkey ^ kPixelKeySalt] = sh.ph;
}

void KeyBlend(const State& s, PsoKey& k) {
    k.blend = SimpleState(s, kMBlendEnable) != 0;
    if (k.blend) {
        k.src = SimpleState(s, kMSrcBlend);
        k.dst = SimpleState(s, kMDestBlend);
        k.op = SimpleState(s, kMBlendOp);
    }
    uint32_t cw = SimpleState(s, kMColorWriteEnable);
    k.wmask = ((cw >> 16) & 1) | ((cw >> 8) & 1) << 1 | (cw & 1) << 2 | ((cw >> 24) & 1) << 3;
}

constexpr float kDepth24Max = 16777215.0f;  // 2^24 minus 1: a 24 bit depth buffer's units per 1.0

void VulkanBackend::KeyDepthStencil(const State& s, const Assembled& a, PsoKey& k) {
    k.zen = s.rs[kRsZEnable] != 0;
    if (k.zen) {
        k.zwrite = SimpleState(s, kMZWriteEnable) != 0;
        k.zfunc = SimpleState(s, kMZFunc);
    }
    k.sten = s.rs[kRsStencilEnable] != 0;
    if (k.sten) {
        k.sfunc = SimpleState(s, kMStencilFunc);
        k.smask = SimpleState(s, kMStencilMask) & 0xFF;
        k.swmask = SimpleState(s, kMStencilWriteMask) & 0xFF;
        k.sfail = s.rs[kRsStencilFail];
        k.szfail = SimpleState(s, kMStencilZFail);
        k.spass = SimpleState(s, kMStencilPass);
    }
    // Constant bias in 24-bit UNORM units (D32 scales by depth exponent); slope bias in console pixels.
    if (a.topology == Topology::Triangles && SimpleState(s, kMSolidOffsetEnable)) {
        k.zbias = int32_t(std::lround(Bits(SimpleState(s, kMDepthOffset)) * kDepth24Max / s.zscale));
        k.zslope = Bits(SimpleState(s, kMSlopeScale)) * (bound_ds_ ? float(bound_ds_->w) / bound_ds_->gw : 1.0f);
    }
}

void KeyRasterizer(const State& s, const Assembled& a, PsoKey& k) {
    uint32_t cull = s.rs[kRsCullMode];
    k.cull = cull != 0 && a.topology == Topology::Triangles;
    k.ccw = cull == kCullCw;  // culling clockwise leaves counter clockwise as the front
    k.fill = s.rs[kRsFillMode];
}

void VulkanBackend::BuildPipelineKey(const State& s, const Assembled& a, uint64_t vh, uint64_t ph, PsoKey& k) {
    openshao::pipelines::PipelinePass pass;
    pass.color = bound_rt_ != nullptr;
    pass.depth = bound_ds_ != nullptr;
    pass.color_format = bound_rt_ ? uint32_t(bound_rt_->fmt) : 0;
    pass.depth_scale = bound_ds_ ? float(bound_ds_->w) / bound_ds_->gw : 1;
    openshao::pipelines::FillPipelineKey(s, a, vh, ph, pass, k);
}

constexpr float kAlphaTestAlways = 7.0f;

// D3DRS_FOGTABLEMODE values
constexpr uint32_t kFogTableNone = 0;
constexpr float kFogTableLinear = 3.0f;

// table mode none under a vertex program: oFog is the blend factor, passed on as linear fog from 1 to 0
void FillFogConstants(const State& s, bool vertex_program, DrawConstants& c) {
    if (!s.rs[kRsFogEnable]) return;
    if (vertex_program && s.rs[kRsFogTableMode] == kFogTableNone) {
        c.fog[0] = 1.0f;
        c.fog[1] = 0.0f;
        c.fog[2] = 0.0f;
        c.fog[3] = kFogTableLinear;
        return;
    }
    c.fog[0] = Bits(s.rs[kRsFogStart]);
    c.fog[1] = Bits(s.rs[kRsFogEnd]);
    c.fog[2] = Bits(s.rs[kRsFogDensity]);
    c.fog[3] = float(s.rs[kRsFogTableMode]);
}

void FillSurfaceConstants(const State& s, GuestExtent size, DrawConstants& c) {
    c.surf[0] = float(size.w);
    c.surf[1] = float(size.h);
    c.surf[2] = 1.0f / s.zscale;
    Color(s.rs[kRsFogColor], c.fog_color);
    c.misc[0] = float(SimpleState(s, kMAlphaRef) & 0xFF);
    c.misc[1] = SimpleState(s, kMAlphaTestEnable) ? float(CmpIndex(SimpleState(s, kMAlphaFunc))) : kAlphaTestAlways;
    c.misc[2] = float(CmpIndex(s.rs[kRsShadowFunc]));
    c.misc[3] = s.rs[kRsSpecularEnable] ? 1.0f : 0.0f;
}

void FillCombinerConstants(const State& s, DrawConstants& c) {
    for (int i = 0; i < 8; i++) {
        Color(SimpleState(s, kMPsConstant0 + i * 4), c.pc[i * 2]);
        Color(SimpleState(s, kMPsConstant1 + i * 4), c.pc[i * 2 + 1]);
    }
    Color(SimpleState(s, kMPsFinalConstant0), c.pc[16]);
    Color(SimpleState(s, kMPsFinalConstant1), c.pc[17]);
    Color(s.rs[kRsTextureFactor], c.tfactor);
}

constexpr float kGlowBoxTexels = 3.0f;  // console texels across the box each wide tap averages

void VulkanBackend::FillTexelConstants(int i, const TextureDesc& d, const DrawTextures& stages, DrawConstants& c) {
    c.texsize[i][0] = float(d.width);
    c.texsize[i][1] = float(d.height);
    // the console samples pixels at integer positions, the host half a pixel on: targets are read half a texel back
    if (!stages.rt[i]) return;
    float k = stages.k[i];
    c.texsize[i][2] = -0.5f / (float(d.width) * k);
    c.texsize[i][3] = -0.5f / (float(d.height) * k);
    // a 3 console texel box per tap keeps the console chain's glow width and brightness
    if (stages.wide[i] > 0) {
        float n = std::ceil(kGlowBoxTexels * stages.wide[i] * 0.5f);
        c.spread[i][0] = kGlowBoxTexels / (n * float(d.width) * k);
        c.spread[i][1] = kGlowBoxTexels / (n * float(d.height) * k);
        c.spread[i][2] = n;
    }
}

void VulkanBackend::FillTextureConstants(const State& s, const DrawTextures& stages, DrawConstants& c) {
    for (int i = 0; i < 4; i++) {
        TextureDesc d;
        if (s.textures[size_t(i)] && Describe(s.textures[size_t(i)], d)) FillTexelConstants(i, d, stages, c);
        const auto& t = s.tss[size_t(i)];
        c.bump[i][0] = Bits(t[kTssBumpEnvMat00]);
        c.bump[i][1] = Bits(t[kTssBumpEnvMat01]);
        c.bump[i][2] = Bits(t[kTssBumpEnvMat10]);
        c.bump[i][3] = Bits(t[kTssBumpEnvMat11]);
        c.bumpl[i][0] = Bits(t[kTssBumpEnvLScale]);
        c.bumpl[i][1] = Bits(t[kTssBumpEnvLOffset]);
    }
}

void MatMul(const float* x, const float* y, float* o) {
    for (int r = 0; r < 4; r++) {
        for (int col = 0; col < 4; col++) {
            o[r * 4 + col] = x[r * 4] * y[col] + x[r * 4 + 1] * y[4 + col] + x[r * 4 + 2] * y[8 + col] +
                             x[r * 4 + 3] * y[12 + col];
        }
    }
}

// HLSL mul(row vector, matrix) with column major packing wants the transpose
void Transpose(const float* m, float* o) {
    for (int r = 0; r < 4; r++) {
        for (int col = 0; col < 4; col++) o[col * 4 + r] = m[r * 4 + col];
    }
}

void InverseTranspose3x3(const float* wv, float* n) {
    float det = wv[0] * (wv[5] * wv[10] - wv[6] * wv[9]) - wv[1] * (wv[4] * wv[10] - wv[6] * wv[8]) +
                wv[2] * (wv[4] * wv[9] - wv[5] * wv[8]);
    float id = det != 0 ? 1.0f / det : 0.0f;
    for (int r = 0; r < 3; r++) {
        for (int col = 0; col < 3; col++) {
            int r1 = (r + 1) % 3;
            int r2 = (r + 2) % 3;
            int c1 = (col + 1) % 3;
            int c2 = (col + 2) % 3;
            n[r * 4 + col] = (wv[r1 * 4 + c1] * wv[r2 * 4 + c2] - wv[r1 * 4 + c2] * wv[r2 * 4 + c1]) * id;
        }
    }
}

constexpr size_t kTsView = 2;
constexpr size_t kTsProjection = 3;
constexpr size_t kTsTexture0 = 16;
constexpr size_t kTsWorld = 256;

void FillTransformConstants(const State& s, GuestExtent size, DrawConstants& c) {
    uint32_t gw = size.w;
    uint32_t gh = size.h;
    float wv[16];
    float wvp[16];
    MatMul(s.transforms[kTsWorld].data(), s.transforms[kTsView].data(), wv);
    MatMul(wv, s.transforms[kTsProjection].data(), wvp);
    const auto& v = s.viewport;
    float sx = float(v.w) / gw;
    float sy = float(v.h) / gh;
    float ox = (2.0f * v.x + v.w) / gw - 1;
    float oy = 1 - (2.0f * v.y + v.h) / gh;
    float vpm[16] = {sx, 0, 0, 0, 0, sy, 0, 0, 0, 0, v.max_z - v.min_z, 0, ox, oy, v.min_z, 1};
    float full[16];
    MatMul(wvp, vpm, full);
    Transpose(full, c.wvp);
    Transpose(wv, c.wv);
    // normals take the inverse transpose of the upper 3x3
    float n[16] = {};
    InverseTranspose3x3(wv, n);
    Transpose(n, c.wvn);
    for (int t = 0; t < 4; t++) Transpose(s.transforms[kTsTexture0 + size_t(t)].data(), c.tm[t]);
}

// D3DLIGHT8: Type, Diffuse4, Specular4, Ambient4, Position3, Direction3, Range, Falloff, Attenuation3, Theta, Phi.
constexpr int kLightFloats = 26;
constexpr int kLightDiffuse = 1;
constexpr int kLightSpecular = 5;
constexpr int kLightAmbient = 9;
constexpr int kLightPosition = 13;
constexpr int kLightDirection = 16;
constexpr int kLightRange = 19;
constexpr int kLightFalloff = 20;
constexpr int kLightAttenuation0 = 21;
constexpr int kLightAttenuation1 = 22;
constexpr int kLightAttenuation2 = 23;
constexpr int kLightTheta = 24;
constexpr int kLightPhi = 25;

void LightToView(const std::array<uint32_t, kLightFloats>& l, const float* vm, float (&o)[7][4]) {
    float f[kLightFloats];
    std::memcpy(f, l.data(), sizeof(f));
    for (int j = 0; j < 4; j++) {
        o[0][j] = f[kLightDiffuse + j];
        o[1][j] = f[kLightSpecular + j];
        o[2][j] = f[kLightAmbient + j];
    }
    const float* pos = f + kLightPosition;
    const float* dir = f + kLightDirection;
    for (int j = 0; j < 3; j++) {
        o[3][j] = pos[0] * vm[j] + pos[1] * vm[4 + j] + pos[2] * vm[8 + j] + vm[12 + j];
        o[4][j] = dir[0] * vm[j] + dir[1] * vm[4 + j] + dir[2] * vm[8 + j];
    }
    o[3][3] = f[kLightRange];
    o[4][3] = f[kLightFalloff];
    o[5][0] = f[kLightAttenuation0];
    o[5][1] = f[kLightAttenuation1];
    o[5][2] = f[kLightAttenuation2];
    o[6][0] = std::cos(f[kLightTheta] * 0.5f);
    o[6][1] = std::cos(f[kLightPhi] * 0.5f);
}

void FillLightConstants(const State& s, const FixedKey& fk, DrawConstants& c) {
    std::memcpy(c.mat, s.material.data(), sizeof(float) * 16);
    c.mat[4][0] = s.material[16];
    Color(s.rs[kRsAmbient], c.amb);
    const float* vm = s.transforms[kTsView].data();
    for (int i = 0; i < 8; i++) {
        if (!((fk.types >> (i * 2)) & 3)) continue;
        LightToView(s.lights[size_t(i)], vm, c.lt[i]);
    }
}

void VulkanBackend::UploadConstants(const State& s, const DrawShaders& sh, const PsoKey& k, const DrawTextures& stages,
                                    GuestExtent size, DrawConstants& c, Bindings& b) {
    Alloc vc = Upload(sizeof(s.vsc), 256);
    std::memcpy(vc.cpu, s.vsc.data(), sizeof(s.vsc));
    Alloc dc = Upload(sizeof(DrawConstants), 256);
    FillSurfaceConstants(s, size, c);
    // Scale point size to host pixels without classifying edge points as screen passes.
    if (k.topo == uint32_t(Topology::Points)) c.surf[3] = PointScale();
    FillFogConstants(s, sh.prog, c);
    FillCombinerConstants(s, c);
    FillTextureConstants(s, stages, c);
    if (sh.fixed_transform) {
        FillTransformConstants(s, size, c);
        FillLightConstants(s, sh.fk, c);
    }
    std::memcpy(dc.cpu, &c, sizeof(c));
    b.ubo[0] = vc.buf;
    b.offset[0] = vc.off;
    b.range[0] = sizeof(s.vsc);
    b.ubo[1] = dc.buf;
    b.offset[1] = dc.off;
    b.range[1] = sizeof(DrawConstants);
}

// Clamp single-tile sprite UVs so host half-pixel sampling does not blend the opposite edge.
bool UnitTexcoords(const Assembled& a, const DrawTextures& stages) {
    if (openshao::UnitSpriteTexcoords(a)) {
        bool guarded = true;
        for (int i = 0; i < 4; ++i)
            if (stages.bound[i]) guarded &= !stages.rt[i] && stages.info[i].kind == kTex2D && !stages.info[i].rect;
        if (guarded) return true;
    }
    constexpr float kLo = -1e-4f;
    constexpr float kHi = 1.0001f;
    constexpr uint32_t kSmallDrawVertices = 4096;  // most vertices a small draw has
    if (!a.vertices.empty() || !a.vertex_count || a.vertex_count > kSmallDrawVertices) return false;
    bool any = false;
    for (int r = 0; r < 16; r++) {
        const VertexElement& e = a.elements[size_t(r)];
        if (!(a.regs >> r & 1) || e.format != DXGI_FORMAT_R32G32_FLOAT) continue;
        const VertexBuffer& vb = a.buffers[e.buffer];
        if (!vb.src || !vb.stride || uint64_t(a.vertex_count) * vb.stride > vb.bytes) return false;
        for (uint32_t v = 0; v < a.vertex_count; v++) {
            float uv[2];
            std::memcpy(uv, vb.src + size_t(v) * vb.stride + e.offset, sizeof(uv));
            if (!(uv[0] >= kLo && uv[0] <= kHi && uv[1] >= kLo && uv[1] <= kHi)) return false;
        }
        any = true;
    }
    return any;
}

void VulkanBackend::SetTextureViewsAndSamplers(const State& s, const Assembled& a, DrawTextures& stages, Bindings& b) {
    for (int i = 0; i < 4; i++) {
        if (stages.bound[i]) {
            b.image[i] = stages.view[i];
        } else {
            uint8_t kind = stages.info[i].kind;
            b.image[i] = dummy_[kind < 4 ? kind : kTex2D]->view;
        }
    }
    EnsureStageSamplers(s, stages, UnitTexcoords(a, stages), b.sampler);
}

// viewport covers the target, the guest viewport is in the shader; scissor clips to it
void VulkanBackend::SetViewportScissor(const State& s, uint32_t gw) {
    float W = float(bound_rt_ ? bound_rt_->w : bound_ds_->w);
    float H = float(bound_rt_ ? bound_rt_->h : bound_ds_->h);
    SetViewport(0, 0, W, H);
    float sc = W / gw;
    int32_t x0 = int32_t(s.viewport.x * sc);
    int32_t y0 = int32_t(s.viewport.y * sc);
    int32_t x1 = int32_t((s.viewport.x + s.viewport.w) * sc);
    int32_t y1 = int32_t((s.viewport.y + s.viewport.h) * sc);
    if (s.scissor_count) {
        x0 = std::max(x0, int32_t(s.scissors[0] * sc));
        y0 = std::max(y0, int32_t(s.scissors[1] * sc));
        x1 = std::min(x1, int32_t(s.scissors[2] * sc));
        y1 = std::min(y1, int32_t(s.scissors[3] * sc));
    }
    SetScissor(std::clamp<int32_t>(x0, 0, int32_t(W)), std::clamp<int32_t>(y0, 0, int32_t(H)),
               std::clamp<int32_t>(x1, 0, int32_t(W)), std::clamp<int32_t>(y1, 0, int32_t(H)));
}

void VulkanBackend::SetBlendFactorStencilRef(const State& s) {
    float bf[4];
    Color(SimpleState(s, kMBlendColor), bf);
    vk.vkCmdSetBlendConstants(Cmd(), bf);
    vk.vkCmdSetStencilReference(Cmd(), VK_STENCIL_FACE_FRONT_AND_BACK, SimpleState(s, kMStencilRef) & 0xFF);
}

void VulkanBackend::UploadVertexBuffers(const Assembled& a) {
    if (!a.buffer_count) return;
    VkBuffer bufs[16];
    VkDeviceSize offs[16];
    for (uint32_t i = 0; i < a.buffer_count; i++) {
        const VertexBuffer& b = a.buffers[i];
        Alloc vb = Upload(std::max<uint32_t>(b.bytes, 16), 16);
        if (b.bytes) std::memcpy(vb.cpu, b.src, b.bytes);
        bufs[i] = vb.buf;
        offs[i] = vb.off;
    }
    vk.vkCmdBindVertexBuffers(Cmd(), 0, a.buffer_count, bufs, offs);
}

void VulkanBackend::DrawGeometry(const Assembled& a) {
    UploadVertexBuffers(a);
    if (a.index_count) {
        uint32_t ibytes = a.index_count * (a.index16 ? 2 : 4);
        Alloc ib = Upload(ibytes, 4);
        std::memcpy(ib.cpu, a.index_src, ibytes);
        vk.vkCmdBindIndexBuffer(Cmd(), ib.buf, ib.off, a.index16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
        vk.vkCmdDrawIndexed(Cmd(), a.index_count, 1, 0, a.base_vertex, 0);
    } else {
        vk.vkCmdDraw(Cmd(), a.draw_count, 1, 0, 0);
    }
}

bool VulkanBackend::AssembleDraw(const State& s, uint32_t prim, uint32_t start, uint32_t count,
                                 const uint8_t* indices, Assembled& a) {
    auto perf_t = PerfClock::now();
    bool assembled = Assemble(s, prim, start, count, indices, a);
    g_perf.assemble += PerfMs(perf_t);
    g_perf.verts += a.vertex_count;
    g_perf.raw += a.vertices.empty();
    return assembled;
}

bool VulkanBackend::EnsureQueries() {
    if (query_pool_) return true;
    if (queries_failed_) return false;
    VkQueryPoolCreateInfo qi{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    qi.queryType = VK_QUERY_TYPE_OCCLUSION;
    qi.queryCount = kQueries;
    if (vk.vkCreateQueryPool(dev_, &qi, nullptr, &query_pool_) != VK_SUCCESS) {
        query_pool_ = VK_NULL_HANDLE;
        queries_failed_ = true;
        LOG_FIRST(1, "vulkan: no occlusion queries, visibility tests stay pending");
        return false;
    }
    return true;
}

// Reset occlusion queries outside rendering, then begin each counted draw inside it.
uint32_t VulkanBackend::BeginCountedDraw(const State& s) {
    if (!s.simple[kMCountEnable >> 2] || !EnsureQueries()) return kNoQuery;
    uint32_t query = query_next_++ % kQueries;
    CloseRendering();
    vk.vkCmdResetQueryPool(Cmd(), query_pool_, query, 1);
    return query;
}

void VulkanBackend::EndCountedDraw(uint32_t query) {
    if (query == kNoQuery) return;
    vk.vkCmdEndQuery(Cmd(), query_pool_, query);
    counted_.push_back({query, SamplesPerPixel()});
}

// the console counts its own pixels: the bound target's scale comes out
float VulkanBackend::SamplesPerPixel() const {
    const Target* t = bound_rt_ ? bound_rt_ : bound_ds_;
    if (!t || !t->gw || !t->gh) return 1;
    return float(t->w) * float(t->h) / (float(t->gw) * float(t->gh));
}

// Publish completed guest reports in GPU order: time, count, then zero for done.
void VulkanBackend::DeliverReports() {
    constexpr uint32_t kTime = 0;
    constexpr uint32_t kTimeHigh = 4;
    constexpr uint32_t kCount = 8;
    constexpr uint32_t kStatus = 12;
    uint64_t done = CompletedFence();
    while (!reports_.empty() && reports_.front().fence <= done) {
        const Report& r = reports_.front();
        double pixels = 0;
        for (const Query& q : r.queries) {
            uint64_t v = 0;
            vk.vkGetQueryPoolResults(dev_, query_pool_, q.index, 1, sizeof(v), &v, sizeof(v), VK_QUERY_RESULT_64_BIT);
            pixels += double(v) / q.samples_per_pixel;
        }
        kernel::W32(r.va + kTime, uint32_t(presents_));
        kernel::W32(r.va + kTimeHigh, 0);
        kernel::W32(r.va + kCount, uint32_t(std::lround(pixels)));
        std::atomic_thread_fence(std::memory_order_release);
        kernel::W32(r.va + kStatus, 0);
        reports_.pop_front();
    }
}

void VulkanBackend::Draw(const State& s, uint32_t prim, uint32_t start, uint32_t count, const uint8_t* indices) {
    PerfScope perf_draw{g_perf.draw};
    g_perf.draws++;
    if (DepthCopy(s)) return;
    Assembled& a = asm_;
    if (!AssembleDraw(s, prim, start, count, indices, a)) return;
    // Each overflow upload needs a separate buffer; avoid wrapping the ring within a large draw.
    if (CurrentFrame().used > kUploadSize - kUploadReserve) Submit(true);
    if (!BindTargets(s)) return;

    DrawShaders sh = ClassifyShaders(s, a, count);
    if (sh.prog) EnsureProgramHash(s);
    sh.vkey = VertexKey(s, a, sh);
    EnsureVertexShader(s, a, sh);

    DrawTextures stages;
    if (!BindStageTexturesAndTargets(s, stages)) return;
    if (!openshao::launcher::Current().bloom_effect &&
        openshao::IsOriginalBloomEffectComposite(s, prim, count, stages.rt[0])) return;
    PixelKey pk = MakePixelKey(s, stages.info);
    pk.quad = sh.quad;
    sh.pkey = Hash(&pk, sizeof(pk));
    EnsurePixelShader(pk, sh);
    if (!sh.vs_code || !sh.ps_code) return;

    PsoKey k;
    BuildPipelineKey(s, a, sh.vh, sh.ph, k);
    bool atlas = false, target_texture = false;
    for (int i = 0; i < 4; ++i) {
        if (!stages.bound[i]) continue;
        target_texture |= stages.rt[i] || stages.info[i].kind != kTex2D || stages.info[i].rect;
        atlas |= !stages.rt[i] && stages.info[i].kind == kTex2D;
    }
    k.atlas_guard = sh.quad && a.topology == Topology::Triangles && atlas && !target_texture;
    VkPipeline pso = EnsurePipeline(k, sh.vs_code, sh.ps_code);
    if (!pso) return;
    if (pso_first_use_) {
        openshao::pipelines::ObserveAssetPipeline(Hash(&k, sizeof(k)), pso_compiled_on_draw_);
        RecordPipeline(s, a, sh, pk, k);
    }

    GuestExtent size;
    SurfaceSize(bound_rt_ ? s.rt : s.zs, size.w, size.h);
    DrawConstants c{};
    Bindings b;
    UploadConstants(s, sh, k, stages, size, c, b);
    SetTextureViewsAndSamplers(s, a, stages, b);

    uint32_t query = BeginCountedDraw(s);
    EnsureRendering();
    vk.vkCmdBindPipeline(Cmd(), VK_PIPELINE_BIND_POINT_GRAPHICS, pso);
    BindSet(b);
    SetViewportScissor(s, size.w);
    SetBlendFactorStencilRef(s);
    if (query != kNoQuery) {
        vk.vkCmdBeginQuery(Cmd(), query_pool_, query, precise_ok_ ? VK_QUERY_CONTROL_PRECISE_BIT : 0);
    }
    DrawGeometry(a);
    EndCountedDraw(query);
}


void ScaledRect(const Target& t, int x0, int y0, int x1, int y1, std::vector<VkClearRect>& out) {
    float sc = float(t.w) / t.gw;
    int32_t l = std::clamp<int32_t>(int32_t(x0 * sc), 0, int32_t(t.w));
    int32_t tp = std::clamp<int32_t>(int32_t(y0 * sc), 0, int32_t(t.h));
    int32_t r = std::clamp<int32_t>(int32_t(x1 * sc), 0, int32_t(t.w));
    int32_t b = std::clamp<int32_t>(int32_t(y1 * sc), 0, int32_t(t.h));
    if (r <= l || b <= tp) return;
    out.push_back({{{l, tp}, {uint32_t(r - l), uint32_t(b - tp)}}, 0, 1});
}

void VulkanBackend::ClearTarget(Target& t, const State& s, const ClearArgs& c, std::vector<VkClearRect>& rs) {
    rs.clear();
    if (c.count && c.rects) {
        for (uint32_t i = 0; i < c.count; i++) {
            int32_t r[4];
            std::memcpy(r, c.rects + size_t(i) * sizeof(r), sizeof(r));
            ScaledRect(t, r[0], r[1], r[2], r[3], rs);
        }
    } else {
        const auto& v = s.viewport;
        ScaledRect(t, int(v.x), int(v.y), int(v.x + v.w), int(v.y + v.h), rs);
    }
    t.drawn = true;
    if (rs.empty()) return;
    VkClearAttachment ca{};
    if (t.depth) {
        Transition(*t.res, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        if (c.flags & kClearZ) ca.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
        if ((c.flags & kClearStencil) && HasStencil(t.res->format)) ca.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        if (!ca.aspectMask) return;
        ca.clearValue.depthStencil = {std::clamp(c.z, 0.0f, 1.0f), c.stencil & 0xFF};
        SetAttachments(nullptr, t.res.get(), t.w, t.h);
    } else {
        // Upload pending CPU writes before clearing, or a later upload would overwrite the clear.
        if (LatchCpuWrite(t)) UploadTarget(t, s.rt);
        Transition(*t.res, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        float col[4];
        Color(c.color, col);
        for (int i = 0; t.range != 1 && i < 3; i++) col[i] /= t.range;
        ca.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ca.colorAttachment = 0;
        std::memcpy(ca.clearValue.color.float32, col, sizeof(col));
        SetAttachments(t.res.get(), nullptr, t.w, t.h);
    }
    EnsureRendering();
    vk.vkCmdClearAttachments(Cmd(), 1, &ca, uint32_t(rs.size()), rs.data());
}

void VulkanBackend::Clear(const State& s, uint32_t count, const uint8_t* rects, uint32_t flags, uint32_t color, float z,
                          uint32_t stencil) {
    PerfScope perf_clear{g_perf.clear};
    Target* rt = (flags & kClearTarget) ? EnsureTarget(s.rt, false) : nullptr;
    Target* ds = (flags & (kClearZ | kClearStencil)) ? EnsureTarget(s.zs, true) : nullptr;
    std::vector<VkClearRect> rs;
    ClearArgs c{.count = count, .rects = rects, .flags = flags, .color = color, .z = z, .stencil = stencil};
    for (Target* t : {rt, ds}) {
        if (t) ClearTarget(*t, s, c, rs);
    }
}

// CopyRects' guest lists: src_rects RECTs, dst_points POINTs, all int32
constexpr uint32_t kGuestRectBytes = 16;
constexpr uint32_t kGuestPointBytes = 8;

struct GuestRect {
    int32_t left, top, right, bottom;
};
struct GuestPoint {
    int32_t x, y;
};

GuestRect ReadGuestRect(uint32_t rects, uint32_t i) {
    uint32_t r = rects + i * kGuestRectBytes;
    return {int32_t(kernel::R32(r)), int32_t(kernel::R32(r + 4)), int32_t(kernel::R32(r + 8)),
            int32_t(kernel::R32(r + 12))};
}

GuestPoint ReadGuestPoint(uint32_t points, uint32_t i) {
    uint32_t p = points + i * kGuestPointBytes;
    return {int32_t(kernel::R32(p)), int32_t(kernel::R32(p + 4))};
}

struct DrawnRect {
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0, dx = 0, dy = 0;
};

// without dst_points the rect lands where it is
DrawnRect ReadDrawnRect(uint32_t src_rects, uint32_t dst_points, uint32_t i) {
    GuestRect g = ReadGuestRect(src_rects, i);
    DrawnRect r;
    r.x0 = float(g.left);
    r.y0 = float(g.top);
    r.x1 = float(g.right);
    r.y1 = float(g.bottom);
    r.dx = r.x0;
    r.dy = r.y0;
    if (dst_points) {
        GuestPoint p = ReadGuestPoint(dst_points, i);
        r.dx = float(p.x);
        r.dy = float(p.y);
    }
    return r;
}

uint32_t ScaleUnsigned(int32_t v, float sc) { return uint32_t(uint32_t(v) * sc); }

// a drawn copy's taps round down a ratio a hair over a whole number
constexpr float kTapsSlack = 1e-3f;

void VulkanBackend::CopyByDraw(Target& src_t, Target& dst_t, const CopyList& l) {
    Transition(*src_t.res, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    Transition(*dst_t.res, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    SetAttachments(dst_t.res.get(), nullptr, dst_t.w, dst_t.h);
    float src_scale = float(src_t.w) / src_t.gw;
    float dst_scale = float(dst_t.w) / dst_t.gw;
    bool listed = l.count && l.src_rects;
    uint32_t n = listed ? l.count : 1;
    for (uint32_t i = 0; i < n; i++) {
        float whole_w = float(std::min(src_t.gw, dst_t.gw));
        float whole_h = float(std::min(src_t.gh, dst_t.gh));
        DrawnRect r{.x0 = 0, .y0 = 0, .x1 = whole_w, .y1 = whole_h};
        if (listed) r = ReadDrawnRect(l.src_rects, l.dst_points, i);
        if (r.x1 <= r.x0 || r.y1 <= r.y0) continue;
        float vx = r.dx * dst_scale, vy = r.dy * dst_scale;
        float vw = (r.x1 - r.x0) * dst_scale, vh = (r.y1 - r.y0) * dst_scale;
        int32_t right = std::min(int32_t(vx + vw), int32_t(dst_t.w));
        int32_t bottom = std::min(int32_t(vy + vh), int32_t(dst_t.h));
        if (right <= int32_t(vx) || bottom <= int32_t(vy)) continue;
        SetViewport(vx, vy, vw, vh);
        SetScissor(int32_t(vx), int32_t(vy), right, bottom);
        float uv[4] = {r.x0 / src_t.gw, r.y0 / src_t.gh, (r.x1 - r.x0) / src_t.gw, (r.y1 - r.y0) / src_t.gh};
        uint32_t taps = uint32_t(std::ceil(std::max(src_scale / dst_scale, 1.0f) - kTapsSlack));
        Blit(*src_t.res, uv, {.alpha = true, .taps = taps, .mul = 1 / dst_t.range, .rtv = dst_t.fmt});
    }
    dst_t.drawn = true;
    bound_rt_ = bound_ds_ = nullptr;
}

void VulkanBackend::CopyByRegion(Target& src_t, Target& dst_t, const CopyList& l) {
    Transition(*src_t.res, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    Transition(*dst_t.res, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    float dst_scale = float(dst_t.w) / dst_t.gw;
    bool listed = l.count && l.src_rects;
    uint32_t n = listed ? l.count : 1;
    std::vector<VkImageCopy> regions;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t left = 0, top = 0, right = std::min(src_t.w, dst_t.w), bottom = std::min(src_t.h, dst_t.h);
        uint32_t dx = 0;
        uint32_t dy = 0;
        if (listed) {
            GuestRect r = ReadGuestRect(l.src_rects, i);
            left = ScaleUnsigned(r.left, dst_scale);
            top = ScaleUnsigned(r.top, dst_scale);
            right = std::min(ScaleUnsigned(r.right, dst_scale), src_t.w);
            bottom = std::min(ScaleUnsigned(r.bottom, dst_scale), src_t.h);
            dx = left;
            dy = top;
            if (l.dst_points) {
                GuestPoint p = ReadGuestPoint(l.dst_points, i);
                dx = ScaleUnsigned(p.x, dst_scale);
                dy = ScaleUnsigned(p.y, dst_scale);
            }
        }
        if (right <= left || bottom <= top || dx >= dst_t.w || dy >= dst_t.h) continue;
        right = std::min(right, left + dst_t.w - dx);
        bottom = std::min(bottom, top + dst_t.h - dy);
        VkImageCopy c{};
        c.srcSubresource = c.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        c.srcOffset = {int32_t(left), int32_t(top), 0};
        c.dstOffset = {int32_t(dx), int32_t(dy), 0};
        c.extent = {right - left, bottom - top, 1};
        regions.push_back(c);
    }
    if (!regions.empty()) {
        vk.vkCmdCopyImage(Cmd(), src_t.res->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst_t.res->image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, uint32_t(regions.size()), regions.data());
    }
    dst_t.drawn = true;
    bound_rt_ = bound_ds_ = nullptr;
}

void VulkanBackend::Copy(const Surface& src, const Surface& dst, uint32_t count, uint32_t src_rects,
                         uint32_t dst_points) {
    PerfScope perf_copy{g_perf.copy};
    Target* src_t = FindTarget(src, false);
    // pixels the CPU made: the copy happens in memory, textures and targets pick it up from there
    if (!src_t || !src_t->drawn) {
        if (!CopySurfaceMemory(src, dst, count, src_rects, dst_points)) {
            LOG_FIRST(8, "vulkan: CopyRects %08X to %08X skipped", src.data, dst.data);
        } else if (Target* dst_t = FindTarget(dst, false)) {
            UploadTarget(*dst_t, dst);
        }
        return;
    }
    Target* dst_t = EnsureTarget(dst, false);
    if (!dst_t || src_t == dst_t) return;
    CopyList l{.count = count, .src_rects = src_rects, .dst_points = dst_points};
    if (src_t->fmt != dst_t->fmt) {
        CopyByDraw(*src_t, *dst_t, l);
        return;
    }
    CopyByRegion(*src_t, *dst_t, l);
}


// On X11 use the surface extent because SDL may lag mode changes; on Wayland use SDL size.
void VulkanBackend::ResizeSwap() {
    int pw = 0, ph = 0;
    vkplatform::WindowSize(pw,ph);
    bool vsync = app::Get().vsync;
    static int last_w = 0, last_h = 0;
    bool sdl_changed = pw != last_w || ph != last_h;
    last_w = pw;
    last_h = ph;
    if (!swap_dirty_ && !sdl_changed && vsync == swap_vsync_ && swap_ && presents_ % 15 != 0) return;
    VkSurfaceCapabilitiesKHR caps{};
    if (vk.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu_, surface_, &caps) != VK_SUCCESS) return;
    uint32_t w = caps.currentExtent.width, h = caps.currentExtent.height;
    if (w == 0xFFFFFFFFu) {
        w = uint32_t(std::max(pw, 0));
        h = uint32_t(std::max(ph, 0));
    }
    if (!w || !h) return;  // minimised
    if (!swap_dirty_ && swap_ && w == bb_w_ && h == bb_h_ && vsync == swap_vsync_) return;
    Flush();
    vk.vkDeviceWaitIdle(dev_);
    swap_vsync_ = vsync;
    if (!CreateSwapchain()) swap_dirty_ = true;
}

constexpr int kRampRed = 0;
constexpr int kRampGreen = 256;
constexpr int kRampBlue = 512;

void VulkanBackend::UploadGammaRamp() {
    constexpr uint32_t kRampBytes = 256 * 4;  // one BGRA texel per ramp entry
    ramp_dirty_ = false;
    Alloc a = Upload(kRampBytes, 16);
    bool identity = true;
    for (int i = 0; i < 256; i++) {
        uint8_t* p = a.cpu + i * 4;
        p[0] = ramp_[kRampBlue + i];
        p[1] = ramp_[kRampGreen + i];
        p[2] = ramp_[kRampRed + i];
        p[3] = 255;
        identity &= ramp_[kRampRed + i] == i && ramp_[kRampGreen + i] == i && ramp_[kRampBlue + i] == i;
    }
    gamma_ = !identity;
    Transition(*lut_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy r{};
    r.bufferOffset = a.off;
    r.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    r.imageExtent = {256, 1, 1};
    vk.vkCmdCopyBufferToImage(Cmd(), a.buf, lut_->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &r);
}

void VulkanBackend::BlitFrame(Target& t) {
    float aspect = openshao::aspect::Output(bb_w_,bb_h_);
    float w = float(bb_w_);
    float h = float(bb_h_);
    float vw = std::min(w, h * aspect);
    float vh = vw / aspect;
    float uv[4] = {0, 0, 1, 1};
    SetViewport((w - vw) / 2, (h - vh) / 2, vw, vh);
    SetScissor(0, 0, int32_t(bb_w_), int32_t(bb_h_));
    Blit(*t.res, uv, {.gamma = gamma_, .taps = openshao::sfd::PresentationTaps(), .rtv = swap_format_});
}

void VulkanBackend::RenderUi() {
    if (!ui_ || openshao::options.background) return;
    std::lock_guard lock(app::UiLock());
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(float(bb_w_), float(bb_h_));
    auto now = PerfClock::now();
    io.DeltaTime = std::max(1e-4f, std::chrono::duration<float>(now - ui_t_).count());
    ui_t_ = now;
    ImGui_ImplVulkan_NewFrame();
#ifdef _WIN32
    ImGui_ImplWin32_NewFrame();
#endif
    ImGui::NewFrame();
    if (app::kGame.ui) app::kGame.ui(fps_);
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), Cmd());
}

void VulkanBackend::DrawFrame(Target* t, uint32_t bi) {
    Image& bb = *swap_images_[bi];
    if (t) Transition(*t->res, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    bb.layout = VK_IMAGE_LAYOUT_UNDEFINED;  // cleared below, nothing kept
    Transition(bb, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    CloseRendering();
    VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    color.imageView = bb.view;
    color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.clearValue.color = {{0, 0, 0, 1}};
    VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
    ri.renderArea = {{0, 0}, {bb_w_, bb_h_}};
    ri.layerCount = 1;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments = &color;
    vk.vkCmdBeginRendering(Cmd(), &ri);
    rendering_ = true;
    pending_ = open_ = {&bb, nullptr, bb_w_, bb_h_};
    bound_rt_ = bound_ds_ = nullptr;
    if (t) BlitFrame(*t);
    RenderUi();
    Transition(bb, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    pending_ = open_ = {};
}

void VulkanBackend::ReleaseOldTextures() {
    constexpr uint64_t kSweepEvery = 60;    // presents between sweeps
    constexpr size_t kTexturesKept = 2048;  // no sweep below this many
    constexpr uint64_t kIdleFrames = 600;
    // textures unused for ten seconds go, levels stream theirs in and out
    if (presents_ % kSweepEvery != 0 || textures_.size() <= kTexturesKept) return;
    for (auto it = textures_.begin(); it != textures_.end();) {
        if (it->second.frame + kIdleFrames < presents_) {
            CurrentFrame().garbage.push_back(it->second.res);
            it = textures_.erase(it);
        } else {
            ++it;
        }
    }
}

void VulkanBackend::ResetFrame() {
    Frame& f = CurrentFrame();
    if (f.fence && CompletedFence() < f.fence) {
        auto perf_wait = PerfClock::now();
        WaitFence(f.fence);
        g_perf.wait += PerfMs(perf_wait);
    }
    ResetFrameObjects(f);
    Begin();
}

void VulkanBackend::CountFps() {
    fps_n_++;
    auto now = PerfClock::now();
    double el = std::chrono::duration<double>(now - fps_t_).count();
    if (el >= 0.5) {
        fps_ = fps_n_ / el;
        fps_n_ = 0;
        fps_t_ = now;
    }
}

// What the texture pack costs, every five seconds, and where the player marked the scenes (F10)
void LogTexturePack() {
    texpack::EndFrame();
    texpack::Interval span;
    if (texpack::TakeInterval(300, span) && (span.resident || span.loads || span.failures)) {
        const double loads = double(std::max<uint64_t>(span.loads, 1));
        LOGI("Texture pack: %llu frames: %llu loads, %llu failed, read %.2f ms and upload %.2f ms per load, worst frame %.2f ms "
             "of loading, %u textures %.0f MiB",
             static_cast<unsigned long long>(span.frames), static_cast<unsigned long long>(span.loads),
             static_cast<unsigned long long>(span.failures), double(span.disk_us) / 1000.0 / loads,
             double(span.upload_us) / 1000.0 / loads, double(span.worst_frame_us) / 1000.0, span.resident,
             double(span.resident_bytes) / 1048576.0);
    }
    uint32_t mark = 0;
    if (texpack::TakeMarker(mark)) {
        const auto stats = texpack::GetStats();
        LOGI("Texture pack: marker %u: %u textures %.0f MiB, %llu loads in all", mark, stats.resident,
             double(stats.resident_bytes) / 1048576.0, static_cast<unsigned long long>(stats.loads));
    }
}

void LogPerf() {
    static PerfClock::time_point perf_start = PerfClock::now();
    static uint64_t perf_frames = 0;
    perf_frames++;
    double el = PerfMs(perf_start);
    if (el < 1000.0) return;
    Perf& p = g_perf;
    double n = double(perf_frames);
    double host = p.draw + p.clear + p.copy + p.present + p.sync;
    LOGI("perf: %.1f fps %.2f ms/frame | host %.2f (draw %.2f: assemble %.2f bind %.2f [check %.2f, upload %.2f] "
         "compile %.2f; clear %.2f copy %.2f sync %.2f present %.2f [swap %.2f wait %.2f]) other %.2f",
         n * 1000.0 / el, el / n, host / n, p.draw / n, p.assemble / n, p.bind / n, p.sig / n, p.texup / n,
         p.compile / n, p.clear / n, p.copy / n, p.sync / n, p.present / n, p.swap / n, p.wait / n, (el - host) / n);
    LOGI("perf: per frame %.0f draws (%.0f raw) %.0fk verts %.1f MB upload %.1f overflow | %.1f tex uploads %.2f MB, "
         "%.1f new tex, %.1f new targets, %.1f target uploads, %.1f shrinks, %.1f read-backs %.1f late | %llu shaders "
         "%llu psos this second, %u allocations",
         p.draws / n, p.raw / n, p.verts / n / 1000.0, p.upload / n / 1048576.0, p.overflow / n, p.texups / n,
         p.texup_bytes / n / 1048576.0, p.newtex / n, p.targets / n, p.rtups / n, p.shrinks / n, p.syncs / n,
         p.lates / n, (unsigned long long)p.shaders, (unsigned long long)p.psos, g_memory_objects);
    g_perf = Perf{};
    perf_start = PerfClock::now();
    perf_frames = 0;
}

void VulkanBackend::Present(const Surface& front) {
    PerfScope perf_present{g_perf.present};
    const bool capture=g_capture.exchange(false);
    ResizeSwap();
    Target* t = EnsureTarget(front, false);
    if (t && LatchCpuWrite(*t)) UploadTarget(*t, front);
    if(capture && t)CaptureFrame(*t);
    if (ramp_dirty_) UploadGammaRamp();
    Transition(*lut_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    Frame& f = CurrentFrame();
    uint32_t bi = 0;
    bool acquired = false;
    if (swap_ && !swap_dirty_) {
        VkResult r = vk.vkAcquireNextImageKHR(dev_, swap_, UINT64_MAX, f.acquire, VK_NULL_HANDLE, &bi);
        acquired = r == VK_SUCCESS || r == VK_SUBOPTIMAL_KHR;
        if (r == VK_SUBOPTIMAL_KHR || r == VK_ERROR_OUT_OF_DATE_KHR) swap_dirty_ = true;
        else if (!acquired) LOG_FIRST(8, "vulkan: acquire failed: %s", vkapi::ResultName(r));
    }
    if (acquired) DrawFrame(t, bi);
    Submit(false, acquired ? f.acquire : VK_NULL_HANDLE, acquired ? swap_done_[bi] : VK_NULL_HANDLE);
    present_fences_[presents_ % (kFrames + 1)].store(fence_value_, std::memory_order_release);
    DeliverReports();
    if (acquired) {
        auto perf_swap = PerfClock::now();
        VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        pi.waitSemaphoreCount = 1;
        pi.pWaitSemaphores = &swap_done_[bi];
        pi.swapchainCount = 1;
        pi.pSwapchains = &swap_;
        pi.pImageIndices = &bi;
        VkResult r = vk.vkQueuePresentKHR(queue_, &pi);
        if (r == VK_SUBOPTIMAL_KHR || r == VK_ERROR_OUT_OF_DATE_KHR) swap_dirty_ = true;
        else if (r != VK_SUCCESS) LOG_FIRST(8, "vulkan: present failed: %s", vkapi::ResultName(r));
        g_perf.swap += PerfMs(perf_swap);
    }

    presents_++;
    presents_seen_.store(presents_);
    frame_++;
    ReleaseOldTextures();
    ResetFrame();
    CountFps();
    if (diag::Perf()) LogPerf();
    LogTexturePack();
    ApplySettings();
    if (presents_ % 1800 == 0 && psos_.size() != cache_saved_psos_) {
        cache_saved_psos_ = psos_.size();
        SavePipelineCache();
    }
}

void VulkanBackend::ApplySettings() {
    float want = app::RenderScale(guest_h_);
    if (std::fabs(want - scale_) > 1e-4f) {
        Flush();
        targets_.clear();
        scale_ = want;
        openshao::pipelines::SetPipelineRenderScale(scale_);
        bound_rt_ = bound_ds_ = nullptr;
        pending_ = open_ = {};
    }
}


VulkanBackend::CopySource VulkanBackend::PrepareCopySource(Target& t, const ShrinkOptions& o) {
    CopySource out{t.res.get(), nullptr};
    if (t.w != t.gw || t.h != t.gh || t.fmt != kBgra) {
        out.down = Shrink(*t.res, t.gw, t.gh, o);
        out.res = out.down.get();
    } else {
        Transition(*t.res, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    }
    return out;
}

// long unused: the title freed it and other data is at this address now
bool VulkanBackend::TargetStale(uint32_t data) const {
    auto it = target_used_.find(data);
    uint64_t used = it == target_used_.end() ? 0 : it->second;
    return frame_ - used > kStaleFrames;
}

void VulkanBackend::Sync(uint32_t data, uint32_t) {
    auto it = targets_.find(TargetMapKey(data, false));
    if (it == targets_.end() || !it->second.drawn || TargetStale(data)) return;
    PerfScope perf_sync{g_perf.sync};
    g_perf.syncs++;
    Target& t = it->second;
    CopySource src = PrepareCopySource(t, {});
    if (!src.res) return;
    std::vector<uint8_t> px;
    if (!ReadBack(*src.res, t.gw, t.gh, px)) return;
    // colour targets are 32 bit linear in guest memory, the pitch rounds to 64 bytes
    uint32_t pitch = GuestPitch(t.gw);
    uint8_t* guest = static_cast<uint8_t*>(kernel::HostPtr(data));
    for (uint32_t y = 0; y < t.gh; y++) std::memcpy(guest + size_t(y) * pitch, &px[size_t(y) * t.gw * 4], t.gw * 4);
    t.read = std::move(px);
    t.drawn = false;
    t.sig = MemSig(t.data, t.bytes);
}

// NOOVERWRITE reads the latest completed copy without waiting; occasional locks still Sync.
void VulkanBackend::SyncLate(uint32_t data) {
    if (!DeliverLate(data)) Sync(data, 0);
    RecordLate(data);
}

bool VulkanBackend::DeliverLate(uint32_t data) {
    std::lock_guard lock(late_lock_);
    auto it = late_.find(data);
    if (it == late_.end()) return false;
    Late& l = it->second;
    uint64_t done = CompletedFence();
    uint64_t now = presents_seen_.load();
    int best = -1;
    for (int i = 0; i < kFrames; i++) {
        bool finished = l.fence[i] && l.fence[i] <= done;
        bool recent = now - l.frame[i] <= uint64_t(kFrames) + 1;
        bool newest = best < 0 || l.fence[i] > l.fence[best];
        if (finished && recent && newest) best = i;
    }
    if (best < 0) return false;
    if (l.fence[best] > l.delivered && l.rb[best]) {
        uint32_t pitch = GuestPitch(l.gw);
        uint8_t* dst = static_cast<uint8_t*>(kernel::HostPtr(data));
        const uint8_t* p = l.rb[best]->map;
        for (uint32_t y = 0; y < l.gh; y++) std::memcpy(dst + size_t(y) * pitch, p + size_t(y) * l.gw * 4, l.gw * 4);
        l.delivered = l.fence[best];
    }
    return true;
}

void VulkanBackend::ResetLate(Late& l, const Target& t) {
    auto& garbage = CurrentFrame().garbage;
    for (auto& r : l.rb) {
        if (r) garbage.push_back(r);
    }
    if (l.down) garbage.push_back(l.down);
    l = Late{};
    l.gw = t.gw;
    l.gh = t.gh;
}

Image* VulkanBackend::PrepareLateSource(Target& t, Late& l) {
    ShrinkOptions reuse{.into = l.down};
    CopySource src = PrepareCopySource(t, reuse);
    if (src.down) l.down = src.down;
    return src.res;
}

void VulkanBackend::RecordLateCopy(Late& l, uint32_t slot, Image& src) {
    if (!l.rb[slot]) {
        l.rb[slot] = CreateBuffer(uint64_t(l.gw) * l.gh * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
        if (!l.rb[slot]) return;
    }
    VkBufferImageCopy r{};
    r.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    r.imageExtent = {l.gw, l.gh, 1};
    vk.vkCmdCopyImageToBuffer(Cmd(), src.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, l.rb[slot]->buf, 1, &r);
    l.fence[slot] = fence_value_ + 1;  // the next Submit carries it
    l.frame[slot] = presents_;
    l.next = (slot + 1) % kFrames;
}

void VulkanBackend::RecordLate(uint32_t data) {
    auto it = targets_.find(TargetMapKey(data, false));
    if (it == targets_.end() || TargetStale(data)) return;
    Target& t = it->second;
    PerfScope perf_sync{g_perf.sync};
    g_perf.lates++;
    // Readback pixels already copied into guest memory are not new CPU writes.
    t.sig = MemSig(t.data, t.bytes);
    t.read.clear();
    std::lock_guard lock(late_lock_);
    Late& l = late_[data];
    if (l.gw != t.gw || l.gh != t.gh) ResetLate(l, t);
    // a copy for a later lock, unless the GPU is a whole ring behind
    uint32_t slot = l.next;
    if (l.fence[slot] > CompletedFence()) return;
    Image* src = PrepareLateSource(t, l);
    if (!src) return;
    RecordLateCopy(l, slot, *src);
}

ImageP VulkanBackend::Shrink(Image& src, uint32_t w, uint32_t h, const ShrinkOptions& o) {
    g_perf.shrinks++;
    ImageP down = o.into;
    if (!down || down->w != w || down->h != h) {
        ImageSpec spec;
        spec.w = w;
        spec.h = h;
        spec.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        down = CreateImage(spec);
        if (!down) return nullptr;
    }
    Transition(src, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    SetBlitTarget(*down, w, h);
    float uv[4] = {0, 0, 1, 1};
    Blit(src, uv, {.alpha = true, .taps = o.taps, .mul = o.mul});
    Transition(*down, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    return down;
}

bool VulkanBackend::ReadBack(Image& img, uint32_t w, uint32_t h, std::vector<uint8_t>& out) {
    BufferP rb = CreateBuffer(uint64_t(w) * h * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
    if (!rb) return false;
    Transition(img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkBufferImageCopy r{};
    r.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    r.imageExtent = {w, h, 1};
    vk.vkCmdCopyImageToBuffer(Cmd(), img.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, rb->buf, 1, &r);
    Flush();
    out.assign(rb->map, rb->map + size_t(w) * h * 4);
    return true;
}

void VulkanBackend::CaptureFrame(Target& target) {
    if(diag::CaptureDir().empty()){LOG_FIRST(1,"capture: set XBE_CAPTURE_DIR");return;}
    const auto source=PrepareCopySource(target,{});
    std::vector<uint8_t> pixels;
    if(!source.res || !ReadBack(*source.res,target.gw,target.gh,pixels))return;
    auto surface=SDL_CreateSurfaceFrom(int(target.gw),int(target.gh),
        source.res->format==VK_FORMAT_R8G8B8A8_UNORM?SDL_PIXELFORMAT_RGBA32:SDL_PIXELFORMAT_BGRA32,
        pixels.data(),int(target.gw*4));
    if(!surface){LOGE("vulkan: capture surface failed (%s)",SDL_GetError());return;}
    const auto folder=diag::CaptureDir()/("frame-"+std::to_string(presents_));
    std::error_code error;
    std::filesystem::create_directories(folder,error);
    const auto utf8=(folder/"final.bmp").u8string();
    const std::string filename(utf8.begin(),utf8.end());
    if(error || !SDL_SaveBMP(surface,filename.c_str()))LOGE("vulkan: frame capture failed (%s)",error?error.message().c_str():SDL_GetError());
    else LOGR("vulkan: frame captured to %s",filename.c_str());
    SDL_DestroySurface(surface);
}

void VulkanBackend::SetGamma(const uint8_t* ramp) {
    std::memcpy(ramp_, ramp, sizeof(ramp_));
    ramp_dirty_ = true;
}

// Precompile workers mirror EnsureVertexShader/EnsurePixelShader/EnsurePipeline.
static_assert(sizeof(PsoKey) <= openshao::pipelines::kMaxPipelineKey);

VkShaderModule VulkanBackend::PrecompileModule(const std::string& src, const char* entry, vkshader::Stage stage,
                                               uint64_t& hash) {
    hash = ShaderKey(src, entry, stage);
    openshao::pipelines::InFlightScope flight{pre_shader_flight_, hash};
    {
        std::lock_guard lock(pre_mutex_);
        auto it = pre_modules_.find(hash);
        if (it != pre_modules_.end()) return it->second;
    }
    VkShaderModule module = CompileModule(src, entry, stage);
    if (!module) return VK_NULL_HANDLE;
    std::lock_guard lock(pre_mutex_);
    pre_modules_.try_emplace(hash, module);
    return module;
}

bool VulkanBackend::DescribeCandidate(const openshao::pipelines::PipelineCandidate& c,
                                     openshao::pipelines::Recipe& r) {
    const auto& s = c.state;
    const auto& a = c.layout;
    auto sh = ClassifyShaders(s, a, c.count);
    PixelKey pk = MakePixelKey(s, c.textures);
    pk.quad = sh.quad;
    r = {};
    r.vertex.program = sh.prog ? openshao::pipelines::ProgramHash(s.vs->program) : 0;
    r.vertex.decode = a.decode; r.vertex.regs = a.regs;
    r.vertex.pretransformed = a.pretransformed; r.vertex.quad = sh.quad; r.vertex.fixed = sh.fk;
    r.pixel = pk;
    r.vs_source = candidate_sources_.Vertex(r.vertex, [&] {
        auto vs = VertexHlsl(s, a, sh);
        if (sh.quad) vs = QuadHlsl(std::move(vs));
        return ShaderKey(vs, "main", vkshader::Stage::Vertex);
    });
    r.ps_source = candidate_sources_.Pixel(pk, [&] { return ShaderKey(PixelHlsl(pk), "main", vkshader::Stage::Fragment); });
    openshao::pipelines::PipelinePass pass;
    pass.color = !c.shadow || c.shadow_color; pass.color_format = kBgra;
    pass.depth = c.depth;
    pass.depth_scale = openshao::pipelines::PipelineDepthScale(c, guest_w_);
    PsoKey key;
    openshao::pipelines::FillPipelineKey(s, a, r.vs_source, r.ps_source, pass, key);
    bool atlas = false, target = false;
    for (const auto& tex : c.textures) if (tex.kind) {
        target |= tex.kind != kTex2D || tex.rect || tex.shadow;
        atlas |= tex.kind == kTex2D && !tex.shadow;
    }
    key.atlas_guard = sh.quad && a.topology == Topology::Triangles && atlas && !target;
    r.key_size = sizeof(key); std::memcpy(r.key, &key, sizeof(key));
    return true;
}

bool VulkanBackend::BuildPipeline(const openshao::pipelines::Recipe& r, std::span<const uint32_t> program) {
    if (r.key_size != sizeof(PsoKey)) return false;
    const auto& v = r.vertex;
    std::string vsrc = v.program ? VertexProgramHlsl(std::vector<uint32_t>(program.begin(), program.end()), v.regs, v.decode)
                                 : FixedVertexHlsl(v.regs, v.pretransformed, v.decode, v.fixed);
    if (v.quad) vsrc = QuadHlsl(std::move(vsrc));
    uint64_t vh = 0, ph = 0, gh = 0;
    VkShaderModule vs = PrecompileModule(vsrc, "main", vkshader::Stage::Vertex, vh);
    VkShaderModule ps = PrecompileModule(PixelHlsl(r.pixel), "main", vkshader::Stage::Fragment, ph);
    if (!vs || !ps) return false;
    if (vh != r.vs_source || ph != r.ps_source)
        LOG_FIRST(8, "pipeline precompile: recipe sources changed (vertex %d, pixel %d); building the current ones",
                  vh != r.vs_source, ph != r.ps_source);
    PsoKey k;
    std::memcpy(&k, r.key, sizeof(k));
    k.vs = vh;
    k.ps = ph;
    // Geometry shaders compile before the pipeline key is held: a thread never holds two keys.
    VkShaderModule points = k.topo == uint32_t(Topology::Points) && gs_ok_
        ? PrecompileModule(PointHlsl(), "main", vkshader::Stage::Geometry, gh) : VK_NULL_HANDLE;
    VkShaderModule atlas = k.atlas_guard && gs_ok_
        ? PrecompileModule(openshao::AtlasBoundsHlsl(), "main", vkshader::Stage::Geometry, gh) : VK_NULL_HANDLE;
    if (k.atlas_guard && gs_ok_ && !atlas) return false;
    const uint64_t h = Hash(&k, sizeof(k));
    openshao::pipelines::InFlightScope flight{pre_pso_flight_, h};
    {
        std::lock_guard lock(pre_mutex_);
        if (pre_done_.count(h)) return true;
    }
    VkPipeline pso = MakePipeline(k, vs, ps, points, atlas);
    if (!pso) return false;
    std::lock_guard lock(pre_mutex_);
    pre_done_.insert(h);
    pre_psos_[h] = pso;
    return true;
}

void VulkanBackend::RecordPipeline(const State& s, const Assembled& a, const DrawShaders& sh, const PixelKey& pk,
                                   const PsoKey& k) {
    openshao::pipelines::Recipe r{};
    r.vs_source = sh.vh;
    r.ps_source = sh.ph;
    r.vertex.program = sh.prog ? s.vs->hash : 0;
    r.vertex.decode = a.decode;
    r.vertex.regs = a.regs;
    r.vertex.pretransformed = a.pretransformed;
    r.vertex.quad = sh.quad;
    r.vertex.fixed = sh.fk;
    r.pixel = pk;
    r.key_size = sizeof(k);
    std::memcpy(r.key, &k, sizeof(k));
    openshao::pipelines::TraceObservedAssetRecipe(r, &s);
    std::span<const uint32_t> program;
    if (sh.prog) program = {s.vs->program.data(), s.vs->program.size()};
    openshao::pipelines::Record(r, program, openshao::pipelines::CurrentRoom());
}

}

std::unique_ptr<Backend> CreateVulkanBackend() { return std::make_unique<VulkanBackend>(); }

}
