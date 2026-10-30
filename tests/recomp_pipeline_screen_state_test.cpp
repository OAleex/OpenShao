#include "pipeline_screen_state.h"
#include "pipeline_assets.h"
#include <cstdio>
#include <stdexcept>

namespace kernel { uint8_t* g_base = nullptr; }

namespace {
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Key {
    uint64_t vs, ps;
    uint32_t regs, elements[16], topo, has_rt, has_ds, rtfmt, dsfmt;
    uint32_t blend, src, dst, op, wmask, zen, zwrite, zfunc;
    uint32_t sten, sfunc, smask, swmask, sfail, szfail, spass;
    int32_t zbias;
    float zslope;
    uint32_t cull, ccw, fill;
};
}

int main() {
    try {
        using namespace gpu;
        using namespace openshao::pipelines;
        State s{};
        s.vs_handle = 0xB0000001;
        s.vs = std::make_shared<VertexShader>();
        s.vs->program = {0,0,0,1};
        s.vs->inputs[0] = {0,0,0x32};
        s.vs->inputs[1] = {0,12,0x40};
        s.vs->inputs[3] = {0,16,0x16};
        s.vs->stream_stride[0] = 20;
        Require(IsInterfaceShader(*s.vs), "the retail screen declaration must trigger early UI warming");
        auto other = *s.vs;
        other.inputs[3].type = 0x22;
        Require(!IsInterfaceShader(other), "the mesh declaration sharing the screen program must not trigger UI warming");
        other.stream_stride[0] = 24;
        Require(IsInterfaceShader(other), "the original FLOAT2 particle declaration must prepare its own layout");
        State floating{};
        floating.vs = std::make_shared<VertexShader>(other);
        floating.vs_handle = 0xB0000001;
        floating.streams[0] = {1,24};
        Assembled floating_layout;
        Require(DescribePipelineLayout(floating,8,floating_layout) && floating_layout.regs == 11 &&
                floating_layout.decode == 0 && floating_layout.elements[3].format == 16 &&
                floating_layout.elements[3].offset == 16 && floating_layout.buffers[0].stride == 24,
                "FLOAT2 particle coordinates require an undecoded 24-byte layout instead of packed 20-byte inputs");
        other = *s.vs; other.inputs[9] = {0,20,0x22};
        Require(!IsInterfaceShader(other), "an additional input is not the retail screen declaration");
        other = *s.vs; other.program.clear();
        Require(!IsInterfaceShader(other), "an unprogrammed declaration must not be mistaken for the UI shader");

        // 0011C730 selects the original program and sets stage 0 to colour/alpha op 5.
        ApplyInterfacePipelineState(s, ScreenBlend::Alpha, ScreenStencil::Disabled);
        TexInfo textures[4]{{kTex2D,0,0,0}};
        auto pixel = MakePixelKey(s, textures);
        constexpr uint32_t screen_stage[] = {5,0,2,1,5,2,0,1,1};
        constexpr uint32_t disabled_stage[] = {1,2,1,1,1,2,1,1,1};
        Require(!pixel.programmable && !pixel.alphakill && pixel.proj == 3,
                "programmed screen vertices require fixed pixel stages projected through the disabled stage");
        for (unsigned i=0; i<9; ++i) {
            Require(pixel.ff[0][i] == screen_stage[i], "UI colour/alpha arguments must match the original screen setup");
            Require(pixel.ff[1][i] == disabled_stage[i], "the first disabled stage still participates in pixel shader identity");
        }
        s.streams[0] = {1,20};
        Assembled layout;
        Require(DescribePipelineLayout(s,6,layout), "UI metadata can be described without geometry");
        PipelinePass pass{true,true,true,87,45,1};
        Key key{};
        FillPipelineKey(s,layout,1,2,pass,key);
        Require(key.regs==11 && key.elements[0]==6 && key.elements[1]==0x000C0057 && key.elements[3]==0x0010002A,
                "the screen program uses FLOAT3, colour and packed UV input formats");
        Require(!key.zen && !key.sten && key.wmask==15 && key.blend && key.src==0x302 && key.dst==0x303,
                "ordinary UI blending must work without depth testing");

        ApplyInterfacePipelineState(s, ScreenBlend::Alpha, ScreenStencil::WriteMask);
        FillPipelineKey(s,layout,1,2,pass,key);
        Require(!key.zen && key.sten && key.wmask==0 && key.sfunc==0x207 && key.smask==255 && key.swmask==255 &&
                key.sfail==0x1E01 && key.szfail==0x1E01 && key.spass==0x1E01,
                "0011C600 must write the UI stencil without colour or depth");
        ApplyInterfacePipelineState(s, ScreenBlend::Alpha, ScreenStencil::TestMask);
        FillPipelineKey(s,layout,1,2,pass,key);
        Require(!key.zen && key.sten && key.wmask==7 && key.sfunc==0x205 && key.sfail==0x1E01 &&
                key.szfail==0x1E01 && key.spass==0x1E00,
                "0011C560 must test the mask and retain its replacement/keep operations");
        ApplyInterfacePipelineState(s, ScreenBlend::Additive, ScreenStencil::Disabled, false);
        FillPipelineKey(s,layout,1,2,pass,key);
        Require(!key.sten && key.wmask==7 && key.src==0x302 && key.dst==1 && key.op==0x8006,
                "sprite blend mode 1 must add source alpha without writing alpha");
        ApplyInterfacePipelineState(s, ScreenBlend::ReverseSubtract, ScreenStencil::Disabled);
        FillPipelineKey(s,layout,1,2,pass,key);
        Require(key.dst==1 && key.op==0x800B, "sprite blend mode 2 must preserve reverse subtraction");

        for (bool textured : {false,true}) {
            MakeImmediateScreenInputs(s,textured);
            ApplyImmediateScreenPipelineState(s,textured);
            Require(DescribePipelineLayout(s,8,layout), "immediate screen state must have an owned synthetic declaration");
            Require(s.vs_handle==(textured?0x145u:0x45u) && s.vs->program.empty() && layout.pretransformed &&
                    layout.regs==(textured?0x209:9) && layout.buffers[0].stride==(textured?48u:32u),
                    "EndPrimitive packs float4 attributes and retains the FVF position-space tag");
            Require(layout.elements[0].format==2 && layout.elements[3].format==2 && layout.elements[3].offset==16 &&
                    (!textured || (layout.elements[9].format==2 && layout.elements[9].offset==32)) && !layout.strip,
                    "immediate quads require the expanded triangle-list layout instead of packed FVF bytes");
            pixel=MakePixelKey(s,textures);
            Require(!pixel.proj && pixel.ff[0][0]==(textured?4u:2u) && pixel.ff[0][1]==0 && pixel.ff[0][2]==2 &&
                    pixel.ff[0][4]==(textured?4u:2u) && pixel.ff[0][5]==(textured?2u:0u),
                    "immediate solid/textured paths must select their original fixed stages");
            FillPipelineKey(s,layout,1,2,pass,key);
            Require(key.zen && !key.zwrite && key.zfunc==0x203 && !key.sten,
                    "ordinary immediate overlays inherit the restored depth test without writing depth");
        }
        for (bool four_taps : {false,true}) {
            // 00137F30/00138610 select PS139/138 separately from the FVF.
            auto ps=std::make_shared<PixelShader>();
            ps->def[53]=four_taps ? 0x00011103 : 0x00011102;
            ps->def[54]=four_taps ? 0x00008421 : 1;
            s.ps=ps;
            s.rs[kRsPsTextureModes]=ps->def[54];
            MakeFullscreenInputs(s,four_taps);
            ApplyFullscreenPipelineState(s,four_taps);
            Require(s.ps==ps && s.vs_handle==(four_taps?0x404u:0x104u) && !s.vs,
                    "fullscreen effects preserve their loaded pixel program and select the original FVF");
            Require(DescribePipelineLayout(s,6,layout) && layout.pretransformed && layout.strip &&
                    layout.regs==(four_taps?0x1E01:0x201) && layout.buffers[0].stride==(four_taps?48u:24u),
                    "fullscreen metadata retains raw triangle strips and one or four float2 texture coordinates");
            Require(layout.elements[0].format==2 && layout.elements[9].format==16 && layout.elements[9].offset==16 &&
                    (!four_taps || (layout.elements[12].format==16 && layout.elements[12].offset==40)),
                    "fullscreen texture registers must use FLOAT2 instead of the immediate FLOAT4 declaration");
            const auto effect_textures=FullscreenTextures(four_taps);
            pixel=MakePixelKey(s,effect_textures.data());
            Require(pixel.programmable && pixel.count==(four_taps?0x00011103u:0x00011102u) &&
                    pixel.tex_modes==(four_taps?0x8421u:1u) && pixel.tex[0].kind==kTex2D && pixel.tex[0].rect==1,
                    "fullscreen pixel selection retains the loaded combiner and linear texture classification");
            Require((four_taps && pixel.tex[3].kind==kTex2D && pixel.tex[3].rect==1) ||
                    (!four_taps && pixel.tex[1].kind==kTexNone), "only the four-tap path binds all four source stages");
            FillPipelineKey(s,layout,1,2,pass,key);
            Require(!key.zen && !key.sten && !key.cull && !key.blend && key.wmask==15,
                    "the first fullscreen tap batch replaces its target without blending or depth testing");
            if (four_taps) {
                ApplyFullscreenPipelineState(s,true,true);
                FillPipelineKey(s,layout,1,2,pass,key);
                Require(key.blend && key.src==1 && key.dst==1 && key.op==0x8006,
                        "subsequent four-tap batches add ONE plus ONE after the first batch");
            }
        }
        s.vs=std::make_shared<VertexShader>();
        s.vs->program={0,0,0,1};
        s.vs_handle=0xB0000001;
        for (bool stage1 : {false,true}) {
            ApplyParticlePipelineState(s,ScreenBlend::Alpha,stage1);
            pixel=MakePixelKey(s,textures);
            constexpr uint32_t particle_stage[]={5,2,0,1,5,2,0,1,1};
            for (unsigned i=0; i<9; ++i)
                Require(pixel.ff[0][i]==particle_stage[i], "particles reverse UI colour arguments while retaining modulate2x alpha");
            Require(pixel.ff[1][0]==(stage1?4u:1u) && pixel.ff[1][4]==1 && pixel.proj==(stage1?7u:3u),
                    "particle pixel identity includes the inherited disabled/modulate stage1 alternative");
            FillPipelineKey(s,layout,1,2,pass,key);
            Require(key.zen && !key.zwrite && !key.sten && !key.cull && key.blend && key.dst==0x303,
                    "particles test depth without writing it or culling their quads");
        }
        ApplyParticlePipelineState(s,ScreenBlend::ReverseSubtract,true,true);
        FillPipelineKey(s,layout,1,2,pass,key);
        Require(key.sten && key.sfunc==0x205 && key.smask==255 && key.swmask==255 && key.spass==0x1E00 &&
                key.src==0x302 && key.dst==1 && key.op==0x800B && key.wmask==15,
                "particle instance stencil and subtractive blend flags must survive metadata lowering");
        ApplyFloatingParticlePipelineState(floating);
        pixel=MakePixelKey(floating,textures);
        constexpr uint32_t floating_stage[]={4,0,2,1,4,0,2,1,1};
        for (unsigned i=0; i<9; ++i) {
            Require(pixel.ff[0][i]==floating_stage[i], "floating particles must modulate vertex colour/alpha by their texture");
            Require(pixel.ff[1][i]==disabled_stage[i], "floating particles disable the second fixed stage");
        }
        Require(!pixel.programmable && !pixel.alphakill && pixel.proj==3,
                "floating particles use fixed pixels with programmed vertex projection and no alpha-kill");
        FillPipelineKey(floating,floating_layout,1,2,pass,key);
        Require(key.blend && key.src==0x302 && key.dst==1 && key.op==0x8006 && key.zen &&
                !key.zwrite && key.zfunc==0x203 && !key.cull && key.wmask==15 && !key.sten,
                "floating particles require source-alpha additive blending and depth tests without depth writes");
        Require(!floating.simple[kMAlphaTestEnable/4], "floating particle setup disables alpha testing");
        std::puts("screen pipeline state: original UI, stencil and immediate paths lower correctly");
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr,"screen pipeline state: %s\n",e.what()); return 1; }
}
