#include "photo_mode.h"
#include "photo_mode_camera.h"
#include "photo_mode_image.h"
#include "photo_mode_effects.h"
#include "ed_nexus.h"
#include "keyboard.h"
#include "launcher.h"
#include "menu_renderer.h"
#include "menu_text.h"
#include "performance_overlay.h"
#include "recomp_input.h"
#include "survival_session.h"
#include "mods/languages.h"
#include "core/input.h"
#include "core/log.h"
#include "cpu/x86_context.h"
#include "kernel/kernel.h"
#include <imgui.h>
#include <miniz.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <initializer_list>
#include <fstream>
#include <mutex>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#else
#include <SDL3/SDL_filesystem.h>
#endif

namespace openshao::photo_mode {
namespace {
using Clock = std::chrono::steady_clock;
// Pad pressure slots: black is RB and white is LB on modern controllers.
constexpr unsigned PhysicalA = 0, PhysicalB = 1, PhysicalX = 2, PhysicalY = 3, Black = 4, White = 5, LeftTrigger = 6, RightTrigger = 7;
constexpr uint16_t DpadUp = 0x01, DpadDown = 0x02, DpadLeft = 0x04, DpadRight = 0x08, Dpad = 0x0F;
constexpr unsigned BindingA = 0, BindingB = 1, BindingX = 2, BindingY = 3, BindingBlack = 4, BindingWhite = 5;
constexpr unsigned BindingDpadUp = 10, BindingDpadDown = 11, BindingDpadLeft = 12, BindingDpadRight = 13;
constexpr unsigned BindingLeftTrigger = 14, BindingRightTrigger = 15, BindingMoveUp = 16, BindingCameraUp = 20;
constexpr unsigned Ports = 4;
constexpr uint32_t WorldPaused = 0x5AE5B0, Preferences = 0x5ACEBC, GameMode = 0x88, VersusMode = 2;
// Keyboards without both stick clicks bound open photo mode with this key instead.
// Without frame updates (loading, a closed game) the exit release must not hold input back.
constexpr auto ReleaseTimeout = std::chrono::milliseconds(250);
constexpr auto NoticeTime = std::chrono::milliseconds(4000), NoticeFade = std::chrono::milliseconds(500);
constexpr uint32_t ThumbnailWidth = 128;
// A held direction repeats after this delay, then at this interval.
constexpr auto RepeatDelay = std::chrono::milliseconds(400), RepeatInterval = std::chrono::milliseconds(110);
// Frames drawn at the raised render scale before the photo, so every target is rebuilt at the new size.
constexpr int BoostFrames = 8;
// Supersampled photos stop at an 8K-tall render, which most graphics cards hold.
constexpr float MaxRenderHeight = 4320;

// Photo mode stays over the Select pause, which keeps every actor drawn; the world pause flag hides them mid-fight.
enum class State { Idle, Active, Release };
enum class Notice { None, Saving, Saved, Failed };

std::mutex mutex;
// Input gathered since the last game frame: several callers poll each port, some with keyboard only.
std::array<input::Pad, Ports> gathered{};
State state = State::Idle;
bool shoot_before = false, hide_before = false, started = false;
bool legend_hidden = false, keyboard_last = false, controller_seen = false;
bool options_before = false, back_before = false, panel_open = false;
// The panel was drawn this frame, so its backdrop belongs in the scene; never while a shot hides the controls.
bool panel_drawn = false;
// The options panel: its focused row and the choices, kept until the game closes.
unsigned panel_row = 0;
Look look;
uint16_t dpad_before = 0;
Clock::time_point repeat_at;
// Render camera: the eye, then X/Y/Z rotations in 4096ths of a turn.
constexpr std::array<uint32_t, 6> RenderCameraFields{0x699C58, 0x699C5C, 0x699C60, 0x699C68, 0x699C6C, 0x699C70};
std::array<float, 6> saved_render_camera{};
Clock::time_point last_frame, changed;
FreeCamera camera;
Sticks last_sticks;
// Vertical field of view in degrees of the current (world +0x304) and world (+0x308) cameras, scaled by the zoom.
constexpr uint32_t FieldOfView = 0x2C;
std::array<uint32_t, 2> zoomed_cameras{};
std::array<float, 2> saved_fov{};
// Scene effects: armed after the renderer drew the 3D (BeforeText), taken by its next 2D draw or by the present.
std::atomic<bool> scene_armed{false}, scene_in_frame{false}, shot_in_frame{false};
bool arm_this_frame = false;
// The camera whose view the next frustum update rolls by the tilt; 0 outside photo mode or without a tilt.
uint32_t roll_camera = 0;
float roll_radians = 0;
// The view of the last gameplay frame: Select can hand the current camera back to the world camera.
Vec3 gameplay_eye, gameplay_ahead;
bool gameplay_view = false;
// Centre of the camera area: the heroes (actor +0x80 position), else where the camera stood.
constexpr std::array<uint32_t, 2> HeroActors{0x5D1AB8, 0x5D4CF0};
constexpr uint32_t HeroPosition = 0x80;
Vec3 area_center;
// The area always reaches back to where the camera opened, so it never jumps on the first frame.
float start_reach = 0, start_height = 0;
bool restore = false;
// A photo waits one frame with the controls hidden; High and Max first raise the render scale for a few frames.
enum Shot : int { NoShot, Boosting, Hiding, Ready };
std::atomic<int> shot{NoShot};
// Presents seen since the shot became ready: the renderer may still be showing frames queued with the controls.
std::atomic<int> ready_presents{0};
constexpr int PresentsBeforeShot = 3;
// Legend diagnostics: presents while the legend should show, those that came without it, and stale camera updates.
std::atomic<bool> legend_expected{false};
std::atomic<int> legend_pending{0}, legend_presents{0}, legend_missed{0}, stale_updates{0};
std::atomic<float> turned_degrees{0};
// Set by each text queue render: the next camera update belongs to a new frame and queues the controls again.
std::atomic<bool> text_rendered{true};
std::atomic<bool> saving{false};
int boost_frames = 0;
std::atomic<float> boost{1}, applied_boost{1};
// The choices and quality of the photo being taken; a failed Max photo retries once as High.
Look shot_look;
bool shot_effects = false, shot_retried = false;
// Set while photo mode queues its own controls, which the 2D blocking in the aspect hooks lets through.
thread_local bool drawing_controls = false;
constexpr int ControlsY = 420, CameraControlsY = 394;
// The last saved photo, shown in the top right corner with a thumbnail and its path.
struct Saved { Notice kind = Notice::None; std::string path; Photo thumbnail; Clock::time_point time; bool fallback = false; };
Saved notice;

bool Busy(const input::Pad& pad) {
    if (!pad.connected) return false;
    if (pad.buttons) return true;
    for (const auto value : pad.analog) if (value > 32) return true;
    for (const int value : {pad.lx, pad.ly, pad.rx, pad.ry}) if (std::abs(value) > 8000) return true;
    return false;
}
void Gather(input::Pad& into, const input::Pad& pad) {
    if (!pad.connected) return;
    into.connected = true;
    into.buttons |= pad.buttons;
    for (unsigned i = 0; i < 8; ++i) into.analog[i] = std::max(into.analog[i], pad.analog[i]);
    auto strongest = [](int16_t& to, int16_t value) { if (std::abs(int(value)) > std::abs(int(to))) to = value; };
    strongest(into.lx, pad.lx);
    strongest(into.ly, pad.ly);
    strongest(into.rx, pad.rx);
    strongest(into.ry, pad.ry);
}
// The exit controls are up: B and both stick clicks; any other held control (a resting trigger) does not count.
bool ExitReleased(const input::Pad& pad) {
    return pad.analog[PhysicalB] <= 32;
}
float Axis(int16_t value) { return std::max(-1.f, value / 32767.f); }
void Enter(State next, Clock::time_point now) { state = next; changed = now; }
// The helpers below run with mutex held.
void LogLook() {
    LOGI("Photo mode options: filter %d amount %d vignette %d tilt %d quality %d reach %d", int(look.filter), look.amount,
         int(look.vignette), look.tilt, int(look.quality), int(look.reach));
}
void SetNotice(Notice kind) {
    notice = {};
    notice.kind = kind;
    notice.time = Clock::now();
}
void RequestPhoto() {
    if (saving.load() || shot.load() != NoShot) return;
    shot_look = look;
    shot_effects = true;
    shot_retried = false;
    SetNotice(Notice::Saving);
    if (look.quality == Quality::Screen) shot = Hiding;
    else {
        boost = look.quality == Quality::High ? 2.f : 4.f;
        boost_frames = 0;
        shot = Boosting;
    }
    LOGI("Photo mode photo requested, quality %d", int(look.quality));
}
// Leaving photo mode drops a photo still waiting for the raised render scale.
void CancelBoost() {
    if (shot.load() != Boosting) return;
    shot = NoShot;
    boost = 1;
    saving = false;
    if (notice.kind == Notice::Saving) notice = {};
}
// Versus keeps its own pause flow; photo mode covers Campaign and Survival, Solo and Ko-op.
bool Allowed() {
    if (survival::ActiveSession()) return true;
    const uint32_t preferences = kernel::R32(Preferences);
    return preferences && kernel::R32(preferences + GameMode) != VersusMode;
}
// Set by the Photo Mode row of the Select root; the next frame opens photo mode.
std::atomic<bool> menu_request{false};
int BoundKey(unsigned binding) {
    for (const int key : launcher::Current().keyboard.keys[binding]) if (key) return key;
    return 0;
}
std::string Caption(std::string_view id) {
    return std::string(menu::Catalog::Builtin().Get(launcher::Current().language, id));
}
std::string KeyName(int key) {
    return menu::Caption(launcher::Current().language, "key." + std::to_string(key));
}

// Once per game frame: button edges, exit and camera input from everything gathered since the last frame.
Sticks Frame(Clock::time_point now, bool& fresh) {
    std::lock_guard guard(mutex);
    // The view can be rebuilt more than once per frame, microseconds apart; frames, even rushed ones, are further apart.
    fresh = now - last_frame >= std::chrono::milliseconds(3);
    if (!fresh) return last_sticks;
    const auto pads = gathered;
    gathered = {};
    last_frame = now;
    bool back = false, shoot = false, hide = false, options = false, released = true;
    uint16_t dpad = 0;
    Sticks sticks;
    for (const auto& pad : pads) {
        if (!pad.connected) continue;
        back |= pad.analog[PhysicalB] > 128;
        shoot |= pad.analog[PhysicalA] > 128;
        hide |= pad.analog[PhysicalY] > 128;
        options |= pad.analog[PhysicalX] > 128;
        dpad |= pad.buttons & Dpad;
        released &= ExitReleased(pad);
        auto strongest = [](float& to, float value) { if (std::abs(value) > std::abs(to)) to = value; };
        strongest(sticks.move_x, Axis(pad.lx));
        strongest(sticks.move_y, Axis(pad.ly));
        strongest(sticks.look_x, Axis(pad.rx));
        strongest(sticks.look_y, Axis(pad.ry));
        sticks.down = std::max(sticks.down, pad.analog[White] / 255.f);
        sticks.up = std::max(sticks.up, pad.analog[Black] / 255.f);
        sticks.zoom_out = std::max(sticks.zoom_out, pad.analog[LeftTrigger] / 255.f);
        sticks.zoom_in = std::max(sticks.zoom_in, pad.analog[RightTrigger] / 255.f);
    }
    const bool take = shoot && !shoot_before, toggle = hide && !hide_before;
    const bool open_options = options && !options_before, back_pressed = back && !back_before;
    shoot_before = shoot;
    hide_before = hide;
    options_before = options;
    back_before = back;
    // Directions act once when pressed, then repeat while held.
    uint16_t moves = dpad & ~dpad_before;
    if (moves) repeat_at = now + RepeatDelay;
    else if (dpad && now >= repeat_at) { moves = dpad; repeat_at = now + RepeatInterval; }
    dpad_before = dpad;
    switch (state) {
    case State::Idle:
        if (menu_request.exchange(false) && ed_nexus::PauseRootShown() && Allowed()) {
            started = legend_hidden = panel_open = panel_drawn = false;
            legend_pending = legend_presents = legend_missed = stale_updates = 0;
            turned_degrees = 0;
            text_rendered = true;
            Enter(State::Active, now);
            LOGI("Photo mode opened");
        }
        break;
    case State::Active:
        if (back_pressed && !panel_open) {
            restore = true;
            panel_open = false;
            CancelBoost();
            Enter(State::Release, now);
            legend_expected = false;
            LOGI("Photo mode closed; legend in %d presents, missing in %d, %d left queued, %d stale camera updates, turned %.0f degrees",
                 legend_presents.load(), legend_missed.load(), legend_pending.load(), stale_updates.load(), turned_degrees.load());
            break;
        }
        if (panel_open && (open_options || back_pressed)) { panel_open = false; LOGI("Photo mode options closed"); }
        else if (!panel_open && open_options) { panel_open = true; legend_hidden = false; LOGI("Photo mode options opened"); }
        else if (panel_open && moves) {
            constexpr unsigned rows = unsigned(Row::Count);
            if (moves & DpadUp) panel_row = (panel_row + rows - 1) % rows;
            if (moves & DpadDown) panel_row = (panel_row + 1) % rows;
            const int direction = (moves & DpadRight ? 1 : 0) - (moves & DpadLeft ? 1 : 0);
            if (direction && Adjust(look, Row(panel_row), direction)) LogLook();
        }
        if (toggle) {
            legend_hidden = !legend_hidden;
            if (legend_hidden) panel_open = false;
            LOGI("Photo mode controls %s", legend_hidden ? "hidden" : "shown");
        }
        if (take && panel_open && Row(panel_row) == Row::Reset) {
            if (Adjust(look, Row::Reset, 1)) LogLook();
        } else if (take) {
            RequestPhoto();
        }
        break;
    case State::Release:
        if (released) { Enter(State::Idle, now); LOGI("Photo mode input returned to the game"); }
        break;
    }
    last_sticks = sticks;
    return sticks;
}

Vec3 Load(uint32_t address) {
    return {kernel::G<float>(address), kernel::G<float>(address + 4), kernel::G<float>(address + 8)};
}

std::filesystem::path PicturesFolder() {
    // Hidden test runs keep their photos out of the player's pictures.
    if (const char* folder = std::getenv("OPENSHAO_PHOTO_DIR"); folder && *folder) return folder;
#ifdef _WIN32
    PWSTR path = nullptr;
    std::filesystem::path result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, KF_FLAG_CREATE, nullptr, &path))) result = path;
    CoTaskMemFree(path);
    return result.empty() ? result : result / "OpenShao";
