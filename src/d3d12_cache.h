#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "graphics_cache.h"
#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <mutex>

namespace openshao::graphics {
uint64_t PipelineKey(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, uint64_t root_hash);
struct CacheStats {
    uint64_t shader_hits = 0, shader_compiles = 0;
    uint64_t pipeline_hits = 0, pipeline_creates = 0, pipeline_rejects = 0;
    double shader_ms = 0, pipeline_ms = 0;
};
class D3D12Cache {
public:
    ~D3D12Cache();
    void Open(const std::filesystem::path& directory, ID3D12Device* device, uint64_t version);
    void SetRootSignature(std::span<const uint8_t> bytes);
    bool LoadShader(uint64_t key, Microsoft::WRL::ComPtr<ID3DBlob>& blob);
    void StoreShader(uint64_t key, ID3DBlob* blob, double milliseconds);
    HRESULT CreatePipeline(const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc, REFIID iid, void** result);
    void Report() const;
    // Thread safe: precompile workers share the cache with the render thread.
    CacheStats Statistics() const { std::lock_guard lock(stats_mutex_); return stats_; }
private:
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    std::unique_ptr<CacheFile> shaders_, pipelines_;
    uint64_t root_hash_ = 0;
    mutable std::mutex stats_mutex_;
    CacheStats stats_;
};
}
