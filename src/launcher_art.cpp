#include "launcher_art.h"
#include <algorithm>
#include <cmath>
#include "launcher_fonts.h"

namespace openshao::launcher::art {
namespace {
constexpr float kPi = 3.14159265358979f;
constexpr float kSizes[] = {18.5f, 19, 20, 21, 22, 36, 42, 54};
constexpr int kSizeCount = int(sizeof(kSizes) / sizeof(kSizes[0]));
ImFont* fonts[3][kSizeCount] = {};
float font_scale = 1.f;

struct Rng {
    uint32_t s;
    float f() { s = s * 1664525u + 1013904223u; return float(s >> 8) / 16777216.f; }
};
float Smooth(float a, float b, float x) {
    const float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
    return t * t * (3 - 2 * t);
}
uint32_t Mix(uint32_t a, uint32_t b, float t) {
    auto channel = [&](int shift) {
        return uint32_t(float((a >> shift) & 255) * (1 - t) + float((b >> shift) & 255) * t) << shift;
    };
    return channel(16) | channel(8) | channel(0);
}
// Soft red body that warms toward coral and amber along the stroke and at the bristle edges.
uint32_t Flame(float t, float edge) {
    constexpr uint32_t stops[] = {0xA5313A, 0xCC4844, 0xE8704F, 0xF7AC60};
    const float m = std::clamp(0.02f + 0.62f * t + 0.48f * edge * edge, 0.f, 0.999f) * 3.f;
    const int i = int(m);
    return Mix(stops[i], stops[i + 1], m - float(i));
}
ImFont* Font(Weight weight, float size, float& pixels) {
    int index = kSizeCount - 1;
    for (int i = 0; i < kSizeCount; ++i) if (kSizes[i] >= size - 0.01f) { index = i; break; }
    pixels = size * font_scale;
    ImFont* font = fonts[int(weight)][index];
    return font ? font : ImGui::GetFont();
}
}

ImU32 Rgb(uint32_t rgb, float alpha) {
    return IM_COL32((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, int(std::clamp(alpha, 0.f, 1.f) * 255.f + 0.5f));
}

void Prepare(float scale) {
    font_scale = scale;
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    const unsigned char* data[] = {fonts::kRegular, fonts::kMedium, fonts::kSemiBold};
    const size_t sizes[] = {fonts::kRegularSize, fonts::kMediumSize, fonts::kSemiBoldSize};
    for (int w = 0; w < 3; ++w)
        for (int i = 0; i < kSizeCount; ++i) {
            ImFontConfig config;
            // The data is static; the atlas must not free it.
            config.FontDataOwnedByAtlas = false;
            config.OversampleH = 2;
            config.PixelSnapH = false;
            fonts[w][i] = io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(data[w]), int(sizes[w]), kSizes[i] * scale,
                                                         &config, io.Fonts->GetGlyphRangesDefault());
        }
    io.FontDefault = fonts[0][2];
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(0, 0);
    style.WindowBorderSize = 0;
    style.WindowRounding = 0;
    style.FrameRounding = 8;
    style.ScrollbarSize = 8;
    style.ScrollbarRounding = 4;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImGui::ColorConvertU32ToFloat4(Rgb(color::Line));
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImGui::ColorConvertU32ToFloat4(Rgb(color::Dim));
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImGui::ColorConvertU32ToFloat4(Rgb(color::Mist));
    style.Colors[ImGuiCol_NavHighlight] = ImGui::ColorConvertU32ToFloat4(Rgb(color::Ember));
    style.ScaleAllSizes(scale);
}

ImVec2 Canvas::Measure(Weight weight, float size, const char* text, float wrap) const {
    float pixels = 0;
    ImFont* font = Font(weight, size, pixels);
    const ImVec2 measured = font->CalcTextSizeA(pixels, FLT_MAX, wrap > 0 ? wrap * scale : 0.f, text);
    return {measured.x / scale, measured.y / scale};
}
void Canvas::Text(Weight weight, float size, float x, float y, ImU32 color, const char* text, float wrap) const {
    float pixels = 0;
    ImFont* font = Font(weight, size, pixels);
    list->AddText(font, pixels, P(x, y), color, text, nullptr, wrap > 0 ? wrap * scale : 0.f);
}
void Canvas::TextCentered(Weight weight, float size, float cx, float y, ImU32 color, const char* text) const {
    Text(weight, size, cx - Measure(weight, size, text).x / 2, y, color, text);
}
void Canvas::TextRight(Weight weight, float size, float right, float y, ImU32 color, const char* text) const {
    Text(weight, size, right - Measure(weight, size, text).x, y, color, text);
}
void Canvas::Rect(float x0, float y0, float x1, float y1, ImU32 color, float rounding) const {
    list->AddRectFilled(P(x0, y0), P(x1, y1), color, S(rounding));
}
void Canvas::Outline(float x0, float y0, float x1, float y1, ImU32 color, float rounding, float thickness) const {
    list->AddRect(P(x0, y0), P(x1, y1), color, S(rounding), 0, S(thickness));
}
void Canvas::Rule(float x0, float x1, float y) const {
    // A filled rectangle keeps hairlines crisp on every renderer.
    list->AddRectFilled(P(x0, y), ImVec2(origin.x + x1 * scale, origin.y + y * scale + std::max(1.f, scale)), Rgb(color::Line));
}
std::string Canvas::Fit(Weight weight, float size, float width, const std::string& text) const {
    if (Measure(weight, size, text.c_str()).x <= width) return text;
    // Keep more of the end, which names the file or folder.
    for (size_t keep = text.size(); keep > 4; --keep) {
        const size_t head = keep / 3, tail = keep - head;
        std::string candidate = text.substr(0, head) + "..." + text.substr(text.size() - tail);
        if (Measure(weight, size, candidate.c_str()).x <= width) return candidate;
    }
    return "...";
}

void Background(const Canvas& c, float width, float height, ImVec2 light) {
    c.Rect(0, 0, width, height, Rgb(color::Ink));
    for (int i = 18; i >= 1; --i) c.list->AddCircleFilled(c.P(light.x, light.y), c.S(430.f * float(i) / 18.f), Rgb(0x3A3F45, 0.05f), 96);
    Rng r{99};
    for (int i = 0; i < 2600; ++i) {
        const ImVec2 p = c.P(r.f() * width, r.f() * height);
        const float d = c.S(0.75f);
        c.list->AddRectFilled(p, ImVec2(p.x + d, p.y + d), r.f() < 0.5f ? IM_COL32(255, 250, 240, 9) : IM_COL32(0, 0, 0, 14));
    }
}

void Glow(const Canvas& c, ImVec2 center, float radius, float strength) {
    for (int i = 18; i >= 1; --i)
        c.list->AddCircleFilled(c.P(center.x, center.y), c.S(radius * (0.75f + 0.95f * float(i) / 18.f)), Rgb(0x8A3E33, 0.024f * strength), 128);
}

void Enso(const Canvas& c, ImVec2 center, float R, float width, float drawn, float alpha, uint32_t seed, int bristles, bool flame, uint32_t rgb) {
    // Traced clockwise from the upper left, leaving a gap before the start.
    const float start = -112.f * kPi / 180.f, sweep = 336.f * kPi / 180.f;
    const int steps = std::max(24, int(R * 2.2f));
    Rng r{seed};
    ImDrawList* dl = c.list;
    for (int k = 0; k < bristles; ++k) {
        const float o = bristles == 1 ? 0.f : float(k) / float(bristles - 1) - 0.5f;
        const float edge = std::fabs(o) * 2.f;
        const float end = 1.f - r.f() * (0.04f + 0.30f * edge * edge);
        const float begin = r.f() * 0.02f * edge;
        const float shade = 0.70f + 0.30f * r.f();
        const float f1 = 18 + r.f() * 30, f2 = 41 + r.f() * 60, p1 = r.f() * 6.28f, p2 = r.f() * 6.28f;
        const float dryness = 0.25f + 0.75f * edge + (r.f() - 0.5f) * 0.3f;
        const float limit = std::min(end, drawn);
        if (limit <= begin) continue;
        // Each bristle is one ribbon with its own ink amount at every step, so dry patches and the stroke's
        // ends fade out instead of breaking off. A one-pixel transparent fringe keeps the edges smooth.
        const float half = c.S(std::max(1.f, width / float(bristles) * 2.2f)) * 0.5f, fringe = 1.f;
        const int n = int(float(steps) * (limit - begin)) + 2;
        const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
        dl->PrimReserve(n * 18, (n + 1) * 4);
        const auto base = ImDrawIdx(dl->_VtxCurrentIdx);
        for (int i = 0; i <= n; ++i) {
            const float t = begin + (limit - begin) * float(i) / float(n);
            // Dry brush: the edge bristles run out of ink toward the end of the stroke.
            const float noise = 0.5f + 0.32f * std::sin(t * f1 + p1) + 0.18f * std::sin(t * f2 + p2);
            const float threshold = Smooth(0.50f, 1.0f, t) * dryness * 1.15f;
            float ink = Smooth(threshold - 0.09f, threshold + 0.09f, noise);
            ink *= Smooth(begin, begin + 0.012f, t) * (1.f - Smooth(limit - 0.035f, limit, t));
            const float press = (0.66f + 0.40f * std::sin(t * kPi * 0.9f + 0.2f)) * (0.80f + 0.35f * (1 - Smooth(0.f, 0.06f, t))) *
                                (1.f - 0.70f * Smooth(0.70f, 1.f, t));
            const float w = width * press;
            const float rr = R * (1.f + 0.03f * (0.5f - t)) +
                             R * (0.008f * std::sin(t * 9.f + float(seed)) + 0.005f * std::sin(t * 23.f + float(seed) * 0.7f)) + o * w;
            const float a = start + sweep * t;
            const ImVec2 dir{std::cos(a), std::sin(a)};
            const ImVec2 p = c.P(center.x + dir.x * rr, center.y + dir.y * rr);
            const ImU32 full = flame ? Rgb(Flame(t, edge), alpha * (0.82f + 0.18f * shade) * ink) : Rgb(rgb, alpha * shade * ink);
            const ImU32 clear = full & ~IM_COL32_A_MASK;
            const float offsets[4] = {-half - fringe, -half, half, half + fringe};
            for (int v = 0; v < 4; ++v)
                dl->PrimWriteVtx(ImVec2(p.x + dir.x * offsets[v], p.y + dir.y * offsets[v]), uv, v == 0 || v == 3 ? clear : full);
            if (i == n) break;
            const auto at = ImDrawIdx(base + i * 4), next = ImDrawIdx(at + 4);
            for (int q = 0; q < 3; ++q) {
                dl->PrimWriteIdx(ImDrawIdx(at + q)); dl->PrimWriteIdx(ImDrawIdx(next + q)); dl->PrimWriteIdx(ImDrawIdx(next + q + 1));
                dl->PrimWriteIdx(ImDrawIdx(at + q)); dl->PrimWriteIdx(ImDrawIdx(next + q + 1)); dl->PrimWriteIdx(ImDrawIdx(at + q + 1));
            }
        }
    }
}

void BrushDash(const Canvas& c, float x, float y, float length) {
    Rng r{5};
    for (int k = 0; k < 6; ++k) {
        const float row = y + float(k) * 0.55f;
        c.list->AddLine(c.P(x + r.f() * 2, row), c.P(x + length * (0.75f + 0.25f * r.f()), row + 0.4f), Rgb(color::Ember, 0.85f), c.S(1.2f));
    }
}

bool LanguageFlag(const Canvas& c, ImVec2 center, float width, std::string_view language, float alpha) {
    if (language != "pt-BR" && language != "pt-br") return false;
    // Brazil, 10:7.
    const float w = width, h = width * 0.7f, x0 = center.x - w / 2, y0 = center.y - h / 2;
    c.Rect(x0, y0, x0 + w, y0 + h, Rgb(0x009C3B, alpha), 3);
    const float mx = w * 0.085f, my = h * 0.12f;
    c.list->AddQuadFilled(c.P(center.x, y0 + my), c.P(x0 + w - mx, center.y), c.P(center.x, y0 + h - my), c.P(x0 + mx, center.y),
                          Rgb(0xFFDF00, alpha));
    c.list->AddCircleFilled(c.P(center.x, center.y), c.S(w * 0.175f), Rgb(0x002776, alpha), 32);
    return true;
}

void Toggle(const Canvas& c, float x, float y, bool on, bool hovered) {
    c.Rect(x, y, x + 38, y + 22, Rgb(on ? color::Ember : color::Slate, hovered ? 1.f : 0.92f), 11);
    if (!on) c.Outline(x, y, x + 38, y + 22, Rgb(hovered ? color::Dim : color::Line), 11);
    c.list->AddCircleFilled(c.P(on ? x + 27 : x + 11, y + 11), c.S(7.5f), Rgb(on ? color::Ink : color::Mist), 24);
}

void ProgressBar(const Canvas& c, float x0, float x1, float y, float value) {
    c.Rect(x0, y, x1, y + 3, Rgb(color::Slate), 2);
    if (value > 0) c.Rect(x0, y, x0 + (x1 - x0) * std::clamp(value, 0.f, 1.f), y + 3, Rgb(color::Ember), 2);
}
}
