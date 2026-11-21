#include "app/app.h"
#include "photo_mode.h"
#include "controller.h"
#include "launcher.h"
#include "brightness.h"

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
    .window = openshao::controller::Window, .message = openshao::controller::Message, .ui = openshao::photo_mode::Ui,
    .pad = openshao::photo_mode::Poll, .rumble = nullptr,
};
