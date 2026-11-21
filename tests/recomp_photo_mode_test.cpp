#include "photo_mode_camera.h"
#include "photo_mode_image.h"
#include "photo_mode_effects.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace openshao::photo_mode;
static void Check(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static bool Near(float a, float b, float tolerance = 0.01f) { return std::abs(a - b) < tolerance; }
static bool Near(Vec3 a, Vec3 b, float tolerance = 0.01f) {
    return Near(a.x, b.x, tolerance) && Near(a.y, b.y, tolerance) && Near(a.z, b.z, tolerance);
}

int main() {
    FreeCamera camera;
    camera.Begin({0, 100, 0}, {0, 100, 50});
    Check(Near(camera.Eye(), {0, 100, 0}) && Near(camera.Target(), {0, 100, FreeCamera::TargetDistance}),
          "opening keeps the game's eye and viewing direction");

    camera.Update({0.1f, -0.15f, 0.19f, 0, 0, 0}, 0.05f);
    Check(Near(camera.Eye(), {0, 100, 0}) && Near(camera.Target(), {0, 100, FreeCamera::TargetDistance}),
          "stick noise inside the dead zone does not move the camera");

    camera.Update({0, 1, 0, 0, 0, 0}, 0.1f);
    Check(Near(camera.Eye(), {0, 100, FreeCamera::Speed * 0.1f}), "left stick up moves along the viewing direction");

    camera.Begin({0, 100, 0}, {0, 100, 50});
    camera.Update({1, 0, 0, 0, 0, 0}, 0.1f);
    Check(Near(camera.Eye().x, FreeCamera::Speed * 0.1f) && Near(camera.Eye().z, 0),
          "left stick right strafes without moving forward");

    camera.Begin({0, 100, 0}, {0, 100, 50});
    camera.Update({0, 0, 0, 0, 0, 1}, 0.1f);
    Check(Near(camera.Eye().y, 100 - FreeCamera::Speed * 0.1f), "right trigger raises the camera (game Y points down)");
    camera.Update({0, 0, 0, 0, 1, 0}, 0.1f);
    Check(Near(camera.Eye().y, 100), "left trigger lowers it again");

    camera.Begin({0, 100, 0}, {0, 100, 50});
    for (int i = 0; i < 1000; ++i) camera.Update({0, 1, 0, 0, 0, 1}, 0.1f);
    const Vec3 far = camera.Eye();
    Check(Near(std::sqrt(far.x * far.x + far.z * far.z), FreeCamera::Range, 0.5f) &&
          Near(far.y, 100 - FreeCamera::Height), "the camera stays within range of where photo mode opened");

    camera.Begin({0, 100, 0}, {0, 100, 50});
    camera.SetArea({0, 100, 1000}, FreeCamera::Range, FreeCamera::Height);
    for (int i = 0; i < 1000; ++i) camera.Update({0, 1, 0, 0, 0, 0}, 0.1f);
    Check(Near(camera.Eye().z, 1000 + FreeCamera::Range, 0.5f), "the area is centred on the fighters, so the camera passes behind them");
    camera.Begin({0, 100, 0}, {0, 100, 50});
    camera.SetArea({0, 100, 0}, 0, 0);
    for (int i = 0; i < 1000; ++i) camera.Update({0, 1, 0, 0, 0, 1}, 0.1f);
    Check(camera.Eye().z > FreeCamera::LargeRange && camera.Eye().y < 100 - FreeCamera::LargeHeight, "no limit lets the camera go anywhere");

    camera.Begin({0, 100, 0}, {0, 100, 50});
    camera.Update({0, 0, 1, 0, 0, 0}, 0.1f);
    const Vec3 turned = camera.Target();
    Check(Near(camera.Eye(), {0, 100, 0}) && turned.x > 0 && turned.z < FreeCamera::TargetDistance,
          "right stick turns the view in place");
    for (int i = 0; i < 1000; ++i) camera.Update({0, 0, 0, 1, 0, 0}, 0.1f);
    const Vec3 look = camera.Target();
    Check(look.y < 100 && 100 - look.y < FreeCamera::TargetDistance * std::sin(FreeCamera::PitchLimit) + 0.01f,
          "right stick up looks up and stops before the view flips over");

    camera.Update({0, 1, 0, 0, 0, 0}, 5.f);
    Check(std::abs(camera.Eye().z) < FreeCamera::Speed * 0.1f + 0.01f, "a long frame gap moves at most one tenth of a second");

    camera.Begin({0, 100, 0}, {0, 100, 50});
    camera.Update({1, 1, 1, 1, 0, 1}, 0.1f);
    camera.Reset();
    Check(Near(camera.Eye(), camera.Origin()), "reset returns to the starting point");
    std::puts("PASS: photo mode free camera movement, turning, range limits and reset");

    camera.Begin({0, 100, 0}, {0, 100, 50});
    Check(Near(camera.Zoom(), 1), "photo mode opens at the game's own field of view");
    camera.Update({0, 0, 0, 0, 0, 0, 1, 0}, 0.1f);
    const float closer = camera.Zoom();
    Check(closer < 1 && Near(camera.Eye(), {0, 100, 0}), "right trigger zooms in without moving the camera");
    camera.Update({0, 0, 0, 0, 0, 0, 0, 1}, 0.1f);
    Check(Near(camera.Zoom(), 1, 0.001f), "left trigger zooms back out at the same rate");
    for (int i = 0; i < 1000; ++i) camera.Update({0, 0, 0, 0, 0, 0, 1, 0}, 0.1f);
    Check(Near(camera.Zoom(), FreeCamera::MinZoom), "zooming in stops at the narrowest view");
    camera.Update({0, 0, 1, 0, 0, 0, 0, 0}, 0.1f);
    Check(std::abs(camera.Yaw()) < FreeCamera::TurnRate * 0.1f * FreeCamera::MinZoom + 0.001f,
          "a zoomed-in view turns slower");
    for (int i = 0; i < 1000; ++i) camera.Update({0, 0, 0, 0, 0, 0, 0, 1}, 0.1f);
    Check(Near(camera.Zoom(), FreeCamera::MaxZoom), "zooming out stops at the widest view");
    camera.Reset();
    Check(Near(camera.Zoom(), 1), "reset also restores the zoom");
    std::puts("PASS: photo mode zoom");

    // LookAt from (10, 20, 30) toward +x: right axis -z, up y, forward x.
    const float view[16] = {0, 0, 1, 0, 0, 1, 0, 0, -1, 0, 0, 0, 30, -20, -10, 1};
    Vec3 eye, ahead;
    Check(FromView(view, eye, ahead) && Near(eye, {10, 20, 30}) && Near(ahead, {110, 20, 30}),
          "the rendered view matrix gives back its eye and viewing direction");
    const float broken[16] = {};
    Check(!FromView(broken, eye, ahead), "an empty view matrix is rejected");

    const std::vector<uint8_t> same{10, 20, 30, 255, 40, 50, 60, 255, 70, 80, 90, 255, 1, 2, 3, 255};
    const auto copy = DisplayPhoto(same, 2, 2, 1.f);
    Check(copy.width == 2 && copy.height == 2 && copy.rgb.size() == 12 && copy.rgb[0] == 30 && copy.rgb[1] == 20 &&
          copy.rgb[2] == 10 && copy.rgb[9] == 3, "a square frame keeps its size and turns BGRA into RGB");
    const std::vector<uint8_t> sides{0, 0, 255, 255, 255, 0, 0, 255};
    const auto wide = DisplayPhoto(sides, 2, 1, 4.f);
    Check(wide.width == 4 && wide.height == 1 && wide.rgb[0] == 255 && wide.rgb[2] == 0 && wide.rgb[9] == 0 &&
          wide.rgb[11] == 255 && wide.rgb[3] > wide.rgb[6], "the frame stretches to the display aspect like the screen");
    Check(DisplayPhoto({1, 2, 3}, 2, 2, 1.f).rgb.empty(), "a short copy gives no photo");
    std::tm time{};
    time.tm_year = 126; time.tm_mon = 10; time.tm_mday = 18; time.tm_hour = 13; time.tm_min = 5; time.tm_sec = 9;
    Check(PhotoName(time, 1) == "OpenShao_2026-11-18_13-05-09.png" && PhotoName(time, 2) == "OpenShao_2026-11-18_13-05-09_2.png",
          "photos are named by date and time, numbered within one second");
    std::puts("PASS: photo mode frame conversion and file names");

    Photo big;
    big.width = 4; big.height = 2; big.rgb.assign(4 * 2 * 3, 0);
    for (unsigned i = 0; i < 2; ++i) { big.rgb[i * 3] = 200; big.rgb[(4 + i) * 3] = 100; }
    const auto thumb = Thumbnail(big, 2);
    Check(thumb.width == 2 && thumb.height == 1 && thumb.rgb[0] == 150 && thumb.rgb[3] == 0,
          "the notification thumbnail averages the photo down to its width");
    Check(Thumbnail(big, 8).width == 4, "a small photo keeps its size as a thumbnail");
    auto short_enough = [](const std::string& text) { return text.size() <= 16; };
    const auto path = EllipsizeMiddle(std::string("C:\\Users\\Alex\\Pictures\\OpenShao\\photo.png"), short_enough);
    Check(path.size() <= 16 && path.find("...") != std::string::npos && path.rfind("C:", 0) == 0 && path.ends_with(".png"),
          "a long path keeps its start and its end around an ellipsis");
    Check(EllipsizeMiddle(std::string("short"), short_enough) == "short", "a path that fits is unchanged");
    const auto accents = EllipsizeMiddle(std::string("Imagens\\Fotografias\\M\xC3\xA1rio\\a.png"), short_enough);
    for (size_t i = 0; i + 1 < accents.size(); ++i)
        Check(!((uint8_t(accents[i]) & 0xE0) == 0xC0 && (uint8_t(accents[i + 1]) & 0xC0) != 0x80), "no broken UTF-8");
    std::puts("PASS: photo notification thumbnail and path ellipsis");

    Look options;
    Check(!options.Changes() && RowValue(options, Row::Amount).text == "100%", "the panel opens on the plain picture at full filter strength");
    Check(Adjust(options, Row::Filter, 1) && options.filter == Filter::BlackWhite && Adjust(options, Row::Filter, -2) &&
          options.filter == Filter::Contrast, "the filter row wraps around both ways");
    Check(Adjust(options, Row::Amount, -1) && options.amount == 9 && RowValue(options, Row::Amount).text == "90%",
          "intensity moves in steps of ten percent");
    for (int i = 0; i < 20; ++i) Adjust(options, Row::Tilt, 1);
    Check(options.tilt == Look::MaxTilt && RowValue(options, Row::Tilt).text == "45" && !Adjust(options, Row::Tilt, 1),
          "tilt stops at 45 degrees");
    Check(Adjust(options, Row::Quality, -1) && options.quality == Quality::Max, "quality wraps from Screen to Max");
    Check(RowValue(options, Row::Reach).text == "photo.reach_normal" && Adjust(options, Row::Reach, 1) && options.reach == Reach::Large &&
          Adjust(options, Row::Reach, 1) && RowValue(options, Row::Reach).text == "photo.reach_free", "range goes from normal to large to no limit");
    Check(Adjust(options, Row::Reset, 1) && options == Look{} && !Adjust(options, Row::Reset, 1), "reset restores every choice once");

    Photo flat{4, 2, std::vector<uint8_t>(4 * 2 * 3)};
    for (size_t i = 0; i < flat.rgb.size(); i += 3) { flat.rgb[i] = 200; flat.rgb[i + 1] = 100; flat.rgb[i + 2] = 50; }
    Check(ApplyEffects(flat, MakeParams(Look{}, 2.f)).rgb == flat.rgb, "no effect leaves the photo untouched");
    Look gray;
    gray.filter = Filter::BlackWhite;
    const auto bw = ApplyEffects(flat, MakeParams(gray, 2.f));
    const int luma = int(std::lround((.299f * 200 + .587f * 100 + .114f * 50)));
    Check(std::abs(bw.rgb[0] - luma) <= 1 && bw.rgb[0] == bw.rgb[1] && bw.rgb[1] == bw.rgb[2], "black and white uses the luma");
    gray.amount = 5;
    const auto half = ApplyEffects(flat, MakeParams(gray, 2.f));
    Check(std::abs(half.rgb[0] - (200 + luma) / 2) <= 1, "half intensity mixes the filter halfway");

    Look framed;
    framed.vignette = Vignette::Strong;
    const auto p = MakeParams(framed, 16.f / 9);
    Check(VignetteAt(p, .5f, .5f) == 0 && VignetteAt(p, 0, 0) > .6f, "the vignette darkens the corners and leaves the middle");

    Look tilted;
    tilted.tilt = 3;
    Check(!tilted.Changes() && Near(tilted.Degrees(), 15) && ApplyEffects(flat, MakeParams(tilted, 2.f)).rgb == flat.rgb,
          "tilt turns the game camera by 15 degrees and leaves the colours alone");
    // LookAt from (10, 20, 30) toward +x, as in the view test above.
    float rolled[16] = {0, 0, 1, 0, 0, 1, 0, 0, -1, 0, 0, 0, 30, -20, -10, 1};
    RollView(rolled, tilted.Radians());
    Vec3 roll_eye, roll_ahead;
    Check(FromView(rolled, roll_eye, roll_ahead) && Near(roll_eye, {10, 20, 30}) && Near(roll_ahead, {110, 20, 30}),
          "rolling the view keeps the eye and the viewing direction");
    Check(Near(rolled[5], std::cos(tilted.Radians())) && Near(rolled[9], -std::sin(tilted.Radians())),
          "rolling the view turns its up axis");

    Photo large{8, 4, std::vector<uint8_t>(8 * 4 * 3, 90)};
    const auto small = Downscale(large, 2.f);
    Check(small.width == 4 && small.height == 2 && small.rgb[0] == 90, "a supersampled photo shrinks back by its factor");
    Check(Downscale(large, 1.f).width == 8, "no supersampling keeps the size");

    const auto rect = CanvasRect(0, 0, 640, 480, 16.f / 9);
    Check(Near(rect[0], .125f) && Near(rect[2], .875f) && Near(rect[3], 1), "menus sit in the 4:3 middle of a wide frame");
    Check(PanelLayout::Bottom == PanelLayout::RowsY + (unsigned(Row::Count) - 1) * PanelLayout::RowStep + PanelLayout::BottomPadding,
          "photo panel height follows every option, including Reset");
    Check(PanelLayout::Bottom > PanelLayout::RowsY + unsigned(Row::Reset) * PanelLayout::RowStep + 20 &&
          PanelLayout::Bottom < 360 && PanelLayout::Top < PanelLayout::TitleY,
          "Reset label fits inside the panel with bottom padding and never overlaps the bottom legend");
    for (const float aspect : {4.f / 3, 16.f / 9, 16.f / 10, 21.f / 9, 32.f / 9, 3.f / 2}) {
        const auto area = CanvasRect(PanelLayout::Left, PanelLayout::Top, PanelLayout::Right, PanelLayout::Bottom, aspect);
        const auto params = MakePreview(MakeParams(Look{}, aspect), area, .4f);
        Check(Near(params.panel[1], PanelLayout::Top / 480.f) && Near(params.panel[3], PanelLayout::Bottom / 480.f) &&
              params.panel[0] > 0 && params.panel[2] < 1 && params.panel[2] > params.panel[0],
              "photo backdrop covers all rows across original, wide, ultra-wide and automatic aspects");
    }
    Check(std::string_view(SceneHlsl).find("p1.w") != std::string_view::npos &&
          std::string_view(SceneHlsl).find("246.0 / 480.0") == std::string_view::npos,
          "D3D scene pass receives the same dynamic panel bottom as the other renderer");
    std::puts("PASS: photo options panel and effects");
}
