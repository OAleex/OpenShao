#include "d3d12_cache.h"
#include "core/log.h"
#include <dxgi1_6.h>
#include <bit>
#include <chrono>
#include <cstring>
#include <type_traits>

namespace openshao::graphics {
namespace {
using Microsoft::WRL::ComPtr;
using Clock = std::chrono::steady_clock;
struct Key {
    uint64_t hash;
    template<class T> void Add(T value) {
        uint64_t bits;
        if constexpr (std::is_same_v<T, float>) bits = std::bit_cast<uint32_t>(value);
        else bits = uint64_t(value);
        for (unsigned i = 0; i < 8; ++i) { hash = (hash ^ uint8_t(bits)) * 1099511628211ull; bits >>= 8; }
    }
    void Data(const void* data, size_t size) {
        Add(size);
        if (size) hash = CacheHash({static_cast<const uint8_t*>(data), size}, hash);
    }
    void Text(const char* text) { Data(text, text ? std::strlen(text) : 0); }
};
double Milliseconds(Clock::time_point start) { return std::chrono::duration<double, std::milli>(Clock::now() - start).count(); }
}

uint64_t PipelineKey(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& d, uint64_t root_hash) {
    Key k{root_hash};
    // Encode values explicitly: pointers and structure padding change between sessions.
    for (const auto& shader : {d.VS, d.PS, d.DS, d.HS, d.GS}) k.Data(shader.pShaderBytecode, shader.BytecodeLength);
    k.Add(d.StreamOutput.NumEntries);
    for (UINT i = 0; i < d.StreamOutput.NumEntries; ++i) {
        const auto& e = d.StreamOutput.pSODeclaration[i];
        k.Add(e.Stream); k.Text(e.SemanticName); k.Add(e.SemanticIndex);
        k.Add(e.StartComponent); k.Add(e.ComponentCount); k.Add(e.OutputSlot);
    }
    k.Add(d.StreamOutput.NumStrides);
    for (UINT i = 0; i < d.StreamOutput.NumStrides; ++i) k.Add(d.StreamOutput.pBufferStrides[i]);
    k.Add(d.StreamOutput.RasterizedStream);
    k.Add(d.BlendState.AlphaToCoverageEnable); k.Add(d.BlendState.IndependentBlendEnable);
    for (const auto& b : d.BlendState.RenderTarget) {
        k.Add(b.BlendEnable); k.Add(b.LogicOpEnable); k.Add(b.SrcBlend); k.Add(b.DestBlend); k.Add(b.BlendOp);
        k.Add(b.SrcBlendAlpha); k.Add(b.DestBlendAlpha); k.Add(b.BlendOpAlpha); k.Add(b.LogicOp); k.Add(b.RenderTargetWriteMask);
    }
    k.Add(d.SampleMask);
    const auto& r = d.RasterizerState;
    k.Add(r.FillMode); k.Add(r.CullMode); k.Add(r.FrontCounterClockwise); k.Add(r.DepthBias);
    k.Add(r.DepthBiasClamp); k.Add(r.SlopeScaledDepthBias); k.Add(r.DepthClipEnable);
    k.Add(r.MultisampleEnable); k.Add(r.AntialiasedLineEnable); k.Add(r.ForcedSampleCount); k.Add(r.ConservativeRaster);
    const auto& z = d.DepthStencilState;
    k.Add(z.DepthEnable); k.Add(z.DepthWriteMask); k.Add(z.DepthFunc); k.Add(z.StencilEnable);
    k.Add(z.StencilReadMask); k.Add(z.StencilWriteMask);
    for (const auto& f : {z.FrontFace, z.BackFace}) {
        k.Add(f.StencilFailOp); k.Add(f.StencilDepthFailOp); k.Add(f.StencilPassOp); k.Add(f.StencilFunc);
    }
    k.Add(d.InputLayout.NumElements);
    for (UINT i = 0; i < d.InputLayout.NumElements; ++i) {
        const auto& e = d.InputLayout.pInputElementDescs[i];
        k.Text(e.SemanticName); k.Add(e.SemanticIndex); k.Add(e.Format); k.Add(e.InputSlot);
        k.Add(e.AlignedByteOffset); k.Add(e.InputSlotClass); k.Add(e.InstanceDataStepRate);
    }
    k.Add(d.IBStripCutValue); k.Add(d.PrimitiveTopologyType); k.Add(d.NumRenderTargets);
    for (auto format : d.RTVFormats) k.Add(format);
    k.Add(d.DSVFormat); k.Add(d.SampleDesc.Count); k.Add(d.SampleDesc.Quality); k.Add(d.NodeMask); k.Add(d.Flags);
    return k.hash;
}

D3D12Cache::~D3D12Cache() {
    if (shaders_) shaders_->Flush();
    if (pipelines_) pipelines_->Flush();
    if (shaders_) Report();
}

void D3D12Cache::Open(const std::filesystem::path& directory, ID3D12Device* device, uint64_t version) {
    device_ = device;
    shaders_ = std::make_unique<CacheFile>(directory / "shaders.bin", version);
    Key identity{version};
    ComPtr<IDXGIFactory4> factory;
    ComPtr<IDXGIAdapter1> adapter;
    const LUID luid = device->GetAdapterLuid();
    LARGE_INTEGER driver{};
    DXGI_ADAPTER_DESC1 desc{};
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) &&
        SUCCEEDED(factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&adapter))) &&
        SUCCEEDED(adapter->GetDesc1(&desc))) {
        identity.Add(desc.VendorId); identity.Add(desc.DeviceId); identity.Add(desc.SubSysId); identity.Add(desc.Revision);
        if (SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &driver))) identity.Add(driver.QuadPart);
        else { identity.Add(luid.LowPart); identity.Add(luid.HighPart); }
    } else { identity.Add(luid.LowPart); identity.Add(luid.HighPart); }
    pipelines_ = std::make_unique<CacheFile>(directory / "pipelines.bin", identity.hash);
    LOGR("graphics cache: directory %ls", directory.c_str());
    LOGR("graphics cache: loaded %zu shaders, %zu pipelines; writable shaders=%d pipelines=%d",
         shaders_->Loaded(), pipelines_->Loaded(), shaders_->Writable(), pipelines_->Writable());
    if (shaders_->Errors() || pipelines_->Errors()) LOGR("graphics cache: storage unavailable; rendering continues without persistence");
}