#else
    if (const char* path = SDL_GetUserFolder(SDL_FOLDER_PICTURES)) return std::filesystem::path(path) / "OpenShao";
    if (const char* home = std::getenv("HOME")) return std::filesystem::path(home) / "Pictures" / "OpenShao";
    return {};
#endif
}
bool WritePhoto(const Photo& photo, std::string& path) {
    if (photo.rgb.empty()) { LOGE("Photo mode: empty frame"); return false; }
    const auto folder = PicturesFolder();
    std::error_code error;
    if (!folder.empty()) std::filesystem::create_directories(folder, error);
    if (folder.empty() || error) {
        LOGE("Photo mode: no pictures folder (%s)", error ? error.message().c_str() : "unknown");
        return false;
    }
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::filesystem::path file;
    for (unsigned index = 1; index < 100; ++index) {
        file = folder / PhotoName(local, index);
        if (!std::filesystem::exists(file, error)) break;
    }
    size_t bytes = 0;
    void* png = tdefl_write_image_to_png_file_in_memory_ex(photo.rgb.data(), int(photo.width), int(photo.height), 3,
                                                           &bytes, 6, MZ_FALSE);
    if (!png) { LOGE("Photo mode: PNG encoding failed"); return false; }
    std::ofstream out(file, std::ios::binary);
    out.write(static_cast<const char*>(png), std::streamsize(bytes));
    out.close();
    mz_free(png);
    const auto name = file.u8string();
    path.assign(name.begin(), name.end());
    if (!out) { LOGE("Photo mode: could not write %s", path.c_str()); return false; }
    LOGI("Photo mode saved %ux%u %s", photo.width, photo.height, path.c_str());
    return true;
}

