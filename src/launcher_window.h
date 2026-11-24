#pragma once
#include <cstdint>
#include <functional>

namespace openshao::launcher {
// Native launcher window with Dear ImGui (D3D12 on Windows, SDL/Vulkan on Linux).
class Window {
public:
    Window();
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // `prepare` adds fonts and styles before upload; it receives the display scale.
    bool Open(const char* title, int width, int height, const std::function<void(float scale)>& prepare);
    // False after the close button; `animate` redraws at display rate, otherwise waits for input.
    bool BeginFrame(bool animate);
    void EndFrame();
    void Close();
    float Scale() const;

    // An image for the interface from raw RGBA, top row first, as the handle that ImGui::Image takes (cast to ImTextureID).
    // 0 when the renderer cannot make one, and the caller draws something else.
    uint64_t CreateImage(const uint8_t* rgba, int width, int height);
    void DestroyImage(uint64_t image);

    struct State;
private:
    State* s_;
};
}
