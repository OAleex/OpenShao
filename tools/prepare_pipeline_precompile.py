"""Connect pipeline precompilation to the pinned D3D12 renderer and the title's shader creation."""


def adapt_backend(backend, replace_once):
    backend = '#include "pipeline_assets.h"\n#include "pipeline_source_cache.h"\n#include <span>\n' + backend
    backend = replace_once(backend, 'class D3D12Backend final : public Backend, public Context {\npublic:\n',
        'class D3D12Backend final : public Backend, public Context, public openshao::pipelines::Builder {\n'
        'public:\n'
        '    ~D3D12Backend() override;\n'
        '    bool DescribeCandidate(const openshao::pipelines::PipelineCandidate& c, openshao::pipelines::Recipe& r);\n'
        '    bool BuildPipeline(const openshao::pipelines::Recipe& r, std::span<const uint32_t> program) override;\n')
    backend = replace_once(backend,
        '    std::unordered_map<uint64_t, ComPtr<ID3D12PipelineState>> psos_;\n',
        '    std::unordered_map<uint64_t, ComPtr<ID3D12PipelineState>> psos_;\n'
        '    // Shaders and pipelines the precompile workers built; the render thread adopts them on use.\n'
        '    std::mutex pre_mutex_;\n'
        '    std::unordered_map<uint64_t, ComPtr<ID3DBlob>> pre_blobs_;\n'
        '    std::unordered_map<uint64_t, ComPtr<ID3D12PipelineState>> pre_psos_;\n'
        '    std::unordered_set<uint64_t> pre_done_;  // pipelines made on either side\n'
        '    openshao::pipelines::SourceHashes candidate_sources_;\n'
        '    openshao::pipelines::InFlight pre_shader_flight_, pre_pso_flight_;\n'
        '    bool pso_first_use_ = false;\n'
        '    bool pso_compiled_on_draw_ = false;\n'
        '    ComPtr<ID3DBlob> AdoptShader(uint64_t hash);\n'
        '    void PublishShader(uint64_t hash, const ComPtr<ID3DBlob>& code);\n'
        '    ComPtr<ID3D12PipelineState> AdoptPipeline(uint64_t hash);\n'
        '    void PublishPipeline(uint64_t hash);\n'
        '    ID3DBlob* PrecompileShader(const std::string& src, const char* entry, const char* profile, uint64_t& hash);\n'
        '    void RecordPipeline(const State& s, const Assembled& a, const DrawShaders& sh, const PixelKey& pk, const PsoKey& k);\n')

    # Start after the device, root signature and settings exist.
    backend = replace_once(backend,
        '    LOGI("d3d12: %ux%u guest, %ux%u window, render scale %.3f", width, height, bb_w_, bb_h_, scale_);\n',
        '    LOGI("d3d12: %ux%u guest, %ux%u window, render scale %.3f", width, height, bb_w_, bb_h_, scale_);\n'
        '    openshao::pipelines::Start(openshao::paths::Cache() / "d3d12" / "recipes.bin",\n'
        '                               0xD3D1200000000000ull | sizeof(PsoKey), this);\n'
        '    openshao::pipelines::SetCandidateDescriber([this](const auto& c, auto& r) { return DescribeCandidate(c, r); });\n'
        '    openshao::pipelines::SetPipelineRenderScale(scale_);\n'
        '    openshao::pipelines::WarmCommonScreenPipelines();\n')
    backend = replace_once(backend, '        scale_ = want;\n',
        '        scale_ = want;\n'
        '        openshao::pipelines::SetPipelineRenderScale(scale_);\n')

    start = backend.index('void D3D12Backend::BuildPipelineKey(')
    end = backend.index('\n}\n', start) + 3
    signature = backend[start:backend.index('{', start) + 1]
    backend = backend[:start] + signature + '''
    openshao::pipelines::PipelinePass pass;
    pass.color = bound_rt_ != nullptr;
    pass.depth = bound_ds_ != nullptr;
    pass.color_format = bound_rt_ ? bound_rt_->fmt : 0;
    pass.depth_format = bound_ds_ ? openshao::ShadowFormats(bound_ds_->res->GetDesc().Format).view : 0;
    pass.stencil = bound_ds_ && openshao::ShadowFormats(bound_ds_->res->GetDesc().Format).stencil;
    pass.depth_scale = bound_ds_ ? float(bound_ds_->w) / bound_ds_->gw : 1;
    openshao::pipelines::FillPipelineKey(s, a, vh, ph, pass, k);
}
''' + backend[end:]

    # Shaders: wait for a worker compiling the same source, then take its result.
    backend = replace_once(backend,
        '    ComPtr<ID3DBlob> code;\n'
        '    if (disk_cache_.LoadShader(hash, code)) {\n'
        '        blobs_[hash] = code;\n'
        '        return code.Get();\n'
        '    }\n',
        '    ComPtr<ID3DBlob> code;\n'
        '    openshao::pipelines::InFlightScope shader_flight{pre_shader_flight_, hash};\n'
        '    if ((code = AdoptShader(hash))) {\n'
        '        blobs_[hash] = code;\n'
        '        return code.Get();\n'
        '    }\n'
        '    if (disk_cache_.LoadShader(hash, code)) {\n'
        '        PublishShader(hash, code);\n'
        '        blobs_[hash] = code;\n'
        '        return code.Get();\n'
        '    }\n')
    backend = replace_once(backend,
        '    disk_cache_.StoreShader(hash, code.Get(), PerfMs(compile_started));\n'
        '    blobs_[hash] = code;\n    return code.Get();',
        '    disk_cache_.StoreShader(hash, code.Get(), PerfMs(compile_started));\n'
        '    PublishShader(hash, code);\n'
        '    blobs_[hash] = code;\n    return code.Get();')

    # Pipelines: BuildPipeline in d3d12_precompile.inl mirrors this body.
    ensure = (
        'ID3D12PipelineState* D3D12Backend::EnsurePipeline(const PsoKey& k, ID3DBlob* vs, ID3DBlob* ps) {\n'
        '    uint64_t h = Hash(&k, sizeof(k));\n'
        '    auto it = psos_.find(h);\n'
        '    if (it != psos_.end()) return it->second.Get();\n'
        '    D3D12_INPUT_ELEMENT_DESC el[16];\n'
        '    uint32_t n = FillInputLayout(k, el);\n'
        '    D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};\n'
        '    d.InputLayout = {el, n};\n'
        '    FillPipelineDesc(k, vs, ps, d);\n'
        '    if (k.topo == uint32_t(Topology::Points)) ExpandPoints(d, k.sprite);\n'
        '    if (k.atlas_guard) {\n'
        '        uint64_t hash;\n'
        '        ID3DBlob* gs = Compile(openshao::AtlasBoundsHlsl(), "main", "gs_5_0", hash);\n'
        '        if (!gs) return nullptr;\n'
        '        d.GS = {gs->GetBufferPointer(), gs->GetBufferSize()};\n'
        '    }\n'
        '    ComPtr<ID3D12PipelineState> pso;\n'
        '    g_perf.psos++;\n'
        '    if (FAILED(disk_cache_.CreatePipeline(&d, IID_PPV_ARGS(&pso)))) LOGE("d3d12: pipeline creation failed");\n'
        '    psos_[h] = pso;\n'
        '    return pso.Get();\n'
        '}\n')
    expand = (
        'void D3D12Backend::ExpandPoints(D3D12_GRAPHICS_PIPELINE_STATE_DESC& d, bool sprite) {\n'
        '    uint64_t hash;\n'
        '    ID3DBlob* gs = Compile(sprite ? SpriteHlsl() : PointHlsl(), "main", "gs_5_0", hash);\n'
        '    if (!gs) return;\n'
        '    d.GS = {gs->GetBufferPointer(), gs->GetBufferSize()};\n'
        '    d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;\n'
        '    d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;\n'
        '    d.RasterizerState.FrontCounterClockwise = FALSE;\n'
        '}\n')
    for mirrored in (
        expand,
        '    std::string src = VertexHlsl(s, a, sh);\n'
        '    sh.vs_code = Compile(sh.quad ? QuadHlsl(std::move(src)) : src, "main", "vs_5_0", sh.vh);\n',
        '    sh.ps_code = Compile(PixelHlsl(pk), "main", "ps_5_0", sh.ph);\n',
        '    if (sh.prog) {\n'
        '        std::string src = VertexProgramHlsl(s.vs->program, a.regs, a.decode);\n'
        '        return sh.sprite ? SpriteSizeHlsl(std::move(src)) : src;\n'
        '    }\n'
        '    return FixedVertexHlsl(a.regs, a.pretransformed, a.decode, sh.fk);\n',
    ):
        if backend.count(mirrored) != 1:
            raise ValueError('The pinned renderer changed code that d3d12_precompile.inl mirrors')
    backend = replace_once(backend, ensure, ensure
        .replace('    uint64_t h = Hash(&k, sizeof(k));\n',
                 '    pso_first_use_ = pso_compiled_on_draw_ = false;\n'
                 '    uint64_t h = Hash(&k, sizeof(k));\n')
        .replace('    if (it != psos_.end()) return it->second.Get();\n',
                 '    if (it != psos_.end()) return it->second.Get();\n'
                 '    openshao::pipelines::InFlightScope pso_flight{pre_pso_flight_, h};\n'
                  '    if (ComPtr<ID3D12PipelineState> ready = AdoptPipeline(h)) {\n'
                  '        pso_first_use_ = true;\n'
                  '        return (psos_[h] = std::move(ready)).Get();\n'
                  '    }\n')
        .replace('    psos_[h] = pso;\n',
                 '    psos_[h] = pso;\n'
                 '    PublishPipeline(h);\n'
                  '    pso_first_use_ = pso_compiled_on_draw_ = pso != nullptr;\n'))

    backend = replace_once(backend,
        '    ID3D12PipelineState* pso = EnsurePipeline(k, sh.vs_code, sh.ps_code);\n    if (!pso) return;\n',
        '    ID3D12PipelineState* pso = EnsurePipeline(k, sh.vs_code, sh.ps_code);\n'
        '    if (!pso) return;\n'
        '    if (pso_first_use_) {\n'
        '        openshao::pipelines::ObserveAssetPipeline(Hash(&k, sizeof(k)), pso_compiled_on_draw_);\n'
        '        RecordPipeline(s, a, sh, pk, k);\n'
        '    }\n')
    backend = replace_once(backend,
        '}  // namespace\n\nstd::unique_ptr<Backend> CreateBackend() { return std::make_unique<D3D12Backend>(); }',
        '#include "d3d12_precompile.inl"\n\n'
        '}  // namespace\n\nstd::unique_ptr<Backend> CreateBackend() { return std::make_unique<D3D12Backend>(); }')
    return replace_once(backend,
        'std::unique_ptr<Backend> CreateBackend() { return std::make_unique<D3D12Backend>(); }',
        'std::unique_ptr<Backend> CreateD3D12Backend() { return std::make_unique<D3D12Backend>(); }')


def adapt_d3d8(d3d, replace_once):
    """Programs the title creates while loading assets release the recipes that use them."""
    # After the platform headers the adaptation puts first.
    d3d = replace_once(d3d, '#include "platform_compat.h"\n#include <cstdint>\n',
        '#include "platform_compat.h"\n#include <cstdint>\n#include "pipeline_precompile.h"\n#include "pipeline_assets.h"\n')
    d3d = replace_once(d3d, 'std::recursive_mutex& Lock() { return g_lock; }',
        'std::recursive_mutex& Lock() { return g_lock; }\n#include "pipeline_shader_snapshot.inl"')
    d3d = replace_once(d3d,
        '    W32(out, vs->handle);\n',
        '    W32(out, vs->handle);\n'
        '    openshao::pipelines::WarmInterfaceShader(vs->handle);\n')
    return replace_once(d3d,
        '    gpu::ParseDeclaration(*vs);\n    uint32_t rec = mem::AllocSystem(16, 16);\n',
        '    gpu::ParseDeclaration(*vs);\n'
        '    vs->hash = openshao::pipelines::ProgramHash(vs->program);\n'
        '    openshao::pipelines::RegisterProgram(vs->program);\n'
        '    uint32_t rec = mem::AllocSystem(16, 16);\n')
