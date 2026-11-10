"""Connect persistent shader and pipeline storage to the pinned D3D12 renderer."""
import hashlib
from pathlib import Path


def adapt_backend(backend, replace_once):
    source = Path(__file__).resolve().parents[1] / 'src'
    epoch = hashlib.sha256(backend.encode())
    for name in ('d3d12_cache.cpp', 'd3d12_cache.h'):
        epoch.update((source / name).read_bytes())
    version = int.from_bytes(epoch.digest()[:8], 'little')
    backend = '#include "d3d12_cache.h"\n' + backend
    backend = replace_once(backend, '    std::unordered_map<uint64_t, ComPtr<ID3DBlob>> blobs_;',
        '    openshao::graphics::D3D12Cache disk_cache_;\n'
        '    std::unordered_map<uint64_t, ComPtr<ID3DBlob>> blobs_;')
    backend = replace_once(backend, '    if (!CreateDevice()) return false;',
        '    if (!CreateDevice()) return false;\n'
        f'    disk_cache_.Open(openshao::paths::Cache() / "d3d12", dev_.Get(), 0x{version:016x}ull);')
    backend = replace_once(backend,
        '    dev_->CreateRootSignature(0, rs->GetBufferPointer(), rs->GetBufferSize(), IID_PPV_ARGS(&root_));',
        '    disk_cache_.SetRootSignature({static_cast<const uint8_t*>(rs->GetBufferPointer()), rs->GetBufferSize()});\n'
        '    dev_->CreateRootSignature(0, rs->GetBufferPointer(), rs->GetBufferSize(), IID_PPV_ARGS(&root_));')
    backend = replace_once(backend,
        '    ComPtr<ID3DBlob> code;\n    ComPtr<ID3DBlob> err;\n    PerfScope perf_compile{g_perf.compile};',
        '    ComPtr<ID3DBlob> code;\n'
        '    if (disk_cache_.LoadShader(hash, code)) {\n'
        '        blobs_[hash] = code;\n'
        '        return code.Get();\n'
        '    }\n'
        '    ComPtr<ID3DBlob> err;\n'
        '    const auto compile_started = PerfClock::now();\n'
        '    PerfScope perf_compile{g_perf.compile};')
    backend = replace_once(backend, '    blobs_[hash] = code;\n    return code.Get();',
        '    disk_cache_.StoreShader(hash, code.Get(), PerfMs(compile_started));\n'
        '    blobs_[hash] = code;\n    return code.Get();')
    if backend.count('dev_->CreateGraphicsPipelineState(') != 5:
        raise ValueError('The pinned renderer pipeline creation paths changed')
    backend = backend.replace('dev_->CreateGraphicsPipelineState(', 'disk_cache_.CreatePipeline(')
    backend = replace_once(backend, '    presents_++;',
        '    presents_++;\n    if (presents_ == 1 || presents_ == 120 || presents_ % 600 == 0) disk_cache_.Report();')
    return backend