void Show(Saved value) {
    value.time = Clock::now();
    std::lock_guard guard(mutex);
    notice = std::move(value);
}

// Bound key names joined by spaces, or empty when none of the bindings has a key.
std::string KeyNames(std::initializer_list<unsigned> bindings) {
    std::string names;
    for (const unsigned binding : bindings)
        if (const int key = BoundKey(binding)) names += (names.empty() ? "" : " ") + KeyName(key);
    return names;
}
using Hints = std::vector<menu::FooterHint>;
// What the controls show this frame, copied under the lock.
struct Shown { bool keyboard = false, panel = false; unsigned row = 0; Look look; };
// The actions row: photo, back, options and legend, or the panel's own keys while it is open.
std::string_view TakeCaption(const Shown& shown) {
    return shown.panel && Row(shown.row) == Row::Reset ? "photo.reset" : "photo.take";
}
std::array<Hints, 2> ControllerControls(const Shown& shown) {
    Hints camera{{menu::Button::LeftStick, Caption("photo.move")}, {menu::Button::RightStick, Caption("photo.look")},
                 {menu::Button::Triggers, Caption("photo.zoom")}, {menu::Button::Shoulders, Caption("photo.height")}};
    if (shown.panel)
        return {camera, Hints{{menu::Button::Navigate, Caption("footer.navigate")}, {menu::Button::Adjust, Caption("settings.adjust_hint")},
                              {menu::Button::X, Caption("photo.close")}, {menu::Button::Accept, Caption(TakeCaption(shown))}}};
    return {camera, Hints{{menu::Button::Accept, Caption("photo.take")}, {menu::Button::Back, Caption("footer.back")},
                          {menu::Button::X, Caption("photo.options")}, {menu::Button::Y, Caption("photo.hide")}}};
}
std::array<Hints, 2> KeyboardControls(const Shown& shown) {
    std::array<Hints, 2> rows;
    const auto add = [](Hints& row, const std::string& keys, std::string_view caption) {
        if (!keys.empty()) row.push_back({menu::Button::None, keys + "  " + Caption(caption)});
    };
    // Bindings run up, down, left, right from BindingMoveUp and BindingCameraUp; shown as up, left, down, right.
    add(rows[0], KeyNames({BindingMoveUp, BindingMoveUp + 2, BindingMoveUp + 1, BindingMoveUp + 3}), "photo.move");
    add(rows[0], KeyNames({BindingCameraUp, BindingCameraUp + 2, BindingCameraUp + 1, BindingCameraUp + 3}), "photo.look");
    add(rows[0], KeyNames({BindingLeftTrigger, BindingRightTrigger}), "photo.zoom");
    add(rows[0], KeyNames({BindingWhite, BindingBlack}), "photo.height");
    if (shown.panel) {
        add(rows[1], KeyNames({BindingDpadUp, BindingDpadDown}), "footer.navigate");
        add(rows[1], KeyNames({BindingDpadLeft, BindingDpadRight}), "settings.adjust_hint");
        add(rows[1], KeyNames({BindingX}), "photo.close");
        add(rows[1], KeyNames({BindingA}), TakeCaption(shown));
    } else {
        add(rows[1], KeyNames({BindingA}), "photo.take");
        add(rows[1], KeyNames({BindingB}), "footer.back");
        add(rows[1], KeyNames({BindingX}), "photo.options");
        add(rows[1], KeyNames({BindingY}), "photo.hide");
    }
    return rows;
}
// Options panel in the top left of the canvas, in the style of the game's settings lists.
void DrawPanel(X86Context& ctx, const Shown& shown) {
    using P = PanelLayout;
    menu::NativeLabel(ctx, P::LabelX, P::TitleY, Caption("photo.options_title"), .9f, 254, true, false, P::Right - P::LabelX);
    for (unsigned index = 0; index < unsigned(Row::Count); ++index) {
        const Row row = Row(index);
        const bool focused = index == shown.row;
        const float y = P::RowsY + index * P::RowStep;
        const bool wide = row == Row::Reset;
        menu::NativeLabel(ctx, P::LabelX, y, Caption(RowCaption(row)), .8f, focused ? 254 : 190, false, true,
                          wide ? P::Right - P::LabelX : P::ValueX - P::LabelX - 6);
        if (wide) continue;
        const auto value = RowValue(shown.look, row);
        std::string text = value.number ? value.text : Caption(value.text);
        if (focused) text = "<  " + text + "  >";
        menu::NativeLabel(ctx, P::ValueX, y, text, .75f, focused ? 254 : 210, false, true, P::Right - P::ValueX);
    }
}
// The game's own footer: button glyphs on a controller, bound key names on the keyboard.
void DrawControls(X86Context& ctx, const Shown& shown) {
    drawing_controls = true;
    const auto draw = [&](const std::array<Hints, 2>& rows) {
        menu::NativeFooter(ctx, CameraControlsY, rows[0]);
        menu::NativeFooter(ctx, ControlsY, rows[1]);
    };
    try {
        if (shown.panel) DrawPanel(ctx, shown);
        draw(shown.keyboard ? KeyboardControls(shown) : ControllerControls(shown));
    } catch (const std::exception& error) {
        // A key name outside the game font falls back to the controller icons.
        LOGE("Photo mode controls: %s", error.what());
        try { draw(ControllerControls(shown)); } catch (const std::exception&) {}
    }
    drawing_controls = false;
}
}

