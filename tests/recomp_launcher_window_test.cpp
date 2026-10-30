#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <imgui.h>
#include <backends/imgui_impl_dx12.h>
#include <iostream>
static bool RejectRendererResources() { return false; }
// Inject only the backend result; exercise the real native window lifecycle.
#define ImGui_ImplDX12_CreateDeviceObjects RejectRendererResources
#include OPENSHAO_LAUNCHER_WINDOW_SOURCE
#undef ImGui_ImplDX12_CreateDeviceObjects

int main() {
    const auto name = L"OpenShaoLauncherFailure" + std::to_wstring(GetCurrentProcessId());
    const auto previous = GetThreadDesktop(GetCurrentThreadId());
    const auto desktop = CreateDesktopW(name.c_str(), nullptr, nullptr, 0, 0x01FF, nullptr);
    if (!desktop || !SetThreadDesktop(desktop)) return 1;
    bool opened = false;
    {
        openshao::launcher::Window window;
        opened = window.Open("OpenShao", 1040, 640, {});
    }
    SetThreadDesktop(previous);
    CloseDesktop(desktop);
    if (opened) {
        std::cerr << "FAIL: renderer initialization failure must return to the native-selector fallback before showing the launcher\n";
        return 1;
    }
    std::cout << "PASS: failed launcher resources close cleanly and permit native-selector fallback\n";
}
