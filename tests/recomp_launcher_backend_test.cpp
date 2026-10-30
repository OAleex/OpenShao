// Fault injection stays at the COM boundary; the renderer and font atlas are real.
#define NOMINMAX
#define CINTERFACE
#define COBJMACROS
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <imgui.h>
#include <backends/imgui_impl_dx12.h>
#include <iostream>

struct DeviceProxy {
    ID3D12Device api;
    ID3D12Device* real;
    unsigned calls = 0, fail_at = 0;
};
template<auto Member, class Function> struct Forward;
template<auto Member, class Result, class... Args>
struct Forward<Member, Result (STDMETHODCALLTYPE*)(ID3D12Device*, Args...)> {
    static Result STDMETHODCALLTYPE Call(ID3D12Device* self, Args... args) {
        auto* real = reinterpret_cast<DeviceProxy*>(self)->real;
        return (real->lpVtbl->*Member)(real, args...);
    }
};
static HRESULT STDMETHODCALLTYPE Resource(ID3D12Device* self, const D3D12_HEAP_PROPERTIES* heap,
    D3D12_HEAP_FLAGS flags, const D3D12_RESOURCE_DESC* desc, D3D12_RESOURCE_STATES state,
    const D3D12_CLEAR_VALUE* clear, REFIID iid, void** output) {
    auto& proxy = *reinterpret_cast<DeviceProxy*>(self);
    if (++proxy.calls == proxy.fail_at) { *output = nullptr; return E_OUTOFMEMORY; }
    return ID3D12Device_CreateCommittedResource(proxy.real, heap, flags, desc, state, clear, iid, output);
}
int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    IDXGIFactory4* factory = nullptr;
    IDXGIAdapter* warp = nullptr;
    ID3D12Device* device = nullptr;
    if (FAILED(CreateDXGIFactory1(IID_IDXGIFactory4, reinterpret_cast<void**>(&factory))) ||
        FAILED(IDXGIFactory4_EnumWarpAdapter(factory, IID_IDXGIAdapter, reinterpret_cast<void**>(&warp))) ||
        FAILED(D3D12CreateDevice(reinterpret_cast<IUnknown*>(warp), D3D_FEATURE_LEVEL_11_0,
            IID_ID3D12Device, reinterpret_cast<void**>(&device)))) {
        std::cerr << "FAIL: WARP test device is unavailable\n"; return 1;
    }
    ID3D12DescriptorHeap* heap = nullptr;
    D3D12_DESCRIPTOR_HEAP_DESC descriptor{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0};
    if (FAILED(ID3D12Device_CreateDescriptorHeap(device, &descriptor, IID_ID3D12DescriptorHeap, reinterpret_cast<void**>(&heap)))) return 1;
    ID3D12DeviceVtbl table{};
#define FORWARD(member) table.member = &Forward<&ID3D12DeviceVtbl::member, decltype(table.member)>::Call
    FORWARD(QueryInterface); FORWARD(AddRef); FORWARD(Release);
    FORWARD(CreateRootSignature); FORWARD(CreateGraphicsPipelineState);
    FORWARD(CreateFence); FORWARD(CreateCommandQueue); FORWARD(CreateCommandAllocator);
    FORWARD(CreateCommandList); FORWARD(CreateShaderResourceView); FORWARD(GetDeviceRemovedReason);
#undef FORWARD
    table.CreateCommittedResource = Resource;
    DeviceProxy proxy{{&table}, device, 0, argc > 1 ? unsigned(std::atoi(argv[1])) : 1};
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = ImGui::GetIO().LogFilename = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(heap, &cpu);
    ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(heap, &gpu);
    ImGui_ImplDX12_Init(&proxy.api, 2, DXGI_FORMAT_R8G8B8A8_UNORM, heap, cpu, gpu);
    const bool ready = ImGui_ImplDX12_CreateDeviceObjects();
    const bool expected = proxy.fail_at == 0;
    ImGui_ImplDX12_Shutdown();
    ImGui::DestroyContext();
    ID3D12DescriptorHeap_Release(heap);
    ID3D12Device_Release(device);
    IDXGIAdapter_Release(warp);
    IDXGIFactory4_Release(factory);
    if (ready != expected) {
        std::cerr << "FAIL: font allocation failure must return false instead of crashing or reporting success\n"; return 1;
    }
    std::cout << "PASS: launcher font initialization handles allocation scenario " << proxy.fail_at << '\n';
}
