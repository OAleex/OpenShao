// Launcher window for the Direct3D 12 target: Win32 with the Dear ImGui Win32 and Direct3D 12 backends.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <imgui.h>
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>
#include <algorithm>
#include <exception>
#include <string>
#include "launcher_window.h"
#include "core/log.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace openshao::launcher {
namespace {
constexpr UINT kFrames = 2;
constexpr wchar_t kClass[] = L"OpenShaoLauncher";
template <class T> void Release(T*& object) { if (object) { object->Release(); object = nullptr; } }
bool Succeeded(HRESULT result, const char* operation) {
    if (SUCCEEDED(result)) return true;
    LOGE("Launcher: %s failed (HRESULT %08lX)", operation, result);
    return false;
}
}

struct Window::State {
    HWND window = nullptr;
    ID3D12Device* device = nullptr;
    ID3D12CommandQueue* queue = nullptr;
    ID3D12DescriptorHeap* rtv_heap = nullptr;
    ID3D12DescriptorHeap* srv_heap = nullptr;
    ID3D12CommandAllocator* allocators[kFrames] = {};
    ID3D12GraphicsCommandList* list = nullptr;
    ID3D12Fence* fence = nullptr;
    HANDLE fence_event = nullptr;
    UINT64 fence_values[kFrames] = {}, last_fence = 0;
    IDXGISwapChain3* swap = nullptr;
    ID3D12Resource* targets[kFrames] = {};
    D3D12_CPU_DESCRIPTOR_HANDLE rtv[kFrames] = {};
    bool imgui = false, win32 = false, dx12 = false, closing = false, occluded = false, resized = false, failed = false;
    float scale = 1.f;

    bool Device();
    bool Targets();
    void ReleaseTargets();
    bool Wait();
    bool Check(HRESULT result, const char* operation);
    void Render();
};

namespace {
LRESULT CALLBACK Procedure(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    auto* s = reinterpret_cast<Window::State*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (s && s->imgui && ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(window, message, wp, lp)) return 1;
    switch (message) {
        case WM_SIZE: if (s && wp != SIZE_MINIMIZED) s->resized = true; return 0;
        case WM_GETMINMAXINFO: {
            const float scale = s ? s->scale : 1.f;
            auto* info = reinterpret_cast<MINMAXINFO*>(lp);
            info->ptMinTrackSize = {LONG(900 * scale), LONG(600 * scale)};
            return 0;
        }
        case WM_DPICHANGED: {
            const auto* suggested = reinterpret_cast<const RECT*>(lp);
            SetWindowPos(window, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                         suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_SYSCOMMAND: if ((wp & 0xfff0) == SC_KEYMENU) return 0; break;
        case WM_CLOSE: if (s) s->closing = true; return 0;
        default: break;
    }
    return DefWindowProcW(window, message, wp, lp);
}
}

bool Window::State::Device() {
    IDXGIFactory4* factory = nullptr;
    if (!Succeeded(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "CreateDXGIFactory1")) return false;
    // A software adapter keeps the launcher usable on systems without a Direct3D 12 driver.
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) {
        LOGR("Launcher: hardware Direct3D 12 is unavailable; trying WARP");
        IDXGIAdapter* warp = nullptr;
        if (Succeeded(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)), "EnumWarpAdapter")) {
            Succeeded(D3D12CreateDevice(warp, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)), "D3D12CreateDevice WARP");
            warp->Release();
        }
    }
    bool ok = device != nullptr;
    if (ok) {
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        ok = Succeeded(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue)), "CreateCommandQueue");
    }
    if (ok) {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = kFrames;
        ok = Succeeded(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtv_heap)), "CreateDescriptorHeap RTV");
        if (ok) {
            const auto step = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            auto handle = rtv_heap->GetCPUDescriptorHandleForHeapStart();
            for (UINT i = 0; i < kFrames; ++i) { rtv[i] = handle; handle.ptr += step; }
        }
    }
    if (ok) {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 1;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ok = Succeeded(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srv_heap)), "CreateDescriptorHeap SRV");
    }
    for (UINT i = 0; ok && i < kFrames; ++i)
        ok = Succeeded(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocators[i])), "CreateCommandAllocator");
    if (ok) ok = Succeeded(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocators[0], nullptr, IID_PPV_ARGS(&list)), "CreateCommandList") &&
                 Succeeded(list->Close(), "Close initial command list");
    if (ok) ok = Succeeded(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "CreateFence");
    if (ok) ok = (fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr)) != nullptr;
    if (ok) {
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.BufferCount = kFrames;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.SampleDesc.Count = 1;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.Scaling = DXGI_SCALING_STRETCH;
        IDXGISwapChain1* chain = nullptr;
        ok = Succeeded(factory->CreateSwapChainForHwnd(queue, window, &desc, nullptr, nullptr, &chain), "CreateSwapChainForHwnd") &&
             Succeeded(chain->QueryInterface(IID_PPV_ARGS(&swap)), "QueryInterface swap chain");
        Release(chain);
        if (ok) factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
    }
    factory->Release();
    return ok && Targets();
}

