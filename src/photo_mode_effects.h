#pragma once
#include "photo_mode_image.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace openshao::photo_mode {
enum class Filter : int { None, BlackWhite, Sepia, Vintage, Contrast, Count };
enum class Vignette : int { Off, Light, Strong, Count };
enum class Quality : int { Screen, High, Max, Count };
// How far the camera may go from the fighters: normal, large, or anywhere.
enum class Reach : int { Normal, Large, Free, Count };
enum class Row : unsigned { Filter, Amount, Vignette, Tilt, Quality, Reach, Reset, Count };

// Shared canvas geometry for the native labels and both photo-preview backdrops.
struct PanelLayout {
    static constexpr float Left = 28, Top = 50, Right = 316;
    static constexpr float TitleY = 60, RowsY = 94, RowStep = 24;
    static constexpr float LabelX = 42, ValueX = 196;
    static constexpr float BottomPadding = 32;
    static constexpr float Bottom = RowsY + (unsigned(Row::Count) - 1) * RowStep + BottomPadding;
};

// The options panel's choices; they last until the game closes.
struct Look {
    Filter filter = Filter::None;
    int amount = 10;  // tenths of the filter mixed over the picture
    Vignette vignette = Vignette::Off;
    int tilt = 0;  // steps of TiltStep degrees
    Quality quality = Quality::Screen;
    Reach reach = Reach::Normal;
    static constexpr int MaxAmount = 10, MaxTilt = 9, TiltStep = 5;
    bool operator==(const Look&) const = default;
    float Degrees() const { return float(tilt * TiltStep); }
    float Radians() const { return Degrees() * 3.14159265f / 180; }
    // True when the picture needs a colour pass; the tilt turns the game camera itself.
    bool Changes() const { return (filter != Filter::None && amount > 0) || vignette != Vignette::Off; }
};

template <class E> E Step(E value, int direction) {
    const int count = int(E::Count), next = (int(value) + direction % count + count) % count;
    return E(next);
}
// Left and right on a row; the reset row puts everything back. False when nothing changed.
inline bool Adjust(Look& look, Row row, int direction) {
    const Look before = look;
    switch (row) {
    case Row::Filter: look.filter = Step(look.filter, direction); break;
    case Row::Amount: look.amount = std::clamp(look.amount + direction, 0, Look::MaxAmount); break;
    case Row::Vignette: look.vignette = Step(look.vignette, direction); break;
    case Row::Tilt: look.tilt = std::clamp(look.tilt + direction, -Look::MaxTilt, Look::MaxTilt); break;
    case Row::Quality: look.quality = Step(look.quality, direction); break;
    case Row::Reach: look.reach = Step(look.reach, direction); break;
    case Row::Reset: look = {}; break;
    case Row::Count: break;
    }
    return !(look == before);
}
inline std::string_view RowCaption(Row row) {
    constexpr std::array<std::string_view, size_t(Row::Count)> ids{
        "photo.filter", "photo.amount", "photo.vignette", "photo.tilt", "photo.quality", "photo.reach", "photo.reset"};
    return row < Row::Count ? ids[size_t(row)] : std::string_view{};
}
// The value shown on a row: a catalog id, or the literal text when number is set.
struct Value { std::string text; bool number = false; };
inline Value RowValue(const Look& look, Row row) {
    constexpr std::array<std::string_view, size_t(Filter::Count)> filters{
        "photo.filter_none", "photo.filter_bw", "photo.filter_sepia", "photo.filter_vintage", "photo.filter_contrast"};
    constexpr std::array<std::string_view, size_t(Vignette::Count)> vignettes{
        "photo.vignette_off", "photo.vignette_light", "photo.vignette_strong"};
    constexpr std::array<std::string_view, size_t(Quality::Count)> qualities{
        "photo.quality_screen", "photo.quality_high", "photo.quality_max"};
    constexpr std::array<std::string_view, size_t(Reach::Count)> reaches{"photo.reach_normal", "photo.reach_large", "photo.reach_free"};
    switch (row) {
    case Row::Filter: return {std::string(filters[size_t(look.filter)])};
    case Row::Amount: return {std::to_string(look.amount * 10) + "%", true};
    case Row::Vignette: return {std::string(vignettes[size_t(look.vignette)])};
    case Row::Tilt: return {std::to_string(int(look.Degrees())), true};
    case Row::Quality: return {std::string(qualities[size_t(look.quality)])};
    case Row::Reach: return {std::string(reaches[size_t(look.reach)])};
    default: return {};
    }
}

// Rolls a row-major view matrix (rows hold the axis columns, then the translation) around its viewing axis.
inline void RollView(float m[16], float radians) {
    const float c = std::cos(radians), s = std::sin(radians);
    for (int r = 0; r < 4; ++r) {
        const float x = m[r * 4], y = m[r * 4 + 1];
        m[r * 4] = x * c - y * s;
        m[r * 4 + 1] = x * s + y * c;
    }
}