void Poll(int port, input::Pad& pad) {
    const input::Pad controller_pad = pad;
    keyboard::Poll(port, pad);
    if (port < 0 || port >= int(Ports)) return;
    std::lock_guard guard(mutex);
    controller_seen |= controller_pad.connected;
    if (Busy(controller_pad)) keyboard_last = false;
    else if (Busy(pad)) keyboard_last = true;
    Gather(gathered[size_t(port)], pad);
    if (state == State::Release && Clock::now() - last_frame > ReleaseTimeout) state = State::Idle;
    if (state == State::Idle) return;
    // Select never sees the camera controls or the exit press.
    const bool connected = pad.connected;
    pad = {};
    pad.connected = connected;
}

bool Active() { std::lock_guard guard(mutex); return state == State::Active; }

bool DrawingControls() { return drawing_controls; }

bool Available() { return Allowed(); }
void OpenFromMenu() {
    menu_request = true;
    LOGI("Photo mode requested from the Select menu");
}

bool Message(uint32_t message, uintptr_t key, intptr_t flags) {
    constexpr uint32_t KeyDown = 0x100, KeyUp = 0x101;
    constexpr uintptr_t F12 = 0x7B;
    if (key != F12 || (message != KeyDown && message != KeyUp)) return false;
    // Bit 30 marks auto-repeat: a held key takes one screenshot.
    if (message == KeyDown && !(uintptr_t(flags) & (uintptr_t(1) << 30)) && !saving.load()) {
        std::lock_guard guard(mutex);
        const bool active = state == State::Active;
        int idle = NoShot;
        // F12 keeps the screen size; inside photo mode it carries the chosen effects.
        if (shot.compare_exchange_strong(idle, active ? Hiding : Ready)) {
            shot_look = active ? look : Look{};
            shot_look.quality = Quality::Screen;
            shot_effects = active;
            shot_retried = false;
            LOGI("Screenshot requested");
        }
    }
    return true;
}

