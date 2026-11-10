#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <imgui.h>
#include <backends/imgui_impl_dx12.h>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include OPENSHAO_LAUNCHER_WINDOW_SOURCE

namespace {
int failures = 0;
void Check(bool ok, const std::string& text) {
    if (ok) return;
    std::cerr << "FAIL: " << text << "\n";
    ++failures;
}

std::vector<uint8_t> Picture(int width, int height, uint8_t red) {
    std::vector<uint8_t> rgba(size_t(width) * height * 4);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            uint8_t* pixel = &rgba[(size_t(y) * width + x) * 4];
            pixel[0] = red;
            pixel[1] = uint8_t(x * 255 / std::max(width - 1, 1));
            pixel[2] = uint8_t(y * 255 / std::max(height - 1, 1));
            pixel[3] = 255;
        }
    return rgba;
}

// Draws the images for a few frames, so the descriptors are read by the real renderer.
void Draw(openshao::launcher::Window& window, const std::vector<uint64_t>& images) {
    for (int frame = 0; frame < 3; ++frame) {
        window.BeginFrame(false);
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({400, 300});
        ImGui::Begin("images", nullptr, ImGuiWindowFlags_NoDecoration);
        for (uint64_t image : images) ImGui::Image(ImTextureID(image), {32, 32});
        ImGui::End();
        window.EndFrame();
    }
}
}

int main() {
    const auto name = L"OpenShaoLauncherImages" + std::to_wstring(GetCurrentProcessId());
    const auto previous = GetThreadDesktop(GetCurrentThreadId());
    const auto desktop = CreateDesktopW(name.c_str(), nullptr, nullptr, 0, 0x01FF, nullptr);
    if (!desktop || !SetThreadDesktop(desktop)) return 1;
    {
        openshao::launcher::Window window;
        if (!window.Open("OpenShao", 1040, 640, {})) {
            std::cerr << "FAIL: the launcher window did not open\n";
            return 1;
        }
        const auto red = Picture(64, 48, 200), blue = Picture(256, 256, 20);

        // An image gets a handle of its own, and the launcher can draw it
        const uint64_t first = window.CreateImage(red.data(), 64, 48);
        const uint64_t second = window.CreateImage(blue.data(), 256, 256);
        Check(first != 0 && second != 0, "an image is made from RGBA pixels");
        Check(first != second, "each image has its own handle");
        Draw(window, {first, second});

        // What cannot be made gives 0, which makes the launcher draw its default mark
        Check(window.CreateImage(nullptr, 64, 48) == 0, "no pixels");
        Check(window.CreateImage(red.data(), 0, 48) == 0, "no width");
        Check(window.CreateImage(red.data(), 64, 0) == 0, "no height");
        Check(window.CreateImage(red.data(), -1, 48) == 0, "a negative width");
        Check(window.CreateImage(red.data(), 100000, 48) == 0, "a width the device cannot hold");
        window.DestroyImage(0);       // a handle that is not an image
        window.DestroyImage(12345);   // one that is not either

        // A destroyed image gives its place back, and the places are finite
        window.DestroyImage(first);
        const uint64_t again = window.CreateImage(red.data(), 64, 48);
        Check(again == first, "the place of a destroyed image is used again");
        std::vector<uint64_t> held{again, second};
        for (int i = 0; i < 1000; ++i) {
            const uint64_t image = window.CreateImage(red.data(), 16, 16);
            if (!image) break;
            held.push_back(image);
        }
        Check(held.size() >= 64 && held.size() <= 1000, "many images fit, and not without limit (" + std::to_string(held.size()) + ")");
        Check(window.CreateImage(red.data(), 16, 16) == 0, "a full heap makes no image");
        Draw(window, held);
        for (uint64_t image : held) window.DestroyImage(image);
        Check(window.CreateImage(red.data(), 16, 16) != 0, "images can be made again after all were destroyed");
    }  // the window closes with an image still alive
    SetThreadDesktop(previous);
    CloseDesktop(desktop);
    if (failures) return 1;
    std::cout << "PASS: the launcher makes, draws, reuses and releases images on Direct3D 12\n";
}
