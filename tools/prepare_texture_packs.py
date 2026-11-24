"""Connect texture pack replacements to the pinned D3D12 renderer."""


def adapt_backend(backend, replace_once):
    backend = '#include "d3d12_texture_pack.h"\n' + backend
    backend = replace_once(backend, '    Extension* ext_ = nullptr;',
        '    Extension* ext_ = nullptr;\n'
        '    openshao::graphics::D3D12HdTextures hd_;')
    backend = replace_once(backend, '    uint32_t pack_gen = 0;      // texturedumper::Generation the key was taken at, 0 after new bytes\n};',
        '    uint32_t pack_gen = 0;      // texturedumper::Generation the key was taken at, 0 after new bytes\n'
        '    openshao::graphics::HdTexState hd;\n};')
    backend = replace_once(backend, '    TexInfo& info = stages.info[stage];\n    StagePalette palette;',
        '    TexInfo& info = stages.info[stage];\n'
        '    hd_.BeginFrame(*this, presents_);\n'
        '    StagePalette palette;')
    backend = replace_once(backend,
        '            tx.sig = sig;\n'
        '            UploadTexture(tx, d, fi, p, colours, same);',
        '            tx.sig = sig;\n'
        '            hd_.Lookup(tx.hd, d, fi, p, same);\n'
        '            UploadTexture(tx, d, fi, p, colours, same);')
    backend = replace_once(backend, '    const Tex& drawn = tx.swap ? *tx.swap : tx;',
        '    if (hd_.Stale(tx.hd))\n'
        '        hd_.Lookup(tx.hd, d, fi, static_cast<const uint8_t*>(kernel::HostPtr(d.data)), false);\n'
        '    const Tex& drawn = tx.swap ? *tx.swap : tx;')
    backend = replace_once(backend,
        '    if (ext_) ext_->Sampled(d, nullptr, tx.flags, info);\n'
        '    return drawn.res.Get();',
        '    if (ext_) ext_->Sampled(d, nullptr, tx.flags, info);\n'
        '    if (const openshao::graphics::HdImage* hd = hd_.Use(*this, tx.hd)) {\n'
        '        stages.sv[stage].Format = hd->format;\n'
        '        stages.sv[stage].Texture2D.MipLevels = hd->levels;\n'
        '        return hd->resource.Get();\n'
        '    }\n'
        '    return drawn.res.Get();')
    backend = replace_once(backend, '    PerfScope perf_present{g_perf.present};\n    if (cap_) {\n        FinishCapture(front);',
        '    PerfScope perf_present{g_perf.present};\n'
        '    hd_.BeginFrame(*this, presents_);\n'
        '    if (cap_) {\n        FinishCapture(front);')
    backend = replace_once(backend, '    presents_seen_.store(presents_);',
        '    presents_seen_.store(presents_);\n'
        '    hd_.EndFrame(presents_);')
    return backend