bool ShotRequested() {
    // Called once per present: whether this frame already carries the effects (the D3D12 scene pass).
    const bool in_frame = scene_in_frame.exchange(false);
    if (legend_expected.load()) {
        ++legend_presents;
        int pending = legend_pending.load();
        while (pending > 0 && !legend_pending.compare_exchange_weak(pending, pending - 1)) {}
        if (pending <= 0) { ++legend_missed; LOG_FIRST(10, "Photo mode legend missing from a present"); }
    }
    if (shot.load() != Ready) { ready_presents = 0; return false; }
    if (++ready_presents < PresentsBeforeShot) return false;
    int ready = Ready;
    if (!shot.compare_exchange_strong(ready, NoShot)) return false;
    ready_presents = 0;
    shot_in_frame = in_frame;
    saving = true;
    boost = 1;
    return true;
}

float RenderBoost(float render_height) {
    const float wanted = boost.load();
    float applied = 1;
    if (wanted > 1 && render_height > 0) applied = std::clamp(MaxRenderHeight / render_height, 1.f, wanted);
    applied_boost = applied;
    return applied;
}

void SaveShot(std::vector<uint8_t> bgra, uint32_t width, uint32_t height, float aspect) {
    const float factor = applied_boost.load();
    Look taken;
    bool effects = false, fallback = false;
    {
        std::lock_guard guard(mutex);
        if (bgra.empty() && shot_look.quality == Quality::Max && !shot_retried) {
            // A Max frame the renderer could not copy is taken again as High.
            LOGE("Photo mode: the Max photo could not be copied, retrying as High");
            shot_retried = true;
            shot_look.quality = Quality::High;
            boost = 2;
            boost_frames = 0;
            shot = Boosting;
            return;
        }
        taken = shot_look;
        effects = shot_effects;
        fallback = shot_retried;
    }
    if (bgra.empty()) {
        LOGE("Photo mode: the renderer could not copy the frame");
        saving = false;
        Saved failed;
        failed.kind = Notice::Failed;
        Show(std::move(failed));
        return;
    }
    // PNG encoding takes long enough at high render scales to stall the presenting thread.
    const bool in_frame = shot_in_frame.load();
    std::thread([bgra = std::move(bgra), width, height, aspect, factor, taken, effects, fallback, in_frame] {
        auto photo = DisplayPhoto(bgra, width, height, aspect);
        if (taken.quality == Quality::High) photo = Downscale(photo, factor);
        if (effects && !in_frame && taken.Changes()) photo = ApplyEffects(photo, MakeParams(taken, float(photo.width) / float(photo.height)));
        Saved result;
        result.kind = Notice::Failed;
        result.fallback = fallback;
        if (WritePhoto(photo, result.path)) {
            result.kind = Notice::Saved;
            result.thumbnail = Thumbnail(photo, ThumbnailWidth);
        }
        saving = false;
        Show(std::move(result));
    }).detach();
}

