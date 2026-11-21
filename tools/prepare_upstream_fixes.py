"""Backport XboxXBERecomp fixes made after the pinned revision that apply to this title.

Each change names its upstream commit. The source is
https://github.com/TGP482/XboxXBERecomp (pinned f7b13fd, fixes from 2026-11-01 to 2026-11-08).
The Vulkan renderer in src/linux/vk/vk_backend.cpp carries the same renderer fixes.
"""


def adapt_d3d8(text, replace_once):
    # af7d5ab: SetGammaRamp takes the device lock before it reaches the renderer.
    text = replace_once(text, '    if (g_backend) g_backend->SetGamma(ramp);\n',
        '    std::lock_guard<std::recursive_mutex> lock(g_lock);\n'
        '    if (g_backend) g_backend->SetGamma(ramp);\n')
    # 9e96fe5: SetViewport(NULL) keeps the stored viewport, as the library does.
    text = replace_once(text,
        '    g_state.viewport = {R32(v), R32(v + 4), R32(v + 8), R32(v + 12), GF(v + 16), GF(v + 20)};\n',
        '    // null keeps the stored viewport, as the library does\n'
        '    if (v) g_state.viewport = {R32(v), R32(v + 4), R32(v + 8), R32(v + 12), GF(v + 16), GF(v + 20)};\n')
    return text


def adapt_backend(text, replace_once):
    # 655fdcd: the title can ask for a LOD bias past the range the hardware takes.
    text = replace_once(text, '    sd.MipLODBias = Bits(t[kTssMipLodBias]);\n',
        '    // the title can ask past the range the hardware takes\n'
        '    sd.MipLODBias = std::clamp(Bits(t[kTssMipLodBias]), D3D12_MIP_LOD_BIAS_MIN, D3D12_MIP_LOD_BIAS_MAX);\n')
    # 6c63e71: a fixed function stage with a texture matrix or generated coordinates keeps wrapping.
    text = replace_once(text,
        '    uint32_t EnsureStageSamplers(const State& s, const DrawTextures& stages, bool clamp);',
        '    uint32_t EnsureStageSamplers(const State& s, const DrawTextures& stages, bool clamp, bool fixed);')
    text = replace_once(text,
        'D3D12_SAMPLER_DESC StageSampler(const State& s, int i, const DrawTextures& stages, bool clamp, uint32_t aniso) {',
        'D3D12_SAMPLER_DESC StageSampler(const State& s, int i, const DrawTextures& stages, bool clamp, bool fixed,\n'
        '                                uint32_t aniso) {')
    text = replace_once(text,
        '    // clamp: the draw\'s coordinates all lie in 0..1 (UnitTexcoords), so wrap is read as clamp\n',
        '    // clamp: the draw\'s coordinates all lie in 0..1 (UnitTexcoords), so wrap is read as clamp, unless the fixed\n'
        '    // function moves this stage\'s out of that range with a texture matrix or generates them\n'
        '    if (fixed && ((t[kTssTexCoordIndex] >> 16 & 7) || (t[kTssTextureTransformFlags] & 7))) clamp = false;\n')
    text = replace_once(text,
        'uint32_t D3D12Backend::EnsureStageSamplers(const State& s, const DrawTextures& stages, bool clamp) {',
        'uint32_t D3D12Backend::EnsureStageSamplers(const State& s, const DrawTextures& stages, bool clamp, bool fixed) {')
    text = replace_once(text,
        '    for (int i = 0; i < 4; i++) sd[i] = StageSampler(s, i, stages, clamp, aniso);',
        '    for (int i = 0; i < 4; i++) sd[i] = StageSampler(s, i, stages, clamp, fixed, aniso);')
    text = replace_once(text,
        '    uint32_t samplers = EnsureStageSamplers(s, stages, AnyStageWraps(s) && UnitTexcoords(a, stages));',
        '    bool fixed = !a.pretransformed && !((s.vs_handle & 1) && s.vs && !s.vs->program.empty());\n'
        '    uint32_t samplers = EnsureStageSamplers(s, stages, AnyStageWraps(s) && UnitTexcoords(a, stages), fixed);')
    # b69b4bb: the position register is not a texture coordinate; 2D sprites send x, y in pixels as FLOAT2.
    text = replace_once(text,
        '    bool any = false;\n    for (int r = 0; r < 16; r++) {\n        const VertexElement& e = a.elements[size_t(r)];',
        '    bool any = false;\n'
        '    constexpr int kPositionRegister = 0;  // D3DVSDE_POSITION; 2D sprites send x, y in pixels as FLOAT2\n'
        '    for (int r = kPositionRegister + 1; r < 16; r++) {\n'
        '        const VertexElement& e = a.elements[size_t(r)];')
    # e867755: after a flush the GPU is idle, so every report asked for so far is written.
    text = replace_once(text, 'void D3D12Backend::Flush() { Submit(true); }\n',
        '// the GPU is idle after it: every report asked for so far is written, as a title waiting on one expects\n'
        'void D3D12Backend::Flush() {\n'
        '    Submit(true);\n'
        '    DeliverReports();\n'
        '}\n')
    return text


def adapt_shaders(text, replace_once):
    # d834100: the final combiner reads r0 unsigned; a negative alpha reaches the alpha test as 0.
    text = replace_once(text,
        '            body += "float4 outc = float4(lerp(fogin.rgb, r0.rgb, fogin.a), r0.a);\\n";\n',
        '            // the final combiner reads r0 unsigned, a negative alpha reaches the alpha test as 0\n'
        '            body += "float4 outc = float4(lerp(fogin.rgb, saturate(r0.rgb), fogin.a), saturate(r0.a));\\n";\n')
    # a50a366: w 0 is screen space; clamped to 2^-64 its clip coordinates lose draws on AMD, w 1 draws the same.
    text = replace_once(text, '    p.w = magclamp(p.w);\n',
        '    // w 0 is screen space; clamped to 2^-64 its clip coordinates near 1e-20 lose draws on AMD, w 1 draws the same\n'
        '    p.w = p.w == 0 ? 1 : magclamp(p.w);\n')
    return text
