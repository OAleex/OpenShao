#include "pipeline_assets.h"
#include "pipeline_source_cache.h"
#include <cstdio>
#include <stdexcept>
#include <atomic>
#include <chrono>
#include "kernel/memory.h"
#include "kernel/kernel.h"

namespace kernel { uint8_t* g_base = nullptr; }
namespace gpu {
uint64_t Hash(const void* bytes, size_t count, uint64_t hash) {
    for (size_t i = 0; i < count; ++i) hash = (hash ^ static_cast<const uint8_t*>(bytes)[i]) * 1099511628211ull;
    return hash;
}
}
namespace d3d {
bool SnapshotPipelineShaders(uint32_t vertex, uint32_t pixel, gpu::State& s) {
    if ((vertex==0x104 || vertex==0x404) && (pixel==0xA0000 || pixel==0xA0010)) {
        s.vs_handle=vertex;
        s.ps=std::make_shared<gpu::PixelShader>();
        return true;
    }
    if (vertex != 0xABCD1 && vertex != 0xABCD3) return false;
    s.vs_handle = vertex;
    s.vs = std::make_shared<gpu::VertexShader>();
    s.vs->program = {1,2,3,4};
    s.vs->inputs[0] = {0,0,0x32}; s.vs->inputs[1] = {0,12,0x40};
    s.vs->inputs[3] = {0,16,vertex==0xABCD1 ? 0x16u : 0x22u};
    s.vs->stream_stride[0] = vertex==0xABCD1 ? 20 : 24;
    return true;
}
}