// With mutex held: the scene pass has something to do.
bool SceneNeeded() { return state == State::Active && (look.Changes() || panel_drawn); }

void BeforeText() {
    text_rendered = true;
    {
        std::lock_guard guard(mutex);
        if (!arm_this_frame || !SceneNeeded()) return;
        arm_this_frame = false;
    }
    // Everything queued so far (the 3D) is drawn once the renderer is idle; the next 2D draw gets the effects first.
    FlushRenderer();
    scene_armed = true;
    LOG_FIRST(1, "Photo mode effects armed before the text queues");
}

bool TakeSceneArm() { return scene_armed.exchange(false); }

bool SceneParams(float params[8]) {
    std::lock_guard guard(mutex);
    if (!SceneNeeded()) return false;
    const auto p = MakeParams(look, 1);
    params[0] = look.filter == Filter::None ? 0.f : float(int(look.filter));
    params[1] = p.amount;
    params[2] = p.vignette;
    params[3] = 4.f / 3;
    params[4] = 1;
    params[5] = panel_drawn ? 1.f : 0.f;
    params[6] = PanelLayout::Top / 480.f;
    params[7] = PanelLayout::Bottom / 480.f;
    return true;
}

const char* SceneShader() { return SceneHlsl; }

void NoteSceneInFrame(bool before_controls) {
    scene_in_frame = true;
    if (before_controls) LOG_FIRST(1, "Photo mode effects drawn under the controls");
    else LOG_FIRST(1, "Photo mode effects drawn over the finished frame");
}

bool Preview(float aspect, PreviewParams& out) {
    std::lock_guard guard(mutex);
    if (state != State::Active) return false;
    const bool panel = panel_drawn;
    if (!look.Changes() && !panel) return false;
    // The effects cover the whole frame; the open panel gets a dark backdrop.
    const auto panel_rect = panel ? CanvasRect(PanelLayout::Left, PanelLayout::Top,
                                             PanelLayout::Right, PanelLayout::Bottom, aspect) : NoRect;
    out = MakePreview(MakeParams(look, aspect), panel_rect, .4f);
    return true;
}

const char* PreviewShader() { return PreviewHlsl; }

void Ui(double fps) {
    performance::Draw(fps);
    Saved shown;
    {
        std::lock_guard guard(mutex);
        const auto age = Clock::now() - notice.time;
        // The saving notice stays until the photo is written.
        if (notice.kind != Notice::None && notice.kind != Notice::Saving && age > NoticeTime) notice = {};
        shown.kind = notice.kind;
        shown.time = notice.time;
        if (notice.kind != Notice::None) { shown.path = notice.path; shown.thumbnail = notice.thumbnail; shown.fallback = notice.fallback; }
    }
    // Interface > Photo Notifications turns the notice off; photos are still saved.
    if (shown.kind == Notice::None || !launcher::Current().screenshot_notifications) return;
    const auto* viewport = ImGui::GetMainViewport();
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    ImFont* font = ImGui::GetFont();
    const float size = std::max(18.f, viewport->Size.y * .036f), text_size = size * .8f, margin = size * .45f;
    if (shown.kind != Notice::None) {
        // Top right: the thumbnail beside the result and the file path, fading out at the end.
        const float left_time = std::chrono::duration<float>(NoticeTime - (Clock::now() - shown.time)).count();
        const bool busy = shown.kind == Notice::Saving;
        const float alpha = busy ? 1.f : std::clamp(left_time / std::chrono::duration<float>(NoticeFade).count(), 0.f, 1.f);
        const auto fade = [&](int r, int g, int b, int a) { return IM_COL32(r, g, b, int(a * alpha)); };
        const float edge = size * .6f, line = text_size * 1.25f, path_size = text_size * .8f;
        const float thumb_h = shown.thumbnail.width ? size * 2.6f : 0;
        const float thumb_w = shown.thumbnail.width ? thumb_h * shown.thumbnail.width / shown.thumbnail.height : 0;
        const float text_w = std::min(viewport->Size.x * .32f, viewport->Size.x - thumb_w - edge * 4);
        const auto title = Caption(busy ? "photo.saving" : shown.kind == Notice::Failed ? "photo.failed"
                                   : shown.fallback ? "photo.saved_high" : "photo.saved");
        const auto path = EllipsizeMiddle(shown.path, [&](const std::string& text) {
            return font->CalcTextSizeA(path_size, FLT_MAX, 0, text.c_str()).x <= text_w;
        });
        const float width = (thumb_w ? thumb_w + margin : 0) + std::max({
            font->CalcTextSizeA(text_size, FLT_MAX, 0, title.c_str()).x,
            path.empty() ? 0.f : font->CalcTextSizeA(path_size, FLT_MAX, 0, path.c_str()).x, busy ? size * 7 : 0.f});
        const float height = std::max(thumb_h, path.empty() && !busy ? line : line * 2);
        const ImVec2 at{viewport->Pos.x + viewport->Size.x - edge - width, viewport->Pos.y + edge};
        draw->AddRectFilled({at.x - margin, at.y - margin}, {at.x + width + margin, at.y + height + margin},
                            fade(0, 0, 0, 170), size * .3f);
        float x = at.x;
        if (thumb_w) {
            const auto& thumb = shown.thumbnail;
            const float cell = thumb_h / thumb.height;
            for (uint32_t ty = 0; ty < thumb.height; ++ty)
                for (uint32_t tx = 0; tx < thumb.width; ++tx) {
                    const uint8_t* rgb = &thumb.rgb[(size_t(ty) * thumb.width + tx) * 3];
                    const ImVec2 corner{x + tx * cell, at.y + ty * cell};
                    draw->AddRectFilled(corner, {corner.x + cell + .5f, corner.y + cell + .5f}, fade(rgb[0], rgb[1], rgb[2], 255));
                }
            draw->AddRect({x, at.y}, {x + thumb_w, at.y + thumb_h}, fade(255, 255, 255, 90));
            x += thumb_w + margin;
        }
        const float text_top = at.y + (height - (path.empty() && !busy ? line : line * 2)) * .5f;
        draw->AddText(font, text_size, {x, text_top}, fade(255, 255, 255, 255), title.c_str());
        if (!path.empty()) draw->AddText(font, path_size, {x, text_top + line}, fade(200, 200, 200, 255), path.c_str());
        if (busy) {
            // An indeterminate bar: the encoder reports no progress, so a segment sweeps across while it works.
            const float bar_top = text_top + line * 1.35f, bar_h = std::max(3.f, size * .16f), bar_w = at.x + width - x;
            draw->AddRectFilled({x, bar_top}, {x + bar_w, bar_top + bar_h}, fade(255, 255, 255, 50), bar_h * .5f);
            const float cycle = std::fmod(std::chrono::duration<float>(Clock::now() - shown.time).count(), 1.2f) / 1.2f;
            const float segment = bar_w * .35f, start = (bar_w + segment) * cycle - segment;
            const float left = x + std::max(0.f, start), right = x + std::min(bar_w, start + segment);
            if (right > left) draw->AddRectFilled({left, bar_top}, {right, bar_top + bar_h}, fade(255, 255, 255, 220), bar_h * .5f);
        }
    }
}
}