// Shader and CPU parameters: a colour matrix with offsets, the mix and the vignette.
struct Params {
    float rows[3][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}};
    float amount = 0, vignette = 0, aspect = 4.f / 3;
};
inline Params MakeParams(const Look& look, float aspect) {
    Params p;
    p.aspect = std::isfinite(aspect) && aspect > 0 ? aspect : 4.f / 3;
    const float luma[3] = {.299f, .587f, .114f};
    auto set = [&](const float m[3][4]) { for (int r = 0; r < 3; ++r) for (int c = 0; c < 4; ++c) p.rows[r][c] = m[r][c]; };
    switch (look.filter) {
    case Filter::BlackWhite: {
        const float m[3][4] = {{luma[0], luma[1], luma[2], 0}, {luma[0], luma[1], luma[2], 0}, {luma[0], luma[1], luma[2], 0}};
        set(m);
        break;
    }
    case Filter::Sepia: {
        const float m[3][4] = {{.393f, .769f, .189f, 0}, {.349f, .686f, .168f, 0}, {.272f, .534f, .131f, 0}};
        set(m);
        break;
    }
    case Filter::Vintage: {
        // Partly washed out, warm, with lifted blacks.
        const float keep = .6f, tint[3] = {1.08f, 1.f, .82f}, lift[3] = {.05f, .04f, .06f};
        float m[3][4];
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) m[r][c] = ((r == c ? keep : 0) + (1 - keep) * luma[c]) * tint[r] * .92f;
            m[r][3] = lift[r];
        }
        set(m);
        break;
    }
    case Filter::Contrast: {
        // Contrast 1.35 around mid grey with saturation 1.2.
        const float k = 1.35f, s = 1.2f;
        float m[3][4];
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) m[r][c] = k * ((r == c ? s : 0) + (1 - s) * luma[c]);
            m[r][3] = .5f * (1 - k);
        }
        set(m);
        break;
    }
    default: break;
    }
    p.amount = look.filter == Filter::None ? 0 : look.amount / float(Look::MaxAmount);
    p.vignette = look.vignette == Vignette::Light ? .35f : look.vignette == Vignette::Strong ? .65f : 0;
    return p;
}
inline float Smooth(float edge0, float edge1, float x) {
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.f, 1.f);
    return t * t * (3 - 2 * t);
}
// The vignette's darkening at a position, 0 in the middle.
inline float VignetteAt(const Params& p, float u, float v) {
    const float x = (u - .5f) * p.aspect, y = v - .5f;
    const float d = std::sqrt(x * x + y * y) / std::sqrt(.25f * p.aspect * p.aspect + .25f);
    return p.vignette * Smooth(.45f, 1.f, d);
}
inline void Grade(const Params& p, const float in[3], float out[3]) {
    for (int r = 0; r < 3; ++r) {
        const float f = std::clamp(p.rows[r][0] * in[0] + p.rows[r][1] * in[1] + p.rows[r][2] * in[2] + p.rows[r][3], 0.f, 1.f);
        out[r] = in[r] + (f - in[r]) * p.amount;
    }
}

// The same effects as the on-screen preview, on a saved photo.
inline Photo ApplyEffects(const Photo& photo, const Params& p) {
    if (!photo.width || !photo.height) return photo;
    Photo out{photo.width, photo.height, std::vector<uint8_t>(photo.rgb.size())};
    for (uint32_t y = 0; y < photo.height; ++y) {
        for (uint32_t x = 0; x < photo.width; ++x) {
            const float u = (x + .5f) / photo.width, v = (y + .5f) / photo.height;
            float c[3];
            for (int ch = 0; ch < 3; ++ch) c[ch] = photo.rgb[(size_t(y) * photo.width + x) * 3 + ch] / 255.f;
            float graded[3];
            Grade(p, c, graded);
            const float dark = 1 - VignetteAt(p, u, v);
            for (int ch = 0; ch < 3; ++ch)
                out.rgb[(size_t(y) * photo.width + x) * 3 + ch] = uint8_t(std::lround(std::clamp(graded[ch] * dark, 0.f, 1.f) * 255));
        }
    }
    return out;
}

// Area-averaged copy divided by factor on both sides (a supersampled shot back to its normal size).
inline Photo Downscale(const Photo& photo, float factor) {
    if (!(factor > 1.01f) || !photo.width || !photo.height) return photo;
    const uint32_t width = std::max(1u, uint32_t(std::lround(photo.width / factor)));
    Photo resized = Thumbnail(photo, width);
    return resized;
}