bool Window::State::Check(HRESULT result, const char* operation) {
    if (Succeeded(result, operation)) return true;
    failed = closing = true;
    return false;
}

bool Window::State::Targets() {
    for (UINT i = 0; i < kFrames; ++i) {
        if (!Check(swap->GetBuffer(i, IID_PPV_ARGS(&targets[i])), "GetBuffer")) return false;
        device->CreateRenderTargetView(targets[i], nullptr, rtv[i]);
    }
    return true;
}

bool Window::State::Wait() {
    if (!queue || !fence || !fence_event) return true;
    const UINT64 value = ++last_fence;
    if (!Check(queue->Signal(fence, value), "Signal idle fence")) return false;
    if (fence->GetCompletedValue() < value) {
        if (!Check(fence->SetEventOnCompletion(value, fence_event), "SetEventOnCompletion idle")) return false;
        if (!fence_event || WaitForSingleObject(fence_event, 10000) != WAIT_OBJECT_0) {
            LOGE("Launcher: GPU idle wait failed (device HRESULT %08lX)", device->GetDeviceRemovedReason());
            failed = closing = true;
            return false;
        }
    }
    for (auto& v : fence_values) v = 0;
    return true;
}

void Window::State::ReleaseTargets() {
    Wait();
    for (auto& target : targets) Release(target);
}

void Window::State::Render() {
    if (failed) return;
    if (resized) {
        resized = false;
        ReleaseTargets();
        if (failed || !Check(swap->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0), "ResizeBuffers") || !Targets()) return;
    }
    if (occluded && swap->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) return;
    const UINT index = swap->GetCurrentBackBufferIndex();
    if (fence_values[index] && fence->GetCompletedValue() < fence_values[index]) {
        if (!Check(fence->SetEventOnCompletion(fence_values[index], fence_event), "SetEventOnCompletion frame")) return;
        if (WaitForSingleObject(fence_event, 10000) != WAIT_OBJECT_0) {
            LOGE("Launcher: GPU frame wait failed (device HRESULT %08lX)", device->GetDeviceRemovedReason());
            failed = closing = true;
            return;
        }
    }
    if (!Check(allocators[index]->Reset(), "Reset command allocator") ||
        !Check(list->Reset(allocators[index], nullptr), "Reset command list")) return;
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = targets[index];
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    list->ResourceBarrier(1, &barrier);
    // Matches the launcher background, so a resize never flashes another color.
    const float clear[4] = {0x25 / 255.f, 0x28 / 255.f, 0x2C / 255.f, 1.f};
    list->ClearRenderTargetView(rtv[index], clear, 0, nullptr);
    list->OMSetRenderTargets(1, &rtv[index], FALSE, nullptr);
    list->SetDescriptorHeaps(1, &srv_heap);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), list);
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    list->ResourceBarrier(1, &barrier);
    if (!Check(list->Close(), "Close frame command list")) return;
    ID3D12CommandList* lists[] = {list};
    queue->ExecuteCommandLists(1, lists);
    const auto presented = swap->Present(1, 0);
    if (!Check(presented, "Present")) return;
    occluded = presented == DXGI_STATUS_OCCLUDED;
    fence_values[index] = ++last_fence;
    Check(queue->Signal(fence, last_fence), "Signal frame fence");
}

Window::Window() : s_(new State) {}
Window::~Window() { Close(); delete s_; }
float Window::Scale() const { return s_->scale; }

