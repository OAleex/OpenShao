"""Preserve the shared forward-depth convention and character-shadow formats."""


def adapt_backend(text, replace_once):
    # The two OpenShao backends share shader generation and forward-depth shadow
    # comparisons. Keep that contract while adopting the SDK's transport changes.
    begin = text.index('// Depth is reversed in a 32 bit float buffer:')
    end = text.index('const char* kDepthHlsl', begin)
    text = text[:begin] + text[end:]
    for old, new in (
        ('kDepthTypeless', 'DXGI_FORMAT_R24G8_TYPELESS'),
        ('kDepthFormat', 'DXGI_FORMAT_D24_UNORM_S8_UINT'),
        ('kDepthRead', 'DXGI_FORMAT_R24_UNORM_X8_TYPELESS'),
        ('kDepthFar', 'one'),
    ):
        text = text.replace(old, new)
    text = replace_once(text, '        cv.DepthStencil.Depth = one;', '        cv.DepthStencil.Depth = 1;')
    text = replace_once(text, '        cl_->ClearDepthStencilView(Cpu(dsv_heap_.Get(), t.view, dsv_inc_),\n',
                        '        float one = 1;\n        cl_->ClearDepthStencilView(Cpu(dsv_heap_.Get(), t.view, dsv_inc_),\n')
    text = replace_once(text, '    g_reversed_depth = true;', '    g_reversed_depth = false;')
    text = text.replace('ReversedDepthHlsl(VertexHlsl(s, a, sh))', 'VertexHlsl(s, a, sh)')
    text = replace_once(text, '    ds.DepthFunc = ReversedCmp(k.zfunc);', '    ds.DepthFunc = Cmp(k.zfunc);')
    text = replace_once(text, '        sd.ComparisonFunc = Cmp(s.rs[kRsShadowFunc]);',
                        '        sd.ComparisonFunc = ReversedCmp(s.rs[kRsShadowFunc]);')
    text = replace_once(text,
        '// the console tests texel OP r, D3D12 tests r OP texel; with both reversed (g_reversed_depth) it is the same OP',
        '// The console tests texel OP r; the forward-depth sampler tests r OP texel.')
    text = replace_once(text, '        float reversed = 1 - std::clamp(c.z, 0.0f, 1.0f);',
                        '        float depth = std::clamp(c.z, 0.0f, 1.0f);')
    text = replace_once(text, 'cf, reversed, UINT8(c.stencil),', 'cf, depth, UINT8(c.stencil),')
    text = replace_once(text, '    FillDepthConstants(s, sh, k, c);\n', '')
    text = replace_once(text, 'constexpr uint32_t kZUseW = 2;',
                        'constexpr float kDepth24Max = 16777215.0f;\nconstexpr uint32_t kZUseW = 2;')
    text = replace_once(text, '''    // polygon offset: the slope here, toward the camera is up on reversed depth; the units are in ReversedDepthHlsl
    if (OffsetsPolygons(s, a.topology)) k.zslope = -HostSlopeScale(s, bound_ds_);''', '''    // polygon offset: bias in depth buffer units, the target is always 24 bit; the slope is per console pixel,
    // and an upscaled target's pixels are smaller
    if (OffsetsPolygons(s, a.topology)) {
        k.zbias = int32_t(std::lround(Bits(SimpleState(s, kMDepthOffset)) * kDepth24Max / s.zscale));
        k.zslope = HostSlopeScale(s, bound_ds_);
    }''')
    return text
