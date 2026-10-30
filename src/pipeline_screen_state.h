#pragma once
#include "gpu/shaders.h"

namespace openshao::pipelines {
enum class ScreenBlend { Alpha, Additive, ReverseSubtract };
enum class ScreenStencil { Disabled, WriteMask, TestMask };

// Slots 110/111 at 0050FC6C/0050FC80 share a program, with packed/FLOAT2 UVs.
// Other mesh declarations share that program too and must not trigger screen warming.
inline bool IsInterfaceShader(const gpu::VertexShader& vs) {
    if (vs.program.empty() || (vs.stream_stride[0] != 20 && vs.stream_stride[0] != 24)) return false;
    const uint32_t uv_type=vs.stream_stride[0]==20 ? 0x16 : 0x22;
    for (unsigned reg=0; reg<16; ++reg) {
        const auto& input=vs.inputs[reg];
        if (reg==0 || reg==1 || reg==3) {
            const uint32_t offset=reg==0 ? 0 : reg==1 ? 12 : 16;
            const uint32_t type=reg==0 ? 0x32 : reg==1 ? 0x40 : uv_type;
            if (input.stream!=0 || input.offset!=offset || input.type!=type) return false;
        } else if (input.stream>=0 && input.type && input.type!=2) return false;
        if (reg && vs.stream_stride[reg]) return false;
    }
    return true;
}

namespace screen_detail {
inline void FixedStages(gpu::State& s) {
    using namespace gpu;
    // XDK defaults also matter for the first disabled stage in MakePixelKey.
    for (unsigned stage=0; stage<4; ++stage) {
        auto& t=s.tss[stage];
        t={};
        t[kTssColorOp]=t[kTssAlphaOp]=1;
        t[kTssColorArg0]=t[kTssAlphaArg0]=t[kTssResultArg]=1;
        t[kTssColorArg1]=t[kTssAlphaArg1]=2;
        t[kTssColorArg2]=t[kTssAlphaArg2]=1;
        t[kTssTexCoordIndex]=stage;
    }
    s.ps.reset();
    s.rs[kRsLighting]=s.rs[kRsTwoSidedLighting]=s.rs[kRsCullMode]=0;
    s.rs[kRsFillMode]=0x1B02;
    s.simple[kMShadeMode/4]=0x1D01;
    s.simple[kMSolidOffsetEnable/4]=0;
}
}

// Screen::Render and SetRenderMode (0011C730/0011C250), then optional mask helpers.
// Attachment presence is selected by the caller: these passes never test depth.
inline void ApplyInterfacePipelineState(gpu::State& s, ScreenBlend blend, ScreenStencil stencil,
                                        bool alpha_write=true) {
    using namespace gpu;
    screen_detail::FixedStages(s);
    auto& t=s.tss[0];
    t[kTssColorOp]=t[kTssAlphaOp]=5;
    t[kTssColorArg1]=t[kTssAlphaArg2]=0;
    t[kTssColorArg2]=t[kTssAlphaArg1]=2;
    s.rs[kRsZEnable]=0;
    s.simple[kMZWriteEnable/4]=0;
    s.simple[kMZFunc/4]=0x203;
    s.simple[kMBlendEnable/4]=1;
    s.simple[kMSrcBlend/4]=0x302;
    s.simple[kMDestBlend/4]=blend==ScreenBlend::Alpha ? 0x303 : 1;
    s.simple[kMBlendOp/4]=blend==ScreenBlend::ReverseSubtract ? 0x800B : 0x8006;
    s.simple[kMColorWriteEnable/4]=alpha_write ? 0x01010101 : 0x00010101;
    s.rs[kRsStencilEnable]=stencil!=ScreenStencil::Disabled;
    if (stencil!=ScreenStencil::Disabled) {
        // 0011C600 writes 1 on all stencil outcomes; 0011C560 tests NOTEQUAL.
        s.simple[kMColorWriteEnable/4]=stencil==ScreenStencil::WriteMask ? 0 : 0x00010101;
        s.simple[kMStencilFunc/4]=stencil==ScreenStencil::WriteMask ? 0x207 : 0x205;
        s.simple[kMStencilMask/4]=s.simple[kMStencilWriteMask/4]=0xFFFFFFFF;
        s.simple[kMStencilRef/4]=1;
        s.rs[kRsStencilFail]=s.simple[kMStencilZFail/4]=0x1E01;
        s.simple[kMStencilPass/4]=stencil==ScreenStencil::WriteMask ? 0x1E01 : 0x1E00;
    }
}

// 0012FCC0/0012FBE0 (solid/textured immediate rectangles). A normal overlay
// inherits the depth test restored after Screen::Render, with depth writes off.
// The caller may select the original depth-disabled rectangle variant as well.
inline void ApplyImmediateScreenPipelineState(gpu::State& s, bool textured) {
    ApplyInterfacePipelineState(s,ScreenBlend::Alpha,ScreenStencil::Disabled);
    using namespace gpu;
    auto& t=s.tss[0];
    t[kTssColorOp]=t[kTssAlphaOp]=textured ? 4 : 2;
    t[kTssAlphaArg1]=textured ? 2 : 0;
    s.rs[kRsZEnable]=1;
}

// EndPrimitive in the pinned SDK expands each submitted register to FLOAT4.
// Preserve XYZRHW in the synthetic handle, as the draw does. vb=1 is metadata.
inline void MakeImmediateScreenInputs(gpu::State& s, bool textured) {
    auto vs=std::make_shared<gpu::VertexShader>();
    vs->inputs[0]={0,0,0x42};
    vs->inputs[3]={0,16,0x42};
    if (textured) vs->inputs[9]={0,32,0x42};
    vs->stream_stride[0]=textured ? 48 : 32;
    s.vs_handle=textured ? 0x145 : 0x45;
    s.vs=std::move(vs);
    s.streams={};
    s.streams[0]={1,s.vs->stream_stride[0]};
}
// 00137F30 copies/downsamples with PS139; 00138610 filters with PS138.
// Pixel microcode belongs to the loaded shader library and is never synthesized here.
inline void ApplyFullscreenPipelineState(gpu::State& s, bool four_taps, bool additive=false) {
    const auto pixel=s.ps;
    screen_detail::FixedStages(s);
    s.ps=pixel;
    using namespace gpu;
    s.rs[kRsZEnable]=s.rs[kRsStencilEnable]=s.rs[kRsFogEnable]=0;
    s.simple[kMZWriteEnable/4]=s.simple[kMAlphaTestEnable/4]=0;
    s.simple[kMColorWriteEnable/4]=0x01010101;
    s.simple[kMBlendEnable/4]=four_taps && additive;
    s.simple[kMSrcBlend/4]=s.simple[kMDestBlend/4]=1;
    s.simple[kMBlendOp/4]=0x8006;
    for (unsigned stage=0; stage<(four_taps?4u:1u); ++stage) {
        auto& t=s.tss[stage];
        t[kTssAddressU]=t[kTssAddressV]=3;
        t[kTssMagFilter]=t[kTssMinFilter]=four_taps?2:5;
        t[kTssTexCoordIndex]=stage;
    }
}

// DrawVerticesUP uses four raw vertices: XYZRHW plus one/four FLOAT2 coordinates.
inline void MakeFullscreenInputs(gpu::State& s, bool four_taps) {
    s.vs.reset();
    s.vs_handle=four_taps?0x404:0x104;
    s.streams={};
    s.streams[0]={1,four_taps?48u:24u};
}
inline std::array<gpu::TexInfo,4> FullscreenTextures(bool four_taps) {
    std::array<gpu::TexInfo,4> textures{};
    for (unsigned stage=0; stage<(four_taps?4u:1u); ++stage)
        textures[stage]={gpu::kTex2D,1,0,0};
    return textures;
}

// 00127ED0/001277A0 establish the particle state. 0013E1C0 can leave stage 1
// modulating; 00128160 selects the instance's stencil test and blend flags.
inline void ApplyParticlePipelineState(gpu::State& s, ScreenBlend blend, bool modulate_stage1,
                                       bool stencil=false) {
    ApplyInterfacePipelineState(s,blend,stencil?ScreenStencil::TestMask:ScreenStencil::Disabled);
    using namespace gpu;
    s.tss[0][kTssColorArg1]=2;
    s.tss[0][kTssColorArg2]=0;
    s.tss[1][kTssColorOp]=modulate_stage1?4:1;
    s.rs[kRsZEnable]=1;
    s.simple[kMColorWriteEnable/4]=0x01010101;
    s.simple[kMAlphaTestEnable/4]=1;
    s.simple[kMAlphaFunc/4]=0x204;
    s.simple[kMAlphaRef/4]=5;
}
// 0013E590 selects slot 111 and fixed pixels. Its 24-byte FLOAT2 declaration
// shares the interface program but modulates both colour and alpha without 2x.
inline void ApplyFloatingParticlePipelineState(gpu::State& s) {
    ApplyInterfacePipelineState(s,ScreenBlend::Additive,ScreenStencil::Disabled);
    using namespace gpu;
    auto& t=s.tss[0];
    t[kTssColorOp]=t[kTssAlphaOp]=4;
    t[kTssColorArg1]=t[kTssAlphaArg1]=0;
    t[kTssColorArg2]=t[kTssAlphaArg2]=2;
    s.rs[kRsZEnable]=1;
    s.simple[kMAlphaTestEnable/4]=0;
}
}
