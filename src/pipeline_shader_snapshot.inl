// Included inside namespace d3d by the permanent SDK adaptation.
bool SnapshotPipelineShaders(uint32_t vertex, uint32_t pixel, gpu::State& out) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    out.vs_handle = vertex;
    out.vs.reset(); out.ps.reset();
    if (vertex & 1) {
        auto it = g_vs.find(vertex);
        if (it == g_vs.end()) return false;
        out.vs = std::make_shared<gpu::VertexShader>(*it->second);
        out.vs->hash = openshao::pipelines::ProgramHash(out.vs->program);
    }
    if (pixel) {
        auto it = g_ps.find(pixel);
        if (it == g_ps.end()) return false;
        out.ps = std::make_shared<gpu::PixelShader>(*it->second);
        out.rs[gpu::kRsPsTextureModes] = out.ps->def[54];
    }
    return vertex != 0;
}