X86_EXTERN_FUNC(__imp__sub_00123BD0);
X86_FUNC(sub_00123BD0) {
    using namespace openshao::photo_mode;
    // Frustum corners (fastcall, ecx = camera) from the view at +0x40 and the projection at +0x80.
    if (roll_camera && ctx.ecx == roll_camera) {
        float view[16];
        for (unsigned i = 0; i < 16; ++i) view[i] = kernel::G<float>(roll_camera + 0x40 + i * 4);
        RollView(view, roll_radians);
        for (unsigned i = 0; i < 16; ++i) kernel::G<float>(roll_camera + 0x40 + i * 4) = view[i];
    }
    __imp__sub_00123BD0(ctx, base);
}

X86_EXTERN_FUNC(__imp__sub_00124150);
X86_FUNC(sub_00124150) {
    using namespace openshao::photo_mode;
    // View update (fastcall): +0x33C clear looks from +0x340 at +0x00, set rebuilds +0x00 from the angles at +0x350.
    const uint32_t object = ctx.ecx, world = kernel::R32(0x639224);
    if (!world || object != kernel::R32(world + 0x304)) { __imp__sub_00124150(ctx, base); return; }
    const auto now = Clock::now();
    bool fresh = false;
    const auto sticks = Frame(now, fresh);
    static Clock::time_point last_update;
    bool first = false, put_back = false;
    float roll = 0;
    Reach reach = Reach::Normal;
    {
        std::lock_guard guard(mutex);
        roll = look.Radians();
        reach = look.reach;
        if (state != State::Active) started = false;
        else if (!started) started = first = true;
        if (state != State::Active && restore) { restore = false; put_back = true; }
    }
    if (put_back) {
        for (size_t i = 0; i < RenderCameraFields.size(); ++i) kernel::G<float>(RenderCameraFields[i]) = saved_render_camera[i];
        for (size_t i = 0; i < zoomed_cameras.size(); ++i)
            if (zoomed_cameras[i]) kernel::G<float>(zoomed_cameras[i] + FieldOfView) = saved_fov[i];
    }
    if (!Active()) {
        // Photo mode closed before its photo: nothing is left to hide.
        int hiding = Hiding;
        if (fresh) shot.compare_exchange_strong(hiding, Ready);
        __imp__sub_00124150(ctx, base);
        if (!openshao::ed_nexus::GameplayPaused() && !kernel::G<uint8_t>(WorldPaused)) {
            float view[16];
            for (unsigned i = 0; i < 16; ++i) view[i] = kernel::G<float>(object + 0x40 + i * 4);
            gameplay_view = FromView(view, gameplay_eye, gameplay_ahead);
        }
        return;
    }
    if (first) {
        // Start from the last gameplay view; the current camera's own eye can be the world camera's.
        constexpr uint32_t Eye = 0x340, Target = 0x00;
        const Vec3 eye = gameplay_view ? gameplay_eye : Load(object + Eye);
        const Vec3 ahead = gameplay_view ? gameplay_ahead : Load(object + Target);
        camera.Begin(eye, ahead);
        // Hero 1, or the middle of both heroes in Ko-op; a hero far from the camera is not in this scene (the game camera stays about 4000 away).
        const auto distance = [](Vec3 a, Vec3 b) {
            const float d = std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
            return std::isfinite(d) ? d : 1e30f;
        };
        const Vec3 first = Load(HeroActors[0] + HeroPosition), second = Load(HeroActors[1] + HeroPosition);
        const uint32_t preferences = kernel::R32(Preferences);
        const bool solo = preferences && kernel::R32(preferences + GameMode) == 0;
        constexpr float Near = 8000;
        const bool first_here = distance(first, eye) <= Near, second_here = !solo && distance(second, eye) <= Near;
        const bool both = first_here && second_here;
        const Vec3 middle{(first.x + second.x) / 2, (first.y + second.y) / 2, (first.z + second.z) / 2};
        area_center = both ? middle : first_here ? first : second_here ? second : eye;
        // Wide enough to keep the camera where it opened and to frame both heroes.
        start_reach = std::max(std::hypot(eye.x - area_center.x, eye.z - area_center.z) + 100,
                               both ? std::hypot(first.x - second.x, first.z - second.z) / 2 + 1500 : 0.f);
        start_height = std::abs(eye.y - area_center.y) + 100;
        LOGI("Photo mode heroes at (%.1f, %.1f, %.1f) and (%.1f, %.1f, %.1f)%s", first.x, first.y, first.z, second.x, second.y,
             second.z, solo ? ", solo" : "");
        LOGI("Photo mode area centre (%.1f, %.1f, %.1f), %.0f from the camera, %s", area_center.x, area_center.y, area_center.z,
             distance(area_center, eye), both ? "between both heroes" : first_here || second_here ? "on one hero" : "no hero near, on the camera");
        last_update = now;
        for (size_t i = 0; i < RenderCameraFields.size(); ++i) saved_render_camera[i] = kernel::G<float>(RenderCameraFields[i]);
        const uint32_t world_camera = kernel::R32(world + 0x308);
        zoomed_cameras = {object, world_camera != object ? world_camera : 0};
        for (size_t i = 0; i < zoomed_cameras.size(); ++i)
            if (zoomed_cameras[i]) saved_fov[i] = kernel::G<float>(zoomed_cameras[i] + FieldOfView);
        LOGI("Photo mode camera from (%.1f, %.1f, %.1f), field of view %.1f%s", eye.x, eye.y, eye.z, saved_fov[0],
             zoomed_cameras[1] ? " and a separate world camera" : "");
    }
    if (reach == Reach::Free) camera.SetArea(area_center, 0, 0);
    else if (reach == Reach::Large)
        camera.SetArea(area_center, std::max(FreeCamera::LargeRange, start_reach), std::max(FreeCamera::LargeHeight, start_height));
    else camera.SetArea(area_center, std::max(FreeCamera::Range, start_reach), std::max(FreeCamera::Height, start_height));
    const float yaw0 = camera.Yaw(), pitch0 = camera.Pitch(), zoom0 = camera.Zoom();
    camera.Update(sticks, std::chrono::duration<float>(now - last_update).count());
    turned_degrees = turned_degrees.load() + (camera.Yaw() - yaw0) * 57.29578f;
    last_update = now;
    if (fresh && camera.Zoom() != zoom0 && (camera.Zoom() == FreeCamera::MinZoom || camera.Zoom() == FreeCamera::MaxZoom))
        LOGI("Photo mode zoom limit %.2f", camera.Zoom());
    // The render camera holds the eye and X/Y rotations in 4096ths of a turn; X turns the view up for less.
    constexpr float Units = 4096 / 6.2831853f;
    const Vec3 eye = camera.Eye();
    kernel::G<float>(RenderCameraFields[0]) = eye.x;
    kernel::G<float>(RenderCameraFields[1]) = eye.y;
    kernel::G<float>(RenderCameraFields[2]) = eye.z;
    kernel::G<float>(RenderCameraFields[3]) -= (camera.Pitch() - pitch0) * Units;
    kernel::G<float>(RenderCameraFields[4]) += (camera.Yaw() - yaw0) * Units;
    for (size_t i = 0; i < zoomed_cameras.size(); ++i)
        if (zoomed_cameras[i]) kernel::G<float>(zoomed_cameras[i] + FieldOfView) = saved_fov[i] * camera.Zoom();
    // The tilt rolls this camera's view before the game derives its frustum from it (sub_00123BD0).
    roll_camera = roll ? object : 0;
    roll_radians = roll;
    __imp__sub_00124150(ctx, base);
    roll_camera = 0;
    // A frame is new once its text queues were rendered; time alone missed frames that came under 3 ms apart.
    static Clock::time_point last_block;
    const bool new_frame = text_rendered.exchange(false) || now - last_block > std::chrono::milliseconds(100);
    if (!new_frame) { ++stale_updates; return; }
    last_block = now;
    // The camera update runs before this frame's 2D passes, so queued controls show in the same frame.
    bool hidden;
    Shown shown;
    {
        std::lock_guard guard(mutex);
        hidden = legend_hidden;
        shown = {keyboard_last || !controller_seen, panel_open, panel_row, look};
        if (shot.load() == Boosting && ++boost_frames >= BoostFrames) shot = Hiding;
        arm_this_frame = true;
    }
    int hiding = Hiding;
    // The controls stay hidden from this frame until the present takes the photo, however many updates come first.
    if (shot.compare_exchange_strong(hiding, Ready) || hiding == Ready) hidden = true;
    {
        std::lock_guard guard(mutex);
        panel_drawn = !hidden && shown.panel;
    }
    legend_expected = !hidden;
    if (!hidden) { ++legend_pending; DrawControls(ctx, shown); }
}