void D3D12Cache::SetRootSignature(std::span<const uint8_t> bytes) { root_hash_ = CacheHash(bytes); }

bool D3D12Cache::LoadShader(uint64_t key, ComPtr<ID3DBlob>& blob) {
    if (!shaders_) return false;
    const auto bytes = shaders_->Get(key);
    if (bytes.empty() || FAILED(D3DCreateBlob(bytes.size(), &blob))) return false;
    std::memcpy(blob->GetBufferPointer(), bytes.data(), bytes.size());
    std::lock_guard lock(stats_mutex_);
    stats_.shader_hits++;
    return true;
}

void D3D12Cache::StoreShader(uint64_t key, ID3DBlob* blob, double milliseconds) {
    uint64_t compiles;
    {
        std::lock_guard lock(stats_mutex_);
        compiles = ++stats_.shader_compiles;
        stats_.shader_ms += milliseconds;
    }
    if (blob && shaders_) shaders_->Put(key, {static_cast<const uint8_t*>(blob->GetBufferPointer()), blob->GetBufferSize()});
    if (milliseconds >= 8 && compiles <= 64)
        LOGR("graphics cache: shader %016llx compiled in %.2f ms", static_cast<unsigned long long>(key), milliseconds);
}

HRESULT D3D12Cache::CreatePipeline(const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc, REFIID iid, void** result) {
    const auto start = Clock::now();
    const uint64_t key = PipelineKey(*desc, root_hash_);
    const auto bytes = pipelines_ ? pipelines_->Get(key) : std::vector<uint8_t>{};
    auto d = *desc;
    d.CachedPSO = {bytes.empty() ? nullptr : bytes.data(), bytes.size()};
    ComPtr<ID3D12PipelineState> state;
    HRESULT hr = device_->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&state));
    const bool hit = !bytes.empty() && SUCCEEDED(hr);
    const bool rejected = FAILED(hr) && !bytes.empty();
    if (rejected) {
        d.CachedPSO = {};
        state.Reset();
        hr = device_->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&state));
    }
    const double elapsed = Milliseconds(start);
    uint64_t made;
    {
        std::lock_guard lock(stats_mutex_);
        stats_.pipeline_rejects += rejected;
        if (hit) stats_.pipeline_hits++;
        else stats_.pipeline_creates++;
        stats_.pipeline_ms += elapsed;
        made = stats_.pipeline_creates + stats_.pipeline_hits;
    }
    if (SUCCEEDED(hr) && !hit && pipelines_) {
        ComPtr<ID3DBlob> cached;
        if (SUCCEEDED(state->GetCachedBlob(&cached)) && cached && cached->GetBufferSize())
            pipelines_->Put(key, {static_cast<const uint8_t*>(cached->GetBufferPointer()), cached->GetBufferSize()});
    }
    if (elapsed >= 8 && made <= 64)
        LOGR("graphics cache: pipeline %016llx %s in %.2f ms", static_cast<unsigned long long>(key), hit ? "restored" : "created", elapsed);
    if (FAILED(hr)) { *result = nullptr; return hr; }
    return state.CopyTo(iid, result);
}

void D3D12Cache::Report() const {
    if (!shaders_ || !pipelines_) return;
    const CacheStats stats = Statistics();
    LOGR("graphics cache: session shaders hit=%llu compiled=%llu compile_ms=%.2f; pipelines hit=%llu created=%llu rejected=%llu create_ms=%.2f; saved shaders=%llu pipelines=%llu io_errors=%llu",
         static_cast<unsigned long long>(stats.shader_hits), static_cast<unsigned long long>(stats.shader_compiles), stats.shader_ms,
         static_cast<unsigned long long>(stats.pipeline_hits), static_cast<unsigned long long>(stats.pipeline_creates),
         static_cast<unsigned long long>(stats.pipeline_rejects), stats.pipeline_ms,
         static_cast<unsigned long long>(shaders_->Saved()), static_cast<unsigned long long>(pipelines_->Saved()),
         static_cast<unsigned long long>(shaders_->Errors() + pipelines_->Errors()));
}
}
