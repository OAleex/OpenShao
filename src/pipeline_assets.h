#pragma once
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include "pipeline_precompile.h"
#include "gpu/vertex.h"

namespace gpu {
bool DescribePipelineLayout(const State&, uint32_t primitive, Assembled&);
}
namespace d3d {
// Copies shader definitions under the D3D lock without changing device state.
bool SnapshotPipelineShaders(uint32_t vertex, uint32_t pixel, gpu::State&);
}

namespace openshao::pipelines {
enum class LoadAction { Parse, Poll };
class LoadGate {
public:
    LoadAction Enter(uint64_t generation) {
        if (generation_ != generation) { generation_ = generation; parsed_ = false; }
        return parsed_ ? LoadAction::Poll : LoadAction::Parse;
    }
    void Parsed(uint64_t generation) { generation_ = generation; parsed_ = true; }
    bool ReturnPending(uint32_t& stack_pointer) {
        if (!parsed_) return false;
        stack_pointer += 4;
        return true;
    }
    void Reset() { generation_ = 0; parsed_ = false; }
private:
    uint64_t generation_ = 0;
    bool parsed_ = false;
};

struct MaterialVariant {
    uint32_t object_flags = 0, object_flags2 = 0;
    bool transparent = false, fade = false, shadow = false;
};
inline void SetAssetAlphaKill(gpu::State& state, bool enabled) {
    state.tss[0][gpu::kTssAlphaKill] = enabled ? 4 : 0;  // D3DTALPHAKILL_ENABLE
}

// Original scene baseline and per-unit overlays (001221C0, 0013CF20, 0012F6E0).
inline void ApplyMaterialVariant(gpu::State& s, const MaterialVariant& v) {
    using namespace gpu;
    auto put = [&](uint32_t method, uint32_t value) { s.simple[method / 4] = value; };
    s.rs[kRsZEnable] = 1;
    s.rs[kRsStencilEnable] = 0;
    s.rs[kRsCullMode] = v.object_flags & 0x80000000u ? 0 : 0x901;
    s.rs[kRsFillMode] = 0x1B02;
    s.rs[kRsTwoSidedLighting] = 0;
    put(kMZFunc, 0x203);
    put(kMZWriteEnable, !v.fade && !(v.object_flags2 & 1));
    put(kMBlendEnable, v.transparent || (!v.shadow && (v.object_flags & 0x400C00)));
    put(kMSrcBlend, 0x302);
    put(kMDestBlend, v.object_flags & 0xC00 ? 1 : 0x303);
    put(kMBlendOp, v.object_flags & 0x800 && !(v.object_flags & 0x400) ? 0x800B : 0x8006);
    put(kMColorWriteEnable, v.shadow ? 0 : 0x01010101);
    put(kMSolidOffsetEnable, 0);
    put(kMShadeMode, 0x1D01);
    for (auto& t : s.tss) {
        t[kTssAlphaKill] = 0;
        t[kTssColorOp] = t[kTssAlphaOp] = 1;
    }
    auto& t = s.tss[0];
    t[kTssColorOp] = t[kTssAlphaOp] = 4;
    t[kTssColorArg1] = t[kTssAlphaArg1] = 1;
    t[kTssColorArg2] = t[kTssAlphaArg2] = 2;
}
struct ShaderPair { uint32_t vertex = 0, pixel = 0; bool valid = false; };
// SetObjectRenderType (00130C60) and the immutable rows used by 0012BAA0.
inline ShaderPair SkinShaders(uint32_t render_type, uint32_t pass, uint32_t lights) {
    const uint32_t low = render_type & 255;
    if (low < 7 || low > 18 || lights > 3) return {};
    const uint32_t bones = (low - 7) / 3;
    const uint32_t feature = low - (7 + bones * 3);
    if ((render_type >> 8) != (feature == 0 ? 0x11u : feature == 1 ? 0x12u : 0x14u)) return {};
    uint32_t base = 0, pixel = 0;
    switch (pass) {
    case 0: case 1: base = 26 + lights * 4; pixel = 130; break;
    case 8: base = 42 + lights * 4; pixel = 131; break;
    case 4: case 5: base = 58 + lights * 4; pixel = 132; break;
    case 12: case 13: base = 74 + lights * 4; pixel = 133; break;
    case 16: base = lights <= 1 ? 90 : lights == 2 ? 94 : 98; pixel = 130; break;
    case 2: base = 102; pixel = 136; break;
    case 32: base = 106; pixel = 130; break;
    default: return {};
    }
    // The original pass-8, zero-light table has three shared entries.
    if (pass == 8 && lights == 0) {
        constexpr uint32_t row[] = {42,47,52,57};
        return {row[bones], pixel, true};
    }
    return {base + bones, pixel, true};
}
inline void ApplySkinVariant(gpu::State& s, const MaterialVariant& v, bool shadow, bool stencil) {
    auto material = v; material.shadow = shadow;
    ApplyMaterialVariant(s, material);
    using namespace gpu;
    s.rs[kRsCullMode] = 0;
    s.rs[kRsStencilEnable] = stencil;
    if (stencil) {
        s.simple[kMStencilFunc/4] = 0x207;
        s.simple[kMStencilMask/4] = s.simple[kMStencilWriteMask/4] = 255;
        s.rs[kRsStencilFail] = s.simple[kMStencilZFail/4] = s.simple[kMStencilPass/4] = 0x1E01;
    }
    if (shadow) {
        s.simple[kMSolidOffsetEnable/4] = 1;
        s.simple[kMSlopeScale/4] = 0x40E00000;  // original shadow slope, 7.0
    }
}

// 0013CB10 selects groups 163/164 for additional projected lights and overlays
// their destination-colour multiplication on the unit's normal depth/raster state.
inline void ApplyProjectedMaterialVariant(gpu::State& s, const MaterialVariant& v) {
    ApplyMaterialVariant(s, v);
    using namespace gpu;
    s.simple[kMBlendEnable/4] = 1;
    s.simple[kMSrcBlend/4] = 0;
    s.simple[kMDestBlend/4] = 0x301;
    s.simple[kMBlendOp/4] = 0x8006;
    s.simple[kMAlphaTestEnable/4] = 1;
    s.simple[kMAlphaRef/4] = 0;
    s.simple[kMAlphaFunc/4] = 0x204;
}
// 0012C540 sets MIN/SRC_COLOR/DEST_ALPHA before 0012BF60/0012BAA0(pass 2).
// The transparent partition at 0012BE00 changes the factors without resetting MIN.
inline void ApplySkinShadowBlend(gpu::State& s, bool minimum, bool alpha = true) {
    using namespace gpu;
    s.simple[kMBlendEnable/4] = minimum;
    s.simple[kMSrcBlend/4] = alpha ? 0x302 : 0x300;
    s.simple[kMDestBlend/4] = alpha ? 0x303 : 0x306;
    s.simple[kMBlendOp/4] = minimum ? 0x8007 : 0x8006;
}

struct PipelinePass {
    bool color = true, depth = true, stencil = true;
    uint32_t color_format = 0, depth_format = 0;
    float depth_scale = 1;
};
struct PipelineCandidate {
    const gpu::State& state;
    const gpu::Assembled& layout;
    gpu::TexInfo textures[4]{};
    uint32_t count = 0;
    bool shadow = false;
    bool depth = true;
    bool shadow_color = false;
    uint32_t depth_width = 0;
    float render_scale = 1;
};
// Shadow targets allocated by 00137880 are 256 wide. Other depth passes use
// their main target width. Match each renderer's target-allocation rounding.
inline float PipelineDepthScale(const PipelineCandidate& c, uint32_t main_width) {
    const uint32_t width=c.depth_width ? c.depth_width : main_width;
    return width ? float(std::max(1l,std::lround(width*c.render_scale)))/width : 1;
}
using CandidateDescriber = std::function<bool(const PipelineCandidate&, Recipe&)>;
void SetCandidateDescriber(CandidateDescriber);
void ClearCandidateDescriber();
void SetPipelineRenderScale(float scale);
bool DescribeCandidate(const PipelineCandidate&, Recipe&);
void TraceObservedAssetRecipe(const Recipe&, const gpu::State* state = nullptr);
void ObserveAssetPipeline(uint64_t key, bool compiled_on_draw);
void ReportAssetPipelines();
// Prepare title-wide passes at device/shader creation, before their first draw.
void WarmCommonScreenPipelines();
void WarmInterfaceShader(uint32_t vertex);
void WarmShaderLibraryPipelines();
// Read/copy only on the asset-loading thread. No guest pointer is sent to workers.
void CapturePipelineObject(uint32_t object, uint32_t source_type = 0);
void BeginPipelineLoad();
void SealPipelineLoad();
bool PipelineLoadPending();
void EndPipelineLoad();

// Both ordinary draws and asset prediction use this exact key lowering.
template<class Key>
void FillPipelineKey(const gpu::State& s, const gpu::Assembled& a, uint64_t vs, uint64_t ps,
                     const PipelinePass& pass, Key& k) {
    using namespace gpu;
    std::memset(&k, 0, sizeof(k));
    k.vs = vs; k.ps = ps; k.regs = a.regs;
    for (unsigned r = 0; r < 16; ++r) if ((a.regs >> r) & 1) {
        const auto& e = a.elements[r];
        k.elements[r] = e.format | uint32_t(e.buffer) << 8 | uint32_t(e.offset) << 16;
    }
    if constexpr (requires { k.buffers; k.strides; k.strip; }) {
        k.buffers = a.buffer_count;
        for (unsigned i = 0; i < a.buffer_count && i < 16; ++i) k.strides[i] = a.buffers[i].stride;
        k.strip = a.strip;
    }
    k.topo = uint32_t(a.topology);
    k.has_rt = pass.color; k.has_ds = pass.depth; k.rtfmt = pass.color ? pass.color_format : 0;
    if constexpr (requires { k.dsfmt; }) k.dsfmt = pass.depth ? pass.depth_format : 0;
    auto simple = [&](uint32_t method) { return s.simple[method / 4]; };
    k.blend = simple(kMBlendEnable) != 0;
    if (k.blend) { k.src = simple(kMSrcBlend); k.dst = simple(kMDestBlend); k.op = simple(kMBlendOp); }
    const uint32_t cw = simple(kMColorWriteEnable);
    k.wmask = ((cw >> 16) & 1) | ((cw >> 8) & 1) << 1 | (cw & 1) << 2 | ((cw >> 24) & 1) << 3;
    if (pass.depth) {
        k.zen = s.rs[kRsZEnable] != 0;
        if (k.zen) { k.zwrite = simple(kMZWriteEnable) != 0; k.zfunc = simple(kMZFunc); }
        k.sten = pass.stencil && s.rs[kRsStencilEnable] != 0;
        if (k.sten) {
            k.sfunc = simple(kMStencilFunc); k.smask = simple(kMStencilMask) & 255;
            k.swmask = simple(kMStencilWriteMask) & 255; k.sfail = s.rs[kRsStencilFail];
            k.szfail = simple(kMStencilZFail); k.spass = simple(kMStencilPass);
        }
        if (a.topology == Topology::Triangles && simple(kMSolidOffsetEnable) && s.zscale > 0) {
            float bias, slope;
            uint32_t b = simple(kMDepthOffset), z = simple(kMSlopeScale);
            std::memcpy(&bias, &b, 4); std::memcpy(&slope, &z, 4);
            k.zbias = int32_t(std::lround(bias * 16777215.0f / s.zscale));
            k.zslope = slope * pass.depth_scale;
        }
    }
    const uint32_t cull = s.rs[kRsCullMode];
    k.cull = cull != 0 && a.topology == Topology::Triangles;
    k.ccw = cull == 0x900;
    k.fill = s.rs[kRsFillMode];
}
}