bool Window::Open(const char* title, int width, int height, const std::function<void(float)>& prepare) try {
    State& s = *s_;
    s.failed = s.closing = false;
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{sizeof(wc)};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = Procedure;
    wc.hInstance = instance;
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClass;
    RegisterClassExW(&wc);
    POINT cursor{};
    GetCursorPos(&cursor);
    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY);
    UINT dpi = 96;
    if (HMODULE shcore = LoadLibraryW(L"shcore.dll")) {
        using GetDpi = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
        if (auto get = reinterpret_cast<GetDpi>(GetProcAddress(shcore, "GetDpiForMonitor"))) { UINT y = 96; get(monitor, 0, &dpi, &y); }
        FreeLibrary(shcore);
    }
    s.scale = std::max(1.f, dpi / 96.f);
    MONITORINFO info{sizeof(info)};
    GetMonitorInfoW(monitor, &info);
    const RECT work = info.rcWork;
    RECT frame{0, 0, LONG(width * s.scale), LONG(height * s.scale)};
    AdjustWindowRectExForDpi(&frame, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi);
    const int w = std::min<int>(frame.right - frame.left, work.right - work.left);
    const int h = std::min<int>(frame.bottom - frame.top, work.bottom - work.top);
    const int wide = MultiByteToWideChar(CP_UTF8, 0, title, -1, nullptr, 0);
    std::wstring caption(size_t(std::max(wide, 1)), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, title, -1, caption.data(), wide);
    s.window = CreateWindowExW(0, kClass, caption.c_str(), WS_OVERLAPPEDWINDOW, work.left + (work.right - work.left - w) / 2,
                               work.top + (work.bottom - work.top - h) / 2, w, h, nullptr, nullptr, instance, nullptr);
    if (!s.window) { LOGI("launcher: no window (%lu)", GetLastError()); return false; }
    SetWindowLongPtrW(s.window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&s));
    // A dark title bar in the launcher's colors. Older Windows versions ignore these attributes.
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(s.window, 20, &dark, sizeof(dark));
    const COLORREF caption_color = RGB(0x25, 0x28, 0x2C), text_color = RGB(0xEC, 0xE5, 0xD6);
    DwmSetWindowAttribute(s.window, 35, &caption_color, sizeof(caption_color));
    DwmSetWindowAttribute(s.window, 36, &text_color, sizeof(text_color));
    if (!s.Device()) { LOGI("launcher: Direct3D 12 is unavailable for the launcher"); Close(); return false; }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    s.imgui = true;
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    if (prepare) prepare(s.scale);
    if (!ImGui_ImplWin32_Init(s.window)) { Close(); return false; }
    s.win32 = true;
    if (!ImGui_ImplDX12_Init(s.device, int(kFrames), DXGI_FORMAT_R8G8B8A8_UNORM, s.srv_heap,
                             s.srv_heap->GetCPUDescriptorHandleForHeapStart(), s.srv_heap->GetGPUDescriptorHandleForHeapStart())) {
        Close(); return false;
    }
    s.dx12 = true;
    // The backend normally delays fonts and pipelines until NewFrame, after the
    // window is visible. Validate them now so the native-selector fallback works.
    if (!ImGui_ImplDX12_CreateDeviceObjects()) {
        LOGE("Launcher: renderer resources could not be initialized (device HRESULT %08lX)", s.device->GetDeviceRemovedReason());
        Close(); return false;
    }
    LOGR("Launcher: graphics and font resources are ready");
    ShowWindow(s.window, SW_SHOWNORMAL);
    UpdateWindow(s.window);
    SetForegroundWindow(s.window);
    return true;
} catch (const std::exception& error) {
    LOGE("Launcher: initialization failed: %s", error.what());
    Close();
    return false;
}

bool Window::BeginFrame(bool animate) {
    State& s = *s_;
    // Wait for input while idle; animations and progress redraw at display rate.
    MsgWaitForMultipleObjects(0, nullptr, FALSE, animate ? 16 : 250, QS_ALLINPUT);
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    const bool open = !s.closing;
    s.closing = false;
    return open;
}

void Window::EndFrame() {
    ImGui::Render();
    if (!IsIconic(s_->window)) s_->Render();
}

void Window::Close() {
    State& s = *s_;
    if (s.queue) s.Wait();
    if (s.dx12) ImGui_ImplDX12_Shutdown();
    if (s.win32) ImGui_ImplWin32_Shutdown();
    if (s.imgui) ImGui::DestroyContext();
    s.dx12 = s.win32 = s.imgui = false;
    for (auto& target : s.targets) Release(target);
    Release(s.swap);
    for (auto& allocator : s.allocators) Release(allocator);
    Release(s.list);
    Release(s.fence);
    Release(s.rtv_heap);
    Release(s.srv_heap);
    Release(s.queue);
    Release(s.device);
    if (s.fence_event) { CloseHandle(s.fence_event); s.fence_event = nullptr; }
    if (s.window) { SetWindowLongPtrW(s.window, GWLP_USERDATA, 0); DestroyWindow(s.window); s.window = nullptr; }
    UnregisterClassW(kClass, GetModuleHandleW(nullptr));
}
}
