// Same-layout Reset keeps allocations, discards contents, restores the viewport; layout changes recreate.
HLE_STDCALL(D3DDevice_Reset, 1) {
    constexpr uint32_t invalid = 0x8876086C;
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    const uint32_t pp = a[0];
    if (!g_backend || !pp || pp > UINT32_MAX - 52 ||
        !mem::IsCommitted(pp) || !mem::IsCommitted(pp + 51)) return invalid;
    std::array<uint32_t, 13> requested;
    std::memcpy(requested.data(), HostPtr(pp), sizeof(requested));
    if (!requested[0]) requested[0] = 640;
    if (!requested[1]) requested[1] = 480;
    requested[3] = std::max(requested[3], 1u);
    for (size_t i = 0; i < requested.size(); ++i) {
        if (requested[i] != g_openshao_present[i]) {
            LOGE("d3d: Reset layout change is unsupported (field %zu: %X -> %X)", i, g_openshao_present[i], requested[i]);
            return invalid;
        }
    }
    if (Dev(kDevRenderTarget) != g_buffers[1] || Dev(kDevDepth) != g_depth) {
        LOGE("d3d: Reset with explicit render targets is unsupported");
        return invalid;
    }
    // Wait for queued commands before discarding contents; guest XMemFree does not own surface headers.
    g_backend->Flush();
    for (uint32_t i = 0; i < g_buffer_count; ++i) {
        gpu::State clear = g_state;
        clear.rt = SurfaceOf(g_buffers[i]);
        clear.zs = SurfaceOf(g_depth);
        uint32_t flags = gpu::kClearTarget;
        if (g_depth) {
            flags |= gpu::kClearZ;
            const uint32_t format = R32(g_depth + 12) >> 8 & 0xFF;
            if (format == 0x2E || format == 0x2F) flags |= gpu::kClearStencil;
        }
        g_backend->Clear(clear, 0, nullptr, flags, 0, 1.0f, 0);
    }
    g_backend->Flush();
    // Make the cleared buffers visible to CPU readers as well as subsequent draws.
    for (uint32_t i = 0; i < g_buffer_count; ++i) {
        const uint32_t data = mem::VirtualFromPhysical(R32(g_buffers[i] + 4));
        g_backend->Sync(data, 0);
    }
    if (g_depth) g_backend->Sync(mem::VirtualFromPhysical(R32(g_depth + 4)), 0);
    g_state.viewport = {0, 0, requested[0], requested[1], 0.0f, 1.0f};
    UpdateViewportConstants();
    LOGR("d3d: Reset complete %ux%u; %u implicit buffers cleared, allocations and pipeline preserved",
         requested[0], requested[1], g_buffer_count);
    return 0;
}
