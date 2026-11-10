#include "app/app.h"
#include "keyboard.h"
#include "controller.h"
#include "performance_overlay.h"
#include "cheat_menu.h"
#include "launcher.h"
#include "brightness.h"

namespace {
void DrawOverlays(double fps) {
    openshao::performance::Draw(fps);
    openshao::cheat::Draw(fps);
}

// The SDK creates the ImGui context just before this runs and the renderer builds the font atlas later.
void OnWindow(void* hwnd) {
    openshao::cheat::InitFonts();
    openshao::controller::Window(hwnd);
}
}

const app::GameInfo app::kGame = {
    .title = L"OpenShao",
    .short_title = L"OpenShao",
    .name = L"OpenShao",
    .log = "openshao",
    .settings = "settings.toml",
    .splash = L"openshao",
    .missing = L"Select your Xbox game files.",
    .images = {{"default.xbe", "OpenShao"}},
    .options = {},
    .load = openshao::launcher::Load, .save = openshao::launcher::Save, .init = nullptr,
    .loaded = [] { openshao::brightness::Initialize(openshao::launcher::Current().brightness); },
    .window = OnWindow, .message = openshao::controller::Message, .ui = DrawOverlays,
    .pad = openshao::keyboard::Poll, .rumble = nullptr,
};
