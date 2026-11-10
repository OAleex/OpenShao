#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <imgui.h>

// Coordinates use logical pixels. Canvas applies the display scale.
namespace openshao::launcher::art {
namespace color {
inline constexpr uint32_t Ink = 0x25282C, Slate = 0x30343A, Line = 0x3B4047, Mist = 0x959AA0, Dim = 0x6C7278,
                          Paper = 0xECE5D6, Ember = 0xE0705A, Moss = 0x93B79E, Warning = 0xE8A15A;
}
ImU32 Rgb(uint32_t rgb, float alpha = 1.f);

enum class Weight { Regular, Medium, SemiBold };
// Adds the embedded typeface to the current ImGui atlas and applies the launcher style.
void Prepare(float scale);

struct Canvas {
    ImDrawList* list = nullptr;
    float scale = 1.f;
    ImVec2 origin{};
    ImVec2 P(float x, float y) const { return {origin.x + x * scale, origin.y + y * scale}; }
    float S(float v) const { return v * scale; }
    ImVec2 Measure(Weight weight, float size, const char* text, float wrap = 0.f) const;
    void Text(Weight weight, float size, float x, float y, ImU32 color, const char* text, float wrap = 0.f) const;
    void TextCentered(Weight weight, float size, float cx, float y, ImU32 color, const char* text) const;
    void TextRight(Weight weight, float size, float right, float y, ImU32 color, const char* text) const;
    void Rect(float x0, float y0, float x1, float y1, ImU32 color, float rounding = 0.f) const;
    void Outline(float x0, float y0, float x1, float y1, ImU32 color, float rounding, float thickness = 1.2f) const;
    void Rule(float x0, float x1, float y) const;
    // Shortens text in the middle, as for long paths, so it fits the width.
    std::string Fit(Weight weight, float size, float width, const std::string& text) const;
};

void Background(const Canvas& canvas, float width, float height, ImVec2 light);
// drawn limits the visible stroke. With flame disabled, the stroke uses rgb.
void Enso(const Canvas& canvas, ImVec2 center, float radius, float width, float drawn, float alpha = 1.f,
          uint32_t seed = 11, int bristles = 44, bool flame = true, uint32_t rgb = color::Paper);
void Glow(const Canvas& canvas, ImVec2 center, float radius, float strength);
void BrushDash(const Canvas& canvas, float x, float y, float length);
// Returns false when the language has no flag.
bool LanguageFlag(const Canvas& canvas, ImVec2 center, float width, std::string_view language, float alpha = 1.f);
void Toggle(const Canvas& canvas, float x, float y, bool on, bool hovered);
void ProgressBar(const Canvas& canvas, float x0, float x1, float y, float value);
}