// Preview pass for the presented frame: the colour effects, and a backdrop behind the options panel that keeps its light text.
inline constexpr const char* PreviewHlsl = R"(
cbuffer B : register(b1) { float4 uvrect; float4 opts; float4 r0; float4 r1; float4 r2; float4 fx; float4 panel; float4 dark; };
Texture2D img : register(t0);
Texture2D lut : register(t1);
SamplerState ls : register(s0);
SamplerState ps : register(s1);
struct O { float4 p : SV_Position; float2 uv : TEXCOORD0; };
O vs(uint id : SV_VertexID) {
    O o;
    float2 t = float2((id << 1) & 2, id & 2);
    o.p = float4(t * float2(2, -2) + float2(-1, 1), 0, 1);
    o.uv = uvrect.xy + t * uvrect.zw;
    return o;
}
bool Inside(float2 uv, float4 r) { return uv.x >= r.x && uv.y >= r.y && uv.x <= r.z && uv.y <= r.w; }
float4 psmain(O i) : SV_Target {
    float2 uv = i.uv;
    float4 c = img.SampleLevel(ls, uv, 0);
    c.rgb *= opts.w;
    if (opts.x > 0) c.rgb = float3(lut.Sample(ps, float2(c.r, 0.5)).r, lut.Sample(ps, float2(c.g, 0.5)).g, lut.Sample(ps, float2(c.b, 0.5)).b);
    float3 g = saturate(float3(dot(r0.xyz, c.rgb) + r0.w, dot(r1.xyz, c.rgb) + r1.w, dot(r2.xyz, c.rgb) + r2.w));
    c.rgb = lerp(c.rgb, g, fx.x);
    float2 q = (uv - 0.5) * float2(fx.z, 1);
    float d = length(q) / sqrt(0.25 * fx.z * fx.z + 0.25);
    c.rgb *= 1 - fx.y * smoothstep(0.45, 1.0, d);
    if (Inside(uv, panel)) {
        float l = max(c.r, max(c.g, c.b));
        c.rgb *= lerp(dark.x, 1.0, smoothstep(0.55, 0.85, l));
    }
    return float4(c.rgb, 1);
}
)";

// Scene pass over the 3D before the controls: p0 filter, intensity, vignette, aspect; p1 colour range, panel and vertical bounds.
inline constexpr const char* SceneHlsl = R"(
cbuffer B : register(b1) { float4 p0; float4 p1; };
Texture2D img : register(t0);
struct O { float4 p : SV_Position; float2 uv : TEXCOORD0; };
O vs(uint id : SV_VertexID) {
    O o;
    float2 t = float2((id << 1) & 2, id & 2);
    o.p = float4(t * float2(2, -2) + float2(-1, 1), 0, 1);
    o.uv = t;
    return o;
}
float3 Grade(float3 x, int f) {
    float l = dot(x, float3(0.299, 0.587, 0.114));
    if (f == 1) return float3(l, l, l);
    if (f == 2) return float3(dot(x, float3(0.393, 0.769, 0.189)), dot(x, float3(0.349, 0.686, 0.168)), dot(x, float3(0.272, 0.534, 0.131)));
    if (f == 3) return lerp(float3(l, l, l), x, 0.6) * float3(1.08, 1.0, 0.82) * 0.92 + float3(0.05, 0.04, 0.06);
    if (f == 4) return 1.35 * lerp(float3(l, l, l), x, 1.2) + 0.5 * (1.0 - 1.35);
    return x;
}
float4 psmain(O i) : SV_Target {
    float4 c = img.Load(int3(i.p.xy, 0));
    float range = max(p1.x, 0.0001);
    float3 x = c.rgb * range;
    x = lerp(x, saturate(Grade(x, (int)p0.x)), p0.y);
    float2 q = (i.uv - 0.5) * float2(p0.w, 1);
    float d = length(q) / sqrt(0.25 * p0.w * p0.w + 0.25);
    x *= 1 - p0.z * smoothstep(0.45, 1.0, d);
    if (p1.y > 0) {
        float s = (4.0 / 3.0) / p0.w;
        float2 a = float2(0.5 + (28.0 / 640.0 - 0.5) * s, p1.z);
        float2 b = float2(0.5 + (316.0 / 640.0 - 0.5) * s, p1.w);
        if (all(i.uv >= a) && all(i.uv <= b)) x *= 0.45;
    }
    return float4(x / range, c.a);
}
)";

// The preview pass's constants after uvrect and the blit options, in shader order.
struct PreviewParams {
    float r[3][4];
    float fx[4], panel[4], dark[4];
};
// Canvas rectangle (640x480, menus centred in 4:3) as 0..1 coordinates of a frame shown at aspect.
inline std::array<float, 4> CanvasRect(float x0, float y0, float x1, float y1, float aspect) {
    const float squeeze = (4.f / 3) / (std::isfinite(aspect) && aspect > 0 ? aspect : 4.f / 3);
    return {.5f + (x0 / 640 - .5f) * squeeze, y0 / 480, .5f + (x1 / 640 - .5f) * squeeze, y1 / 480};
}
// The panel backdrop darkens the scene behind it; light pixels (the panel's text) keep their brightness.
inline PreviewParams MakePreview(const Params& p, std::array<float, 4> panel, float panel_dark) {
    PreviewParams out{};
    for (int r = 0; r < 3; ++r) for (int c = 0; c < 4; ++c) out.r[r][c] = p.rows[r][c];
    out.fx[0] = p.amount; out.fx[1] = p.vignette; out.fx[2] = p.aspect;
    for (int i = 0; i < 4; ++i) out.panel[i] = panel[i];
    out.dark[0] = panel_dark;
    return out;
}
// No rectangle: nothing is inside it.
inline constexpr std::array<float, 4> NoRect{2, 2, -1, -1};
}
