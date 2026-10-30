// Pipeline precompilation for the adapted Direct3D 12 renderer. Included at the end of its anonymous
// namespace by tools/prepare_pipeline_precompile.py, which also adds the members used here.
static_assert(sizeof(PsoKey) <= openshao::pipelines::kMaxPipelineKey);

D3D12Backend::~D3D12Backend() {
    openshao::pipelines::ReportAssetPipelines();
    openshao::pipelines::ClearCandidateDescriber();
    openshao::pipelines::Shutdown();
}

bool D3D12Backend::DescribeCandidate(const openshao::pipelines::PipelineCandidate& c,
                                    openshao::pipelines::Recipe& r) {
    const auto& s = c.state;
    const auto& a = c.layout;
    auto sh = ClassifyShaders(s, a, c.count);
    auto source_hash = [](const std::string& src, const char* profile) {
        const uint64_t seed = Hash(profile, std::strlen(profile));
        return Hash(src.data(), src.size(), Hash("main", 4, seed));
    };
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
        return source_hash(vs, "vs_5_0");
    });
    r.ps_source = candidate_sources_.Pixel(pk, [&] { return source_hash(PixelHlsl(pk), "ps_5_0"); });
    openshao::pipelines::PipelinePass pass;
    pass.color = !c.shadow || c.shadow_color;
    pass.depth = c.depth;
    pass.depth_scale = openshao::pipelines::PipelineDepthScale(c, guest_w_);
    pass.color_format = DXGI_FORMAT_B8G8R8A8_UNORM;
    pass.depth_format = c.shadow && depth32_supported_ ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_D24_UNORM_S8_UINT;
    pass.stencil = !c.shadow || !depth32_supported_;
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

ComPtr<ID3DBlob> D3D12Backend::AdoptShader(uint64_t hash) {
    std::lock_guard lock(pre_mutex_);
    auto it = pre_blobs_.find(hash);
    return it == pre_blobs_.end() ? nullptr : it->second;
}

void D3D12Backend::PublishShader(uint64_t hash, const ComPtr<ID3DBlob>& code) {
    if (!code) return;
    std::lock_guard lock(pre_mutex_);
    pre_blobs_.try_emplace(hash, code);
}

ComPtr<ID3D12PipelineState> D3D12Backend::AdoptPipeline(uint64_t hash) {
    std::lock_guard lock(pre_mutex_);
    auto it = pre_psos_.find(hash);
    if (it == pre_psos_.end()) return nullptr;
    ComPtr<ID3D12PipelineState> pso = std::move(it->second);
    pre_psos_.erase(it);
    return pso;
}

void D3D12Backend::PublishPipeline(uint64_t hash) {
    std::lock_guard lock(pre_mutex_);
    pre_done_.insert(hash);
}

// Worker thread. Uses Compile's key; pre_blobs_ keeps the blob, so the pointer stays valid.
ID3DBlob* D3D12Backend::PrecompileShader(const std::string& src, const char* entry, const char* profile,
                                         uint64_t& hash) {
    uint64_t profile_seed = Hash(profile, std::strlen(profile));
    uint64_t entry_seed = Hash(entry, std::strlen(entry), profile_seed);
    hash = Hash(src.data(), src.size(), entry_seed);
    openshao::pipelines::InFlightScope flight{pre_shader_flight_, hash};
    if (ComPtr<ID3DBlob> ready = AdoptShader(hash)) return ready.Get();
    ComPtr<ID3DBlob> code;
    if (!disk_cache_.LoadShader(hash, code)) {
        ComPtr<ID3DBlob> err;
        const auto started = PerfClock::now();
        if (FAILED(D3DCompile(src.data(), src.size(), nullptr, nullptr, nullptr, entry, profile,
                              D3DCOMPILE_OPTIMIZATION_LEVEL1, 0, &code, &err))) {
            LOG_FIRST(8, "pipeline precompile: %s compile failed: %s", profile,
                      err ? static_cast<const char*>(err->GetBufferPointer()) : "?");
            return nullptr;
        }
        disk_cache_.StoreShader(hash, code.Get(), PerfMs(started));
    }
    std::lock_guard lock(pre_mutex_);
    return pre_blobs_.try_emplace(hash, code).first->second.Get();
}

// Worker thread. Mirrors EnsureVertexShader, EnsurePixelShader, EnsurePipeline and ExpandPoints;
// the adaptation script stops the build when the SDK changes those.
bool D3D12Backend::BuildPipeline(const openshao::pipelines::Recipe& r, std::span<const uint32_t> program) {
    if (r.key_size != sizeof(PsoKey)) return false;
    const auto& v = r.vertex;
    std::string vsrc = v.program ? VertexProgramHlsl(std::vector<uint32_t>(program.begin(), program.end()), v.regs, v.decode)
                                 : FixedVertexHlsl(v.regs, v.pretransformed, v.decode, v.fixed);
    if (v.quad) vsrc = QuadHlsl(std::move(vsrc));
    uint64_t vh = 0, ph = 0, gh = 0;
    ID3DBlob* vs = PrecompileShader(vsrc, "main", "vs_5_0", vh);
    ID3DBlob* ps = PrecompileShader(PixelHlsl(r.pixel), "main", "ps_5_0", ph);
    if (!vs || !ps) return false;
    if (vh != r.vs_source || ph != r.ps_source)
        LOG_FIRST(8, "pipeline precompile: recipe sources changed (vertex %d, pixel %d); building the current ones",
                  vh != r.vs_source, ph != r.ps_source);
    PsoKey k;
    std::memcpy(&k, r.key, sizeof(k));
    k.vs = vh;
    k.ps = ph;
    // Geometry shaders compile before the pipeline key is held: a thread never holds two keys.
    ID3DBlob* points = k.topo == uint32_t(Topology::Points) ? PrecompileShader(PointHlsl(), "main", "gs_5_0", gh) : nullptr;
    ID3DBlob* atlas = k.atlas_guard ? PrecompileShader(openshao::AtlasBoundsHlsl(), "main", "gs_5_0", gh) : nullptr;
    if (k.atlas_guard && !atlas) return false;
    D3D12_INPUT_ELEMENT_DESC el[16];
    uint32_t n = FillInputLayout(k, el);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
    d.InputLayout = {el, n};
    FillPipelineDesc(k, vs, ps, d);
    if (points) {
        d.GS = {points->GetBufferPointer(), points->GetBufferSize()};
        d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        d.RasterizerState.FrontCounterClockwise = FALSE;
    }
    if (atlas) d.GS = {atlas->GetBufferPointer(), atlas->GetBufferSize()};
    const uint64_t h = Hash(&k, sizeof(k));
    openshao::pipelines::InFlightScope flight{pre_pso_flight_, h};
    {
        std::lock_guard lock(pre_mutex_);
        if (pre_done_.count(h)) return true;
    }
    ComPtr<ID3D12PipelineState> pso;
    if (FAILED(disk_cache_.CreatePipeline(&d, IID_PPV_ARGS(&pso)))) return false;
    std::lock_guard lock(pre_mutex_);
    pre_done_.insert(h);
    pre_psos_[h] = std::move(pso);
    return true;
}

void D3D12Backend::RecordPipeline(const State& s, const Assembled& a, const DrawShaders& sh, const PixelKey& pk,
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
