#include <cmath>
#include <cstdio>
#include <imgui.h>
#include <imgui_internal.h>
#include "performance_overlay.h"

int main() {
    using namespace openshao::performance;
    int failures = 0;
    auto check = [&](bool ok, const char* message) { if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", message); } };
    check(!Visible(), "overlay must start hidden");
    check(!Message(0x100, 'W', 0) && !Visible(), "game keys must pass through without toggling the overlay");
    check(Message(0x100, 0x78, 0) && Visible(), "F9 down must enable the overlay");
    check(Message(0x100, 0x78, intptr_t(1) << 30) && Visible(), "holding F9 must not toggle on key repeat");
    check(Message(0x101, 0x78, 0) && Visible(), "releasing F9 must keep the overlay enabled");
    check(Format(60) == "60.0 FPS  |  16.67 ms", "60 presents per second must display a 16.67 ms average frame interval");
    check(Format(30) == "30.0 FPS  |  33.33 ms", "30 presents per second must display a 33.33 ms average frame interval");
    check(Format(0) == "-- FPS  |  -- ms" && Format(NAN) == "-- FPS  |  -- ms", "missing samples must not display invalid timing");
    ImGui::CreateContext();
    auto& io = ImGui::GetIO(); io.IniFilename = nullptr; io.DisplaySize = {960,720}; io.DeltaTime = 1.f/60;
    unsigned char* pixels; int width, height; io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    ImGui::NewFrame(); Draw(60); ImGui::Render();
    // ImGui measures a new auto-sized window during its first frame.
    ImGui::NewFrame(); Draw(60); ImGui::Render();
    check(ImGui::GetDrawData()->TotalVtxCount > 0, "enabled overlay must draw through the renderer UI callback");
    const auto* overlay=ImGui::FindWindowByName("##openshao-performance");
    check(overlay && overlay->Size.y>=64 && overlay->Size.x>=280,"The performance panel must leave room for a larger two-line version and timing display");
    std::string logged;
    // Capture rendered text in memory; never access the desktop clipboard.
    io.ClipboardUserData=&logged;
    io.SetClipboardTextFn=[](void* data,const char* text){*static_cast<std::string*>(data)=text;};
    ImGui::NewFrame();ImGui::LogToClipboard();Draw(60);ImGui::LogFinish();ImGui::Render();
    check(logged.find("OpenShao 0.4.1")!=std::string::npos && logged.find("60.0 FPS")!=std::string::npos,
        "The visible performance panel must identify the port version alongside the measured timing");
    check(!io.WantCaptureKeyboard && !io.WantCaptureMouse, "performance display must not consume gameplay input");
    check(Message(0x100, 0x78, 0) && !Visible(), "second F9 press must hide the overlay");
    ImGui::NewFrame(); Draw(60); ImGui::Render();
    check(ImGui::GetDrawData()->TotalVtxCount == 0, "hidden overlay must produce no draw calls");
    ImGui::DestroyContext();
    return failures ? 1 : 0;
}