using namespace openshao::pipelines;
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main() {
    try {
        SourceHashes sources;
        VertexRecipe vertex{};
        gpu::PixelKey pixel_key{};
        unsigned vertex_builds=0,pixel_builds=0;
        auto vertex_source=[&]{++vertex_builds;return uint64_t(100+vertex.regs);};
        auto pixel_source=[&]{++pixel_builds;return uint64_t(200);};
        Require(sources.Vertex(vertex,vertex_source)==100 && sources.Pixel(pixel_key,pixel_source)==200,
                "source hashes must retain the backend's exact generated-source identity");
        Require(sources.Vertex(vertex,vertex_source)==100 && sources.Pixel(pixel_key,pixel_source)==200 &&
                vertex_builds==1 && pixel_builds==1,
                "render-state variants sharing shaders must generate each source only once");
        vertex.regs=3;
        Require(sources.Vertex(vertex,vertex_source)==103 && vertex_builds==2,
                "a changed vertex declaration must generate its own source");
        auto changed=pixel_key;
        reinterpret_cast<unsigned char*>(&changed)[0]^=1;
        Require(sources.Pixel(changed,pixel_source)==200 && pixel_builds==2,
                "a changed pixel recipe must not reuse another shader's source hash");
        for(unsigned i=0;i<5000;++i) {
            vertex.decode=i;
            sources.Vertex(vertex,vertex_source);
        }
        const auto generated=vertex_builds;
        Require(sources.Vertex(vertex,vertex_source)==103 && vertex_builds==generated,
                "bounded source caching must keep accepting shaders from later rooms");
        SourceHashes other_backend;
        Require(other_backend.Vertex(vertex,[]{return uint64_t(900);})==900,
                "renderer instances must not share profile-specific source hashes");
        MaterialVariant opaque{};
        gpu::State s{};
        ApplyMaterialVariant(s, opaque);
        Require(s.rs[gpu::kRsZEnable] == 1 && s.simple[gpu::kMZWriteEnable/4] == 1,
                "ordinary assets require depth testing and writing");
        Require(s.simple[gpu::kMZFunc/4] == 0x203 && s.rs[gpu::kRsCullMode] == 0x901,
                "asset state must match original scene comparison and winding");
        Require(s.simple[gpu::kMColorWriteEnable/4] == 0x01010101,
                "asset pipelines must retain all four colour channels");
        MaterialVariant transparent{};
        transparent.transparent = true;
        transparent.fade = true;
        transparent.object_flags = 0x80000000u;
        ApplyMaterialVariant(s, transparent);
        Require(s.simple[gpu::kMBlendEnable/4] && !s.simple[gpu::kMZWriteEnable/4] && !s.rs[gpu::kRsCullMode],
                "fading two-sided assets need alpha blending without depth writes or culling");
        transparent.object_flags |= 0x800;
        ApplyMaterialVariant(s, transparent);
        Require(s.simple[gpu::kMBlendOp/4] == 0x800B && s.simple[gpu::kMDestBlend/4] == 1,
                "subtractive material mode must not use ordinary alpha blending");
        for (const auto flag : {0x400u, 0x800u, 0x400000u}) {
            MaterialVariant flagged{};
            flagged.object_flags = flag;
            ApplyMaterialVariant(s, flagged);
            Require(s.simple[gpu::kMBlendEnable/4] && s.simple[gpu::kMZWriteEnable/4],
                    "original blend flags require transparency with depth writes even for opaque textures");
        }
        SetAssetAlphaKill(s, true);
        Require(s.tss[0][gpu::kTssAlphaKill] == 4,
                "alpha-kill enable must use D3DTALPHAKILL_ENABLE, not a boolean render state");
        SetAssetAlphaKill(s, false);
        Require(s.tss[0][gpu::kTssAlphaKill] == 0, "disabled alpha kill must clear the stage");
        for (const auto code : {0x1107u, 0x1208u, 0x1409u}) {
            const auto pair = SkinShaders(code, 4, 0);
            Require(pair.vertex == 58 && pair.pixel == 132,
                    "single-bone assets must use the original skinned lighting shader family");
        }
        Require(SkinShaders(0x140C, 4, 0).vertex == 59 &&
                SkinShaders(0x140F, 4, 0).vertex == 60 &&
                SkinShaders(0x1412, 4, 0).vertex == 61,
                "skinning declarations must follow the original bone-count rows");
        Require(SkinShaders(0x1409, 4, 2).vertex == 66 &&
                SkinShaders(0x1409, 5, 0).pixel == 132,
                "lighting and combined-light passes must select their original shader rows");
        Require(SkinShaders(0x140C, 2, 0).vertex == 103 && SkinShaders(0x140C, 2, 0).pixel == 136,
                "skinned shadow maps require a different vertex program from the ordinary shadow pass");
        Require(!SkinShaders(0x1406, 4, 0).valid && !SkinShaders(0x1409, 3, 0).valid &&
                !SkinShaders(0x1409, 4, 4).valid, "unknown skin declarations or passes must be rejected");
        ApplySkinVariant(s, opaque, false, true);
        Require(!s.rs[gpu::kRsCullMode] && s.rs[gpu::kRsStencilEnable] &&
                s.simple[gpu::kMStencilFunc/4] == 0x207 &&
                s.simple[gpu::kMStencilMask/4] == 255 && s.rs[gpu::kRsStencilFail] == 0x1E01,
                "skinned scene prediction must include the inherited keep-stencil pass");
        ApplySkinVariant(s, opaque, true, false);
        Require(!s.simple[gpu::kMColorWriteEnable/4] && s.simple[gpu::kMSolidOffsetEnable/4] &&
                s.simple[gpu::kMSlopeScale/4] == 0x40E00000 && !s.rs[gpu::kRsCullMode],
                "skinned shadow prediction must preserve its depth-only slope-biased pass");
        ApplySkinShadowBlend(s, true);
        Require(s.simple[gpu::kMBlendEnable/4] && s.simple[gpu::kMSrcBlend/4] == 0x302 &&
                s.simple[gpu::kMDestBlend/4] == 0x303 && s.simple[gpu::kMBlendOp/4] == 0x8007,
                "the transparent skin-shadow partition inherits MIN while selecting source-alpha blending");
        Require(!s.simple[gpu::kMColorWriteEnable/4] && s.simple[gpu::kMSolidOffsetEnable/4] &&
                s.simple[gpu::kMSlopeScale/4] == 0x40E00000 && s.rs[gpu::kRsZEnable] &&
                s.simple[gpu::kMZWriteEnable/4] && !s.rs[gpu::kRsCullMode],
                "skin-shadow blending must preserve colour masking, depth writes and shadow slope bias");
        ApplySkinShadowBlend(s, true, false);
        Require(s.simple[gpu::kMBlendEnable/4] && s.simple[gpu::kMSrcBlend/4] == 0x300 &&
                s.simple[gpu::kMDestBlend/4] == 0x306 && s.simple[gpu::kMBlendOp/4] == 0x8007 &&
                !s.simple[gpu::kMColorWriteEnable/4] && s.simple[gpu::kMSlopeScale/4] == 0x40E00000,
                "the outer skin-shadow batch retains SRC_COLOR/DEST_ALPHA factors before the transparent partition");
        ApplySkinShadowBlend(s, false);
        Require(!s.simple[gpu::kMBlendEnable/4] && !s.simple[gpu::kMColorWriteEnable/4] &&
                s.simple[gpu::kMSlopeScale/4] == 0x40E00000,
                "the ordinary skin-shadow partition must disable blending without discarding shadow state");
        ApplyProjectedMaterialVariant(s, opaque);
        Require(s.simple[gpu::kMBlendEnable/4] && s.simple[gpu::kMSrcBlend/4] == 0 &&
                s.simple[gpu::kMDestBlend/4] == 0x301 && s.simple[gpu::kMBlendOp/4] == 0x8006,
                "projected light groups 163/164 must multiply destination colour by the source");
        Require(s.rs[gpu::kRsZEnable] && s.simple[gpu::kMZWriteEnable/4] &&
                s.simple[gpu::kMZFunc/4] == 0x203 && s.simple[gpu::kMColorWriteEnable/4] == 0x01010101 &&
                s.rs[gpu::kRsCullMode] == 0x901 && !s.simple[gpu::kMSolidOffsetEnable/4] &&
                s.simple[gpu::kMAlphaTestEnable/4] && !s.simple[gpu::kMAlphaRef/4] &&
                s.simple[gpu::kMAlphaFunc/4] == 0x204,
                "projected material passes retain ordinary depth/raster state and their original alpha comparison");
        MaterialVariant projected_instance{};
        projected_instance.fade = true;
        projected_instance.object_flags = 0x80000C00u;
        ApplyProjectedMaterialVariant(s, projected_instance);
        Require(!s.simple[gpu::kMZWriteEnable/4] && !s.rs[gpu::kRsCullMode] &&
                s.simple[gpu::kMSrcBlend/4] == 0 && s.simple[gpu::kMDestBlend/4] == 0x301 &&
                s.simple[gpu::kMBlendOp/4] == 0x8006,
                "projected multiplication overrides ordinary additive/subtractive flags while retaining instance depth/culling");
        LoadGate gate;
        Require(gate.Enter(11) == LoadAction::Parse, "a new asset must run its parser");
        gate.Parsed(11);
        uint32_t stack = 0xD0010000;
        Require(gate.ReturnPending(stack) && stack == 0xD0010004,
                "a polling hook must consume its guest return address exactly once");
        Require(gate.Enter(11) == LoadAction::Poll, "a completed parser must never run twice while compilation waits");
        Require(gate.Enter(12) == LoadAction::Parse, "a reused load record with a new generation must parse");
        unsigned descriptions = 0;
        float described_scale=0;
        SetCandidateDescriber([&](const PipelineCandidate& c, Recipe& r) {
            described_scale=c.render_scale;
            ++descriptions; r = {}; r.key_size = 1; r.key[0] = uint8_t(descriptions); return true;
        });
        gpu::Assembled layout;
        PipelineCandidate candidate{s, layout}; candidate.count = 90;
        Recipe first{}, again{};
        Require(DescribeCandidate(candidate, first), "metadata description succeeds");
        s.vsc[0][0] = 7; s.psc[0][0] = 3;
        Require(DescribeCandidate(candidate, again) && descriptions == 1 && first.key[0] == again.key[0],
                "equivalent assets must share metadata lowering even when shader constants differ");
        candidate.shadow = true;
        Require(DescribeCandidate(candidate, again) && descriptions == 2,
                "a different attachment pass must not reuse a scene recipe");
        layout.elements[0].offset = 12;
        Require(DescribeCandidate(candidate, again) && descriptions == 3,
                "a different vertex layout must not reuse an incompatible recipe");
        candidate.count = 5;
        Require(DescribeCandidate(candidate, again) && descriptions == 4,
                "five-vertex draws must use the renderer's small-quad classification");
        candidate.count = 6;
        Require(DescribeCandidate(candidate, again) && descriptions == 4,
                "two-triangle quads must share the small-quad recipe");
        candidate.count = 7;
        Require(DescribeCandidate(candidate, again) && descriptions == 4,
                "larger draws must reuse the existing ordinary recipe");
        candidate.depth_width=256;
        Require(DescribeCandidate(candidate,again) && descriptions==5,
                "a different shadow-map width must invalidate its depth-bias recipe");
        candidate.render_scale=2.3625f;
        Require(PipelineDepthScale(candidate,640)==605.0f/256,
                "shadow bias must use the rounded 256-wide host target, not the unrounded display scale");
        candidate.depth_width=0;
        Require(PipelineDepthScale(candidate,640)==1512.0f/640,
                "ordinary depth uses the main target width with the same allocation rounding");
        SetPipelineRenderScale(2.3625f);
        Require(DescribeCandidate(candidate,again) && descriptions==6 && described_scale==2.3625f,
                "a render-scale change must invalidate cached lowering and reach the active backend safely");
        SetPipelineRenderScale(2.3625f);
        Require(DescribeCandidate(candidate,again) && descriptions==6,
                "an unchanged render scale must retain shared metadata lowering");
        SetPipelineRenderScale(1);
        // Later rooms must retain their own hot descriptions after the cache fills.
        for (unsigned i=0;i<5000;++i) {
            candidate.depth_width=10000+i;
            Require(DescribeCandidate(candidate,again), "a later room can describe its pipelines");
        }
        const auto after_fill=descriptions;
        const auto last_recipe=again;
        Require(DescribeCandidate(candidate,again) && descriptions==after_fill &&
                std::memcmp(&again,&last_recipe,sizeof(again))==0,
                "a full description cache must still reuse newly loaded assets");
        ClearCandidateDescriber();
        Require(!DescribeCandidate(candidate, again), "renderer shutdown must invalidate cached descriptions");
        struct Builder final : openshao::pipelines::Builder {
            std::atomic<unsigned> built{0};
            ~Builder() { Shutdown(); }
            bool BuildPipeline(const Recipe& r, std::span<const uint32_t> program) override {
                if (r.vertex.program && ProgramHash(program) != r.vertex.program) return false;
                ++built;
                return true;
            }
        } builder;
        const auto root = std::filesystem::temp_directory_path() /
            ("openshao-screen-warm-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Start(root / "recipes.bin", 1, &builder);
        unsigned serial=0;
        bool immediate=false, programmed=false, depth=false, no_depth=false, stencil=false, pixel=false;
        SetCandidateDescriber([&](const PipelineCandidate& c, Recipe& r) {
            immediate |= c.state.vs && c.state.vs->program.empty();
            programmed |= c.state.vs && !c.state.vs->program.empty();
            depth |= c.depth; no_depth |= !c.depth;
            stencil |= c.state.rs[gpu::kRsStencilEnable] != 0;
            pixel |= bool(c.state.ps);
            r = {};
            r.key_size=4; const auto id=++serial; std::memcpy(r.key,&id,4);
            if (c.state.vs && !c.state.vs->program.empty()) r.vertex.program=ProgramHash(c.state.vs->program);
            return true;
        });
        WarmCommonScreenPipelines();
        Require(immediate && depth && no_depth && builder.built == serial,
                "device startup must finish immediate screen passes with both attachment configurations");
        const auto common=serial;
        WarmCommonScreenPipelines();
        Require(serial==common, "startup screen warming must be idempotent");
        WarmInterfaceShader(0xABCD1);
        Require(programmed && stencil && builder.built == serial && serial>common,
                "shader creation must finish original interface and stencil passes before the first draw");
        const auto packed_interface=serial;
        WarmInterfaceShader(0xABCD1);
        Require(serial==packed_interface, "repeated interface programs must share prepared passes");
        WarmInterfaceShader(0xABCD3);
        Require(serial>packed_interface && builder.built==serial,
                "a shared program with the original floating-UV declaration requires its own early particle pipelines");
        const auto interface=serial;
        WarmInterfaceShader(0xABCD3);
        Require(serial==interface, "floating particle pipelines must also be shared across repeated shader creation");
        Require(mem::Init() && mem::Commit(0x63A000,0x1000) && mem::Commit(0x200000,0x2000),
                "a synthetic shader manager must be mapped for the library ordering regression");
        kernel::g_base=mem::Base();
        kernel::W32(0x63AFB4,0x63AFB8);
        kernel::W32(0x63AFB8,0x200000);
        kernel::W32(0x63AFBC,0x201000);
        kernel::W32(0x200000,0xABCD1);
        WarmShaderLibraryPipelines();
        Require(serial==interface && !pixel, "an early partial shader library must not prepare effects without pixel programs");
        kernel::W32(0x63A218+139*4,0x10001);
        kernel::W32(0x63A218+138*4,0x10002);
        kernel::W32(0x201000+8,0xA0000);
        kernel::W32(0x201000+16,0xA0010);
        WarmShaderLibraryPipelines();
        Require(pixel && serial>interface && builder.built==serial,
                "the later complete shader library must still prepare effects before use");
        const auto library=serial;
        WarmShaderLibraryPipelines();
        Require(serial==library, "completed effect preparation must be shared by later asset loads");
        ClearCandidateDescriber();
        Shutdown();
        std::filesystem::remove_all(root);
        std::puts("pipeline assets: ok");
    } catch (const std::exception& e) { std::fprintf(stderr,"pipeline assets: %s\n",e.what()); return 1; }
}
