#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <wrl/client.h>
#else
#include "linux/linux_platform.h"
#endif
#include <SDL3/SDL.h>
#include <imgui.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include <vector>
#include "launcher.h"
#include "discord_presence.h"
#include "launcher_art.h"
#include "launcher_footer_layout.h"
#include "launcher_settings.h"
#include "launcher_text.h"
#include "launcher_window.h"
#include "app/app.h"
#include "controller.h"
#include "core/log.h"
#include "file_ops.h"
#include "frame_rate.h"
#include "game_setup.h"
#include "issue_link.h"
#include "host.h"
#include "keyboard.h"
#include "language_pack.h"
#include "mods/browser.h"
#include "mods/languages.h"
#include "mods/version.h"
#include "mouse.h"
#include "performance_overlay.h"
#include "cheat_menu.h"
#include "runtime_paths.h"
#include "support_report.h"
#include "updater.h"

namespace fs = std::filesystem;
namespace openshao::launcher {
namespace {
Settings current;
std::atomic<std::shared_ptr<const Settings>> published{std::make_shared<const Settings>()};
std::atomic<float> audio_gain{1.f};
std::string requested_language,requested_voice_language;
std::mutex settings_mutex;
void Publish(){published.store(std::make_shared<const Settings>(current));audio_gain.store(current.master_volume/100.f);}
std::atomic<bool> persist{true};
std::wstring Wide(const std::string& text) { return fs::path(std::u8string(text.begin(), text.end())).wstring(); }
std::string Narrow(const std::wstring& text) { auto bytes = fs::path(text).u8string(); return {bytes.begin(), bytes.end()}; }
toml::table Disk() {
    std::ifstream file(paths::Config(), std::ios::binary);
    return file ? toml::parse(file) : toml::table{};
}
const char* T(std::string_view key) { return text::C(key); }
std::string F(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string_view>> values) {
    return text::Format(key, values);
}
// The launcher language lives in [launcher].language. An absent or unknown value starts the first-run choice.
std::string SavedLauncherLanguage() {
    try {
        const auto saved = Disk()["launcher"]["language"].value<std::string>();
        if (saved) for (const auto& language : text::Languages()) if (mods::SameLanguage(language.code, *saved)) return language.code;
    } catch (...) {}
    return {};
}
std::vector<std::string> SystemLocales() {
    // Tests set OPENSHAO_LAUNCHER_LOCALE to simulate another system language.
    if (const char* forced = std::getenv("OPENSHAO_LAUNCHER_LOCALE"); forced && *forced) return {forced};
    std::vector<std::string> locales;
    int count = 0;
    if (SDL_Locale** list = SDL_GetPreferredLocales(&count)) {
        for (int i = 0; i < count && list[i]; ++i) {
            if (!list[i]->language) continue;
            std::string code = list[i]->language;
            if (list[i]->country && *list[i]->country) code += "-" + std::string(list[i]->country);
            locales.push_back(std::move(code));
        }
        SDL_free(list);
    }
    return locales;
}
std::string LanguageName(std::string_view code) {
    for (const auto& language : text::Languages()) if (language.code == code) return language.name;
    return std::string(code);
}
void Apply(const Settings& s, bool save) {
    current = s; persist = save;
    Publish();
    auto& host = app::Get();
    host.display_mode = s.display_mode == "fullscreen" ? app::DisplayMode::Fullscreen :
                        s.display_mode == "windowed" ? app::DisplayMode::Windowed : app::DisplayMode::Borderless;
    host.width = s.width; host.height = s.height; host.render_percent = s.render_percent;
    // native_frames raises the cap only while it produces output frames.
    host.fps_cap = FrameRate; host.vsync = s.vsync; host.aniso = s.aniso;
    frames::Configure(s.fps_cap,s.vsync);
    auto table = Merge({}, s);
    controller::Load(&table);
    mouse::Configure(s.mouse);
    keyboard::Configure(s.keyboard);
    performance::SetVisible(s.show_fps);
    cheat::SetEnabled(s.cheats_enabled);
    cheat::SetHotkey(s.cheat_menu_key);
    cheat::SetStatueFullHeal(s.statue_full_heal);
    cheat::SetMenuPosition(s.cheat_menu_x, s.cheat_menu_y);
}
bool NeedsLanguagePack(std::string_view code) {
    return !mods::IsOriginalLanguage(code)&&options.language_pack.empty()&&language::FindInstalledPack(paths::Mods(),code).empty();
}
std::string MissingLanguagePack(const Settings& settings) {
    if(NeedsLanguagePack(settings.language)) return F("message.text_pack",{{"language",settings.language}});
    if(NeedsLanguagePack(settings.voice_language)) return F("message.voice_pack",{{"language",settings.voice_language}});
    return {};
}
bool PlayWithSavedSettings() {
    Settings settings;
    try { settings = Read(Disk()); }
    catch (const std::exception& e) { paths::ReportError(L"Cannot read settings.toml.\n" + Wide(e.what())); return false; }
    std::string error;
    if (!Validate(settings, error)) { paths::ReportError(L"Check settings.toml: " + Wide(error)); return false; }
    if(const auto missing=MissingLanguagePack(settings);!missing.empty()) {
        paths::ReportError(Wide(missing));
        return false;
    }
    Apply(settings, true);
    if (options.configure_only) {
        std::printf("Settings: %s\n", Narrow(paths::Config().wstring()).c_str());
        return false;
    }
    return true;
}

void OpenLocation(const std::string& target) {
#ifdef _WIN32
    ShellExecuteW(nullptr, L"open", Wide(target).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    linux_platform::OpenFolder(files::FromUtf8(target));
#endif
}
#ifdef _WIN32
bool ChooseWithDialog(fs::path& path, bool folder, const wchar_t* title, const COMDLG_FILTERSPEC* filters = nullptr, UINT filter_count = 0) {
    const auto initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool chosen = false;
    {
        Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
            dialog->SetOptions(FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_DONTADDTORECENT | (folder ? FOS_PICKFOLDERS : FOS_FILEMUSTEXIST));
            dialog->SetTitle(title);
            if (!folder && filter_count) dialog->SetFileTypes(filter_count, filters);
            if (SUCCEEDED(dialog->Show(GetActiveWindow()))) {
                Microsoft::WRL::ComPtr<IShellItem> item; PWSTR value = nullptr;
                if (SUCCEEDED(dialog->GetResult(&item)) && SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &value))) {
                    path = value; CoTaskMemFree(value); chosen = true;
                }
            }
        }
    }
    if (SUCCEEDED(initialized)) CoUninitialize();
    return chosen;
}
#endif
bool ChooseFolder(fs::path& path, const std::string& title) {
#ifdef _WIN32
    return ChooseWithDialog(path, true, Wide(title).c_str());
#else
    return linux_platform::ChooseFile(path, true, title.c_str());
#endif
}
bool ChooseModArchive(fs::path& path) {
#ifdef _WIN32
    const auto label = Wide(T("mods.dialog_filter"));
    const COMDLG_FILTERSPEC filter[] = {{label.c_str(), L"*.tar.xz"}};
    return ChooseWithDialog(path, false, Wide(T("mods.dialog_title")).c_str(), filter, 1);
#else
    return linux_platform::ChooseFile(path, false, T("mods.dialog_title"));
#endif
}

bool ChooseGameSource(fs::path& path, bool folder) {
#ifdef _WIN32
    const auto games = Wide(T("game.dialog_filter")), all = Wide(T("game.dialog_all"));
    const COMDLG_FILTERSPEC filters[] = {{games.c_str(), L"*.iso;*.xiso;*.zip;default.xbe"}, {all.c_str(), L"*.*"}};
    return ChooseWithDialog(path, folder, Wide(T(folder ? "game.dialog_folder" : "game.dialog_file")).c_str(), filters, 2);
#else
    return linux_platform::ChooseFile(path, folder, T(folder ? "game.dialog_folder" : "game.dialog_file"));
#endif
}

// Game (source selection) and Language (first run) replace the tabs until resolved.
enum class Page { Play, Mods, Files, Support, About, Game, Language };
const char* PageName(Page page) {
    switch (page) {
        case Page::Mods: return "mods"; case Page::Files: return "files"; case Page::About: return "about";
        case Page::Support: return "support";
        case Page::Game: return "game"; case Page::Language: return "language"; default: return "play";
    }
}

// Prepare the game off the interface thread so ZIP imports remain responsive and cancellable.
class GamePreparation {
public:
    explicit GamePreparation(fs::path input) : input_(std::move(input)) {
        worker_ = std::thread([this] {
            setup::Source prepared; std::vector<uint8_t> executable; std::string error;
            bool ok = setup::PrepareSource(input_, prepared, executable, error, [this](uint64_t done, uint64_t total) {
                done_ = done; total_ = total;
                return !cancel_.load();
            });
            if (ok && !setup::RememberDisc(prepared.path, error)) ok = false;
            std::lock_guard lock(mutex_);
            ok_ = ok; error_ = cancel_ ? std::string(T("game.import_cancelled")) : error; path_ = prepared.path; finished_ = true;
        });
    }
    ~GamePreparation() { cancel_ = true; if (worker_.joinable()) worker_.join(); }
    void Cancel() { cancel_ = true; }
    bool Finished() { std::lock_guard lock(mutex_); return finished_; }
    // Read results only when Finished() returns true.
    bool Ok() { std::lock_guard lock(mutex_); return ok_; }
    std::string Error() { std::lock_guard lock(mutex_); return error_; }
    fs::path Path() { std::lock_guard lock(mutex_); return path_; }
    float Progress() const { const uint64_t total = total_; return total ? float(double(done_.load()) / double(total)) : 0.f; }
    bool Importing() const { return total_.load() != 0; }
    bool Cancelling() const { return cancel_.load(); }
private:
    fs::path input_;
    std::thread worker_;
    std::mutex mutex_;
    std::atomic<bool> cancel_{false};
    std::atomic<uint64_t> done_{0}, total_{0};
    bool finished_ = false, ok_ = false;
    std::string error_;
    fs::path path_;
};

struct Popup {
    std::string title, text, confirm;  // an empty confirm label shows one dismiss button
    std::function<void()> action;
    bool close_after = false;
    std::string cancel;  // the second button's label; empty shows "Cancel"
};

struct Session {
    Page page = Page::Play;
    Settings settings;
    fs::path source;
    fs::path data_folder, save_folder;
    std::unique_ptr<mods::Browser> mods;
    std::unique_ptr<update::Updater> updater;
    std::optional<Popup> popup;
    int result = -1;  // 0 closes, 1 plays
    bool relaunch = false, check_updates = true, discord_presence = true;
    bool game_required = false;  // no usable game yet: the selection page cannot be left
    Page game_return = Page::Play;
    bool update_offered = false;  // the update popup shows once per launcher session
    bool language_required = false;  // first run: a language must be confirmed before anything else
    Page language_return = Page::Play;
    std::string language_saved, language_choice, system_language;
    int language_columns = 1;
    std::unique_ptr<GamePreparation> preparing;
    std::string game_error;
    std::future<support::Report> report_job;
    support::Report report;
    bool report_scanned = false;
    float reveal = 0.f;  // the ensō draws itself once when the launcher opens
    footer_layout::CachedCheckPulse cached_check_pulse;

    void GenerateReport() {
        if (report_job.valid()) return;
        logx::Flush();
        const auto& log = logx::Path();
        const support::Source source{paths::Root(), files::FromUtf8(log), mods::RecompVersion};
        report_job = std::async(std::launch::async, [source] { return support::Generate(source); });
    }
    void PollReport() {
        if (!report_scanned) {
            report_scanned = true; std::error_code ec;
            for (const auto& entry : fs::directory_iterator(paths::Root()/"reports",ec)) {
                if (entry.path().extension() != ".zip" || !entry.is_regular_file(ec)) continue;
                if (report.zip.empty() || entry.last_write_time(ec) > fs::last_write_time(report.zip,ec)) report.zip = entry.path();
            }
        }
        if (report_job.valid() && report_job.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            report = report_job.get();
    }

    void Message(std::string title, std::string text, bool close_after = false) {
        popup = Popup{std::move(title), std::move(text), {}, {}, close_after};
    }
    void Confirm(std::string title, std::string text, std::string label, std::function<void()> action) {
        popup = Popup{std::move(title), std::move(text), std::move(label), std::move(action), false};
    }
    void Dismiss() {
        if (!popup) return;
        const bool close = popup->close_after;
        popup.reset();
        if (close) result = 0;
    }
    void Accept() {
        if (!popup) return;
        auto action = popup->action;
        const bool close = popup->close_after;
        popup.reset();
        if (action) action();
        if (close) result = 0;
    }

    bool Busy() const { const auto view = mods->View(); return view.busy && !view.checking; }

    void Play() {
        if (options.configure_only) { Close(); return; }
        if (source.empty()) { ChangeGame(); return; }
        if (Busy()) {
            page = Page::Mods;
            Message(T("message.mod_busy"), T("message.mod_busy_play"));
            return;
        }
        if (const auto missing = MissingLanguagePack(settings); !missing.empty()) {
            page = Page::Mods;
            Message(T("message.pack_required"), missing);
            return;
        }
        std::string error;
        if (!Validate(settings, error)) { Message(T("message.check_settings"), error); return; }
        if (updater) updater->Cancel();
        Apply(settings, true);
        if (!source.empty()) { options.disc = source; options.game.clear(); }
        result = 1;
    }
    void Close() {
        if (updater) updater->Cancel();
        result = 0;
    }

    void ChangeGame(const fs::path& chosen = {}) {
        if (chosen.empty()) {
            if (page != Page::Game) game_return = page;
            game_error.clear();
            page = Page::Game;
            return;
        }
        if (preparing) return;
        game_error.clear();
        preparing = std::make_unique<GamePreparation>(chosen);
    }
    void ChooseGame(bool folder) {
        fs::path selected;
        if (!preparing && ChooseGameSource(selected, folder)) ChangeGame(selected);
    }
    void LeaveGame() {
        if (game_required || preparing) return;
        page = game_return == Page::Game ? Page::Play : game_return;
    }
    // Applies a finished selection on the interface thread.
    void PollGame() {
        if (!preparing || !preparing->Finished()) return;
        if (preparing->Ok()) {
            source = preparing->Path();
            game_required = false;
            page = game_return == Page::Game ? Page::Play : game_return;
        } else game_error = preparing->Error();
        preparing.reset();
    }
    fs::path ModGame() {
        setup::Source prepared; std::vector<uint8_t> executable; std::string error;
        if (source.empty() || !setup::PrepareGame(source, prepared, executable, error, true)) {
            Message(T("message.mod_install_failed"), error.empty() ? std::string(T("message.mod_needs_game")) : error);
            return {};
        }
        source = prepared.path;
        return prepared.path;
    }
    void InstallMod(const std::string& id) { if (const auto game = ModGame(); !game.empty()) mods->Install(id, game); }
    void InstallModFile(const fs::path& file) { if (const auto game = ModGame(); !game.empty()) mods->InstallFile(file, game); }
    void RemoveMod(const mods::Mod& mod) {
        const auto id = mod.id;
        Confirm(F("message.remove_title", {{"name", mod.name}}), T("message.remove_text"), T("mods.remove"),
                [this, id] { mods->Remove(id); });
    }
    void ToggleMod(const std::string& id) {
        for (const auto& installed : mods->View().installed)
            if (installed.mod.id == id) { mods->Enable(id, !installed.enabled); return; }
    }

    void RestoreVideo() {
        auto safe = settings;
#ifdef _WIN32
        safe.renderer = "d3d12";
#endif
        safe.display_mode = "borderless"; safe.width = safe.height = 0; safe.render_percent = 100;
        safe.aspect_ratio = "original"; safe.vsync = true;
        std::string error;
        try { if (!Write(paths::Config(), Merge(Disk(), safe), error)) throw std::runtime_error(error); }
        catch (const std::exception& e) { Message(T("message.video_failed"), e.what()); return; }
        settings = safe;
        Message(T("message.video_restored"), T("message.video_restored_text"));
    }
    void SetRenderer(const std::string& renderer) {
#ifdef _WIN32
        if(renderer!="d3d12" && renderer!="vulkan")return;
        if(settings.renderer==renderer)return;
        auto desired=settings;desired.renderer=renderer;
        std::string error;
        try { if(!Write(paths::Config(),Merge(Disk(),desired),error))throw std::runtime_error(error); }
        catch(const std::exception& e){Message(T("message.renderer_failed"),e.what());return;}
        settings=std::move(desired);
#endif
    }
    void MoveFolder(bool data, const fs::path& chosen = {}) {
        fs::path selected = chosen;
        if (selected.empty() && !ChooseFolder(selected, T(data ? "files.choose_data" : "files.choose_saves"))) return;
        std::string error;
        if (!paths::ChangeFolders(data ? selected : data_folder, data ? save_folder : selected, error)) {
            Message(T("message.move_failed"), error);
            return;
        }
        Message(T("message.move_scheduled"), T("message.move_scheduled_text"), true);
    }
    void OpenSaves() {
        const auto root = !options.save_dir.empty() ? fs::absolute(options.save_dir) :
                          options.portable_saves ? app::ExeDir() / "hdd" : paths::SaveRoot() / "hdd";
        std::error_code ec;
        if (fs::is_directory(root, ec)) OpenLocation(files::Utf8(root));
        else Message(T("message.no_saves"), F("message.no_saves_text", {{"path", files::Utf8(root)}}));
    }
    void SetCheckUpdates(bool enabled) {
        try {
            auto table = Disk();
            auto launcher = table["launcher"].as_table() ? *table["launcher"].as_table() : toml::table{};
            launcher.insert_or_assign("check_updates", enabled);
            table.insert_or_assign("launcher", launcher);
            std::string error;
            if (!Write(paths::Config(), table, error)) throw std::runtime_error(error);
            check_updates = enabled;
            if (enabled && updater) updater->Tick();
        } catch (const std::exception& e) { Message(T("message.updates_failed"), e.what()); }
    }
    void SetDiscordPresence(bool enabled) {
        try {
            auto table = Disk();
            auto launcher = table["launcher"].as_table() ? *table["launcher"].as_table() : toml::table{};
            launcher.insert_or_assign("discord_presence", enabled);
            table.insert_or_assign("launcher", launcher);
            std::string error;
            if (!Write(paths::Config(), table, error)) throw std::runtime_error(error);
            discord_presence = enabled;
            discord::SetState(discord::State::Launcher, options, enabled);
        } catch (const std::exception& e) { Message(T("message.discord_failed"), e.what()); }
    }
    // Opens the language list. Selecting a row previews the captions; Continue saves the choice.
    void OpenLanguage(bool required) {
        if (page != Page::Language) language_return = page;
        language_required = required;
        language_choice = text::Current();
        page = Page::Language;
    }
    void ChooseLanguage(const std::string& code) {
        if (!text::Supported(code)) return;
        language_choice = code;
        text::Select(code);
    }
    void MoveLanguage(int step) {
        const auto& list = text::Languages();
        if (list.empty()) return;
        int index = 0;
        for (size_t i = 0; i < list.size(); ++i) if (list[i].code == language_choice) index = int(i);
        index = std::clamp(index + step, 0, int(list.size()) - 1);
        ChooseLanguage(list[size_t(index)].code);
    }
    void ConfirmLanguage() {
        if (page != Page::Language) return;
        text::Select(language_choice);
        try {
            auto table = Disk();
            auto launcher = table["launcher"].as_table() ? *table["launcher"].as_table() : toml::table{};
            launcher.insert_or_assign("language", text::Current());
            table.insert_or_assign("launcher", launcher);
            std::string error;
            if (!Write(paths::Config(), table, error)) throw std::runtime_error(error);
            language_saved = text::Current();
        } catch (const std::exception& e) { Message(T("message.language_failed"), e.what()); }
        language_required = false;
        page = game_required ? Page::Game : language_return == Page::Language ? Page::Play : language_return;
    }
    void LeaveLanguage() {
        if (language_required || page != Page::Language) return;
        text::Select(language_saved.empty() ? system_language : language_saved);
        language_choice = text::Current();
        page = language_return == Page::Language ? Page::Play : language_return;
    }
    void StartUpdate() {
        if (!updater) return;
        if (Busy()) { page = Page::Mods; Message(T("message.mod_busy"), T("message.mod_busy_update")); return; }
        updater->Download();
    }
    // A found update is offered once in a popup: update now, or later from the footer or the next start.
    void OfferUpdate() {
        if (update_offered || !check_updates || !updater || popup || language_required || game_required || preparing) return;
        const auto view = updater->Get();
        if (view.state != update::State::Available) return;
        update_offered = true;
        Confirm(T("message.update_title"), F("message.update_text", {{"version", view.release.version}, {"current", mods::RecompVersion}}),
                T("play.update"), [this] { StartUpdate(); });
        popup->cancel = T("common.later");
    }
    void CheckGameUpdates() {
        if (!updater) return;
        const auto result = updater->Check(true);
        if (result == update::CheckResult::Cached)
            cached_check_pulse.Start(std::chrono::steady_clock::now());
    }

    bool Animating() const {
        if (cached_check_pulse.Active(std::chrono::steady_clock::now())) return true;
        if (reveal < 1.f) return true;
        if (report_job.valid()) return true;
        if (mods && mods->View().busy) return true;
        if (preparing) return true;
        if (updater) { const auto state = updater->Get().state; return state == update::State::Checking || state == update::State::Downloading; }
        return false;
    }
};

using art::Canvas;
using art::Rgb;
using art::Weight;
namespace color = art::color;

struct Hit { bool hovered = false, clicked = false; };
Hit Item(const char* id, const Canvas& c, float x0, float y0, float x1, float y1) {
    ImGui::SetCursorScreenPos(c.P(x0, y0));
    Hit hit;
    hit.clicked = ImGui::InvisibleButton(id, ImVec2(std::max(1.f, c.S(x1 - x0)), std::max(1.f, c.S(y1 - y0))));
    hit.hovered = ImGui::IsItemHovered();
    if (hit.hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    return hit;
}
bool Button(const char* id, const Canvas& c, float x0, float y0, float x1, float y1, const char* label, bool primary, bool enabled = true) {
    ImGui::BeginDisabled(!enabled);
    const auto hit = Item(id, c, x0, y0, x1, y1);
    ImGui::EndDisabled();
    const float alpha = enabled ? 1.f : 0.4f;
    if (primary) c.Rect(x0, y0, x1, y1, Rgb(hit.hovered ? 0xE8826C : color::Ember, alpha), 9);
    else {
        if (hit.hovered) c.Rect(x0, y0, x1, y1, Rgb(color::Slate), 9);
        c.Outline(x0, y0, x1, y1, Rgb(hit.hovered ? color::Dim : color::Line, alpha), 9);
    }
    const auto size = c.Measure(Weight::SemiBold, 19, label);
    c.Text(Weight::SemiBold, 19, (x0 + x1 - size.x) / 2, (y0 + y1 - size.y) / 2 - 0.5f, Rgb(primary ? color::Ink : color::Paper, alpha), label);
    return hit.clicked;
}
// Translated captions vary in length: buttons grow with their label.
float ButtonWidth(const Canvas& c, const char* label, float minimum = 84) {
    return std::max(minimum, c.Measure(Weight::SemiBold, 19, label).x + 36);
}
bool Link(const char* id, const Canvas& c, float x, float y, const char* label, bool right_aligned = false, bool enabled = true) {
    const auto size = c.Measure(Weight::Medium, 19, label);
    const float x0 = right_aligned ? x - size.x : x;
    ImGui::BeginDisabled(!enabled);
    const auto hit = Item(id, c, x0, y - 2, x0 + size.x, y + size.y + 2);
    ImGui::EndDisabled();
    c.Text(Weight::Medium, 19, x0, y, Rgb(enabled ? (hit.hovered ? 0xF09A84 : color::Ember) : color::Dim), label);
    return hit.clicked;
}

void UpdateSpinner(const Canvas& c, float x, float y) {
    const auto center = c.P(x, y);
    constexpr float radius = 7.f;
    c.list->AddCircle(center, c.S(radius), Rgb(color::Dim, 0.35f), 24, c.S(2.f));
    const float seconds = std::chrono::duration<float>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    const float start = std::fmod(seconds * 7.f, 6.2831853f);
    c.list->PathArcTo(center, c.S(radius), start, start + 4.0f, 20);
    c.list->PathStroke(Rgb(color::Ember), 0, c.S(2.3f));
}

void TopBar(Session& s, const Canvas& c, float W) {
    if (s.page == Page::Language) return;
    if (s.page == Page::Game || s.game_required) {
        const bool support = s.page == Page::Support;
        if (Link("Support##selection", c, W-40, 28, T(support ? "nav.choose_game" : "nav.support"), true))
            s.page = support ? Page::Game : Page::Support;
        return;
    }
    const Page pages[] = {Page::Play, Page::Mods, Page::Files, Page::Support, Page::About};
    const char* labels[] = {T("nav.play"), T("nav.mods"), T("nav.files"), T("nav.support"), T("nav.about")};
    float x = W - 40;
    float xs[5];
    for (int i = 4; i >= 0; --i) { x -= c.Measure(Weight::Medium, 19, labels[i]).x; xs[i] = x; x -= 34; }
    const bool updates = s.mods->View().updates;
    for (int i = 0; i < 5; ++i) {
        const float width = c.Measure(Weight::Medium, 19, labels[i]).x;
        const auto hit = Item(labels[i], c, xs[i] - 8, 18, xs[i] + width + 8, 56);
        if (hit.clicked) s.page = pages[i];
        const bool active = s.page == pages[i];
        c.Text(Weight::Medium, 19, xs[i], 28, Rgb(active || hit.hovered ? color::Paper : color::Mist), labels[i]);
        if (active) art::BrushDash(c, xs[i] - 1, 50, width + 2);
        if (pages[i] == Page::Mods && updates) c.list->AddCircleFilled(c.P(xs[i] + width + 6, 30), c.S(3), Rgb(color::Ember));
    }
}

void PlayPage(Session& s, const Canvas& c, float W, float H) {
    const footer_layout::Rows rows(H);
    const float footer = rows.divider;
    float content_footer=footer;
#ifdef _WIN32
    content_footer-=64;
#endif
    const ImVec2 center{W / 2, 96 + (content_footer - 96) / 2};
    const float R = std::clamp((content_footer - 130) * 0.40f, 90.f, 170.f);
    const float stroke = R * 0.2f;
    const auto view = s.updater ? s.updater->Get() : update::View{};
    const bool updating = view.state == update::State::Downloading || view.state == update::State::Ready;
    if (!updating) {
        const auto hit = Item("##play", c, center.x - R, center.y - R, center.x + R, center.y + R);
        if (hit.clicked) s.Play();
        art::Glow(c, center, R, hit.hovered ? 1.45f : 1.f);
        const float eased = 1.f - std::pow(1.f - std::clamp(s.reveal, 0.f, 1.f), 3.f);
        art::Enso(c, center, R, stroke, eased, 1.f);
        const char* label = T(options.configure_only ? "play.close" : "play.play");
        const float label_h = c.Measure(Weight::Medium, 54, label).y;
        c.TextCentered(Weight::Medium, 54, center.x + 4, center.y - label_h / 2 - 2, Rgb(color::Paper, hit.hovered ? 1.f : 0.94f), label);
    } else {
        const float value = view.total ? float(double(view.done) / double(view.total)) : 0.f;
        art::Enso(c, center, R, stroke, 1.f, 0.09f, 11, 44, false, color::Paper);
        art::Enso(c, center, R, stroke, view.state == update::State::Ready ? 1.f : std::clamp(value, 0.f, 1.f), 1.f);
        char percent[16];
        std::snprintf(percent, sizeof percent, "%d%%", int(std::clamp(value, 0.f, 1.f) * 100.f));
        c.TextCentered(Weight::Medium, 54, center.x + 4, center.y - 30, Rgb(color::Paper), view.state == update::State::Ready ? "100%" : percent);
        c.TextCentered(Weight::Regular, 18.5f, center.x + 4, center.y + 22, Rgb(color::Mist), F("play.updating", {{"version", view.release.version}}).c_str());
    }

#ifdef _WIN32
    const char* api_label=T("play.graphics_api");
    const float api_label_w=c.Measure(Weight::Medium,19,api_label).x, api_y=footer-48;
    const float api_x=W/2-(api_label_w+20+140+12+126)/2, d3d_x=api_x+api_label_w+20;
    c.Text(Weight::Medium,19,api_x,api_y+10,Rgb(color::Mist),api_label);
    if(Button("##renderer-d3d12",c,d3d_x,api_y,d3d_x+140,api_y+40,"Direct3D 12",s.settings.renderer=="d3d12",!updating))s.SetRenderer("d3d12");
    if(Button("##renderer-vulkan",c,d3d_x+152,api_y,d3d_x+278,api_y+40,"Vulkan",s.settings.renderer=="vulkan",!updating))s.SetRenderer("vulkan");
#endif
    c.Rule(40, W - 40, footer);
    const float y = rows.primary;
    const bool ready = !s.source.empty();
    c.list->AddCircleFilled(c.P(46, y + 9), c.S(4), Rgb(ready ? color::Moss : color::Warning));
    const char* status = T(ready ? "play.game_ready" : "play.no_game");
    c.Text(Weight::Medium, 19, 60, y, Rgb(color::Paper), status);
    const float detail_x = 60 + c.Measure(Weight::Medium, 19, status).x + 10;
    if (!ready && Link("Choose game##footer", c, detail_x, y, T("play.choose_game"))) s.ChangeGame();

    const float right = W - 40;
    const auto notes_label = F("play.release_notes", {{"version", mods::RecompVersion}});
    if (Link("Release notes##installed", c, 40, rows.secondary, notes_label.c_str()))
        OpenLocation(update::ReleaseNotesUrl(mods::RecompVersion));
    const bool can_check = s.updater && view.state != update::State::Checking &&
                           view.state != update::State::Downloading && view.state != update::State::Ready;
    if (can_check) {
        if (Link("Check game updates##footer", c, right, rows.secondary, T("common.check_updates"), true))
            s.CheckGameUpdates();
    }
    const bool cached_check_loading = can_check &&
        s.cached_check_pulse.Active(std::chrono::steady_clock::now());
    if (cached_check_loading) {
        UpdateSpinner(c, right - 11.f, y + 10.f);
    } else switch (view.state) {
        case update::State::Available: {
            const float button = ButtonWidth(c, T("play.update"), 96);
            if (Button("Update##footer", c, right - button, rows.button_top, right, rows.button_bottom, T("play.update"), true)) s.StartUpdate();
            c.TextRight(Weight::Regular, 19, right - button - 16, y, Rgb(color::Mist), F("play.update_available", {{"version", view.release.version}}).c_str());
            break;
        }
        case update::State::Downloading: {
            const float button = ButtonWidth(c, T("common.cancel"), 96);
            if (Button("Cancel##update", c, right - button, rows.button_top, right, rows.button_bottom, T("common.cancel"), false)) s.updater->Cancel();
            char done[32], total[32];
            std::snprintf(done, sizeof done, "%.1f", double(view.done) / 1048576.0);
            std::snprintf(total, sizeof total, "%.1f", double(view.total) / 1048576.0);
            const auto progress = F("play.download_progress", {{"done", done}, {"total", total}});
            c.TextRight(Weight::Regular, 19, right - button - 16, y, Rgb(color::Mist), view.total ? progress.c_str() : T("play.downloading"));
            break;
        }
        case update::State::Ready:
            c.TextRight(Weight::Regular, 19, right, y, Rgb(color::Mist), T("play.restarting"));
            break;
        case update::State::Checking:
            c.TextRight(Weight::Regular, 19, right - 27.f, y, Rgb(color::Dim), T("play.checking"));
            UpdateSpinner(c, right - 11.f, y + 10.f);
            break;
        case update::State::Current:
            c.TextRight(Weight::Regular, 19, right, y, Rgb(color::Dim), T("play.up_to_date"));
            break;
        case update::State::Failed: {
            const bool offered = !view.release.version.empty();
            if (Link("Release page##update", c, right, y, T("play.view_releases"), true))
                OpenLocation(view.release.page.empty() ? update::kReleasesPage : view.release.page);
            const float link = c.Measure(Weight::Medium, 19, T("play.view_releases")).x;
            if (offered && Link("Retry##update", c, right - link - 18, y, T("play.retry"), true)) s.StartUpdate();
            const float retry = offered ? c.Measure(Weight::Medium, 19, T("play.retry")).x + 18 : 0.f;
            c.TextRight(Weight::Regular, 19, right - link - retry - 18, y, Rgb(color::Warning),
                        T(offered ? "play.install_failed" : "play.check_failed"));
            if (ImGui::IsMouseHoveringRect(c.P(W / 2, y - 6), c.P(right, y + 24)) && !view.error.empty()) ImGui::SetTooltip("%s", view.error.c_str());
            break;
        }
        default: break;
    }
}

void Header(const Canvas& c, const char* title, const char* subtitle, float width) {
    c.Text(Weight::SemiBold, 36, 48, 104, Rgb(color::Paper), title);
    c.Text(Weight::Regular, 20, 48, 148, Rgb(color::Mist), subtitle, width - 96);
}

void ModsPage(Session& s, const Canvas& c, float W, float H) {
    const auto view = s.mods->View();
    Header(c, T("mods.title"), T("mods.subtitle"), W);
    const float right = W - 48;
    if (Link("Check for updates##mods", c, right, 112, T("common.check_updates"), true, !view.busy)) s.mods->Refresh();
    const float refresh = c.Measure(Weight::Medium, 19, T("common.check_updates")).x;
    if (Link("Install from file##mods", c, right - refresh - 28, 112, T("mods.install_file"), true, !view.busy)) {
        fs::path file;
        if (ChooseModArchive(file)) s.InstallModFile(file);
    }
    float top = 200;
    if (view.busy && !view.checking) {
        c.Text(Weight::Medium, 18.5f, 48, 176, Rgb(color::Paper), view.status.c_str());
        if (Link("Cancel##modop", c, right, 176, T("common.cancel"), true)) s.mods->Cancel();
        art::ProgressBar(c, 48, right, 196, view.total ? float(double(view.done) / double(view.total)) : 0.f);
        top = 216;
    } else if (!view.error.empty()) {
        c.Text(Weight::Regular, 18.5f, 48, 176, Rgb(color::Warning), c.Fit(Weight::Regular, 18.5f, right - 48, view.error).c_str());
        top = 208;
    }
    c.Rule(48, right, top);

    const auto rows = mods::DisplayMods(view);
    ImGui::SetCursorScreenPos(c.P(0, top + 1));
    ImGui::BeginChild("##mod-list", ImVec2(c.S(W), c.S(std::max(40.f, H - top - 24))), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    const Canvas list{ImGui::GetWindowDrawList(), c.scale, ImGui::GetCursorScreenPos()};
    float y = 0;
    for (const auto& mod : rows) {
        ImGui::PushID(mod.id.c_str());
        const mods::Installed* installed = nullptr;
        for (const auto& i : view.installed) if (i.mod.id == mod.id) installed = &i;
        const auto* release = mods::LatestCompatible(mod);
        const bool update = installed && release && mods::CompareVersions(release->version, installed->mod.version) > 0;
        const std::string update_label = update ? F("mods.update_to", {{"version", release->version}}) : std::string{};
        const float update_w = update ? ButtonWidth(list, update_label.c_str(), 148) : 0.f, install_w = ButtonWidth(list, T("mods.install"), 96);
        const float text_x = 48 + 76, actions = installed ? 38 + 24 + (update ? update_w + 12 : 0) : install_w + 24;
        const float wrap = right - text_x - actions - 24;
        const float summary_h = list.Measure(Weight::Regular, 19, mod.summary.c_str(), wrap).y;
        const float row_h = std::max(56.f + 44, 32 + summary_h + 28 + 44);
        const float ty = y + 22;
        list.Rect(48, ty, 104, ty + 56, Rgb(color::Slate), 12);
        const float icon_alpha = installed && installed->enabled ? 1.f : 0.45f;
        // A flag stays bright until its pack is installed and disabled.
        if (!art::LanguageFlag(list, {76, ty + 28}, 38, mod.language, installed && !installed->enabled ? 0.45f : 1.f)) art::Enso(list, {76, ty + 28}, 15, 5, 1.f, icon_alpha, 7, 10);
        list.Text(Weight::SemiBold, 22, text_x, ty + 2, Rgb(color::Paper), mod.name.c_str());
        list.Text(Weight::Regular, 19, text_x, ty + 32, Rgb(color::Mist), mod.summary.c_str(), wrap);
        std::string meta;
        if (installed) meta = F(installed->enabled ? "mods.version" : "mods.version_disabled", {{"version", installed->mod.version}});
        else meta = release ? F("mods.version_available", {{"version", release->version}}) : std::string(T("mods.unavailable"));
        if (!mod.authors.empty()) {
            std::string authors = mod.authors[0];
            for (size_t i = 1; i < mod.authors.size(); ++i) authors = F("mods.and", {{"first", authors}, {"second", mod.authors[i]}});
            meta = F("mods.by", {{"details", meta}, {"authors", authors}});
        }
        if (!mod.unsupported_reason.empty()) meta = mod.unsupported_reason;
        const float meta_y = ty + 32 + summary_h + 6;
        list.Text(Weight::Regular, 18, text_x, meta_y, Rgb(mod.unsupported_reason.empty() ? color::Dim : color::Warning), meta.c_str());
        if (installed) {
            const float meta_w = list.Measure(Weight::Regular, 18, meta.c_str()).x;
            const float remove_x = text_x + meta_w + 14;
            ImGui::BeginDisabled(view.busy);
            const auto remove = Item("##remove", list, remove_x, meta_y - 1, remove_x + list.Measure(Weight::Medium, 18, T("mods.remove")).x, meta_y + 17);
            ImGui::EndDisabled();
            list.Text(Weight::Medium, 18, remove_x, meta_y, Rgb(remove.hovered ? color::Paper : color::Mist, view.busy ? 0.5f : 1.f), T("mods.remove"));
            if (remove.clicked) s.RemoveMod(mod);
            ImGui::BeginDisabled(view.busy);
            const auto toggle = Item("##toggle", list, right - 38, ty + 6, right, ty + 28);
            ImGui::EndDisabled();
            art::Toggle(list, right - 38, ty + 6, installed->enabled, toggle.hovered);
            if (toggle.clicked) s.ToggleMod(mod.id);
            if (update && Button("##update", list, right - 38 - 24 - update_w, ty, right - 38 - 24, ty + 34, update_label.c_str(), false, !view.busy))
                s.InstallMod(mod.id);
        } else if (Button("##install", list, right - install_w, ty, right, ty + 34, T("mods.install"), true, release && !view.busy && mod.unsupported_reason.empty())) {
            s.InstallMod(mod.id);
        }
        y += row_h;
        list.Rule(48, right, y);
        ImGui::PopID();
    }
    const char* empty = T(rows.empty() ? "mods.catalog_unavailable" : "mods.more_soon");
    list.Text(Weight::Regular, 18.5f, 48, y + 24, Rgb(color::Dim), empty, right - 48);
    ImGui::SetCursorScreenPos(list.P(0, y + 64));
    ImGui::Dummy(ImVec2(1, 1));
    ImGui::EndChild();
}

void FilesPage(Session& s, const Canvas& parent, float W, float H) {
    Header(parent, T("files.title"), T("files.subtitle"), W);
    ImGui::SetCursorScreenPos(parent.P(0, 200));
    ImGui::BeginChild("##files-list", ImVec2(parent.S(W), parent.S(std::max(40.f, H - 224))),
                      ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    const Canvas c{ImGui::GetWindowDrawList(), parent.scale, ImGui::GetCursorScreenPos()};
    const char* labels[] = {T("files.game"), T("files.saves"), T("files.data"), T("files.reports"), T("files.video"),
                            T("files.updates"), T("files.discord"), T("files.language")};
    float label_w = 0;
    for (const char* label : labels) label_w = std::max(label_w, c.Measure(Weight::SemiBold, 20, label).x);
    const float right = W - 48, label_x = 48, value_x = std::max(170.f, label_x + label_w + 28);
    const bool busy = s.mods->View().busy;
    const bool locked = paths::DataFolderOverridden() || busy;
    const bool saves_locked = locked || !options.save_dir.empty() || options.portable_saves;
    float y = 0;
    // Buttons sit right-aligned with a 12-pixel gap; the value text fits the space left of them.
    auto row = [&](const char* label, const std::string& value, std::initializer_list<const char*> buttons) {
        c.Rule(48, right, y);
        c.Text(Weight::SemiBold, 20, label_x, y + 25, Rgb(color::Paper), label);
        float used = 16;
        for (const char* button : buttons) used += ButtonWidth(c, button) + 12;
        c.Text(Weight::Regular, 19, value_x, y + 25, Rgb(color::Mist), c.Fit(Weight::Regular, 19, right - value_x - used, value).c_str());
    };
    // Draws the buttons of the current row from the right edge; returns which one was clicked (-1 for none).
    auto buttons = [&](std::initializer_list<std::pair<const char*, bool>> list, const char* id) {
        float x1 = right;
        int index = 0, clicked = -1;
        for (const auto& [label, enabled] : list) {
            const float width = ButtonWidth(c, label);
            ImGui::PushID(index);
            if (Button(id, c, x1 - width, y + 16, x1, y + 50, label, false, enabled)) clicked = index;
            ImGui::PopID();
            x1 -= width + 12;
            ++index;
        }
        return clicked;
    };
    row(labels[0], s.source.empty() ? std::string(T("play.no_game")) : files::Utf8(s.source), {T("files.change")});
    if (buttons({{T("files.change"), !busy}}, "##game") == 0) s.ChangeGame();
    y += 60;
    row(labels[1], files::Utf8(s.save_folder), {T("files.move"), T("files.open")});
    switch (buttons({{T("files.move"), !saves_locked}, {T("files.open"), true}}, "##saves")) {
        case 0: s.MoveFolder(false); break;
        case 1: s.OpenSaves(); break;
        default: break;
    }
    y += 60;
    row(labels[2], files::Utf8(s.data_folder), {T("files.move"), T("files.open")});
    switch (buttons({{T("files.move"), !locked}, {T("files.open"), true}}, "##data")) {
        case 0: s.MoveFolder(true); break;
        case 1: OpenLocation(files::Utf8(paths::Root())); break;
        default: break;
    }
    y += 60;
    row(labels[3], T("files.reports_detail"), {T("files.support")});
    if (buttons({{T("files.support"), true}}, "##reports") == 0) s.page = Page::Support;
    y += 60;
    row(labels[4], T("files.video_detail"), {T("files.restore")});
    if (buttons({{T("files.restore"), true}}, "##video") == 0) s.RestoreVideo();
    y += 60;
    row(labels[7], F("files.language_detail", {{"language", LanguageName(text::Current())}}), {T("files.change")});
    if (buttons({{T("files.change"), true}}, "##language") == 0) s.OpenLanguage(false);
    y += 60;
    c.Rule(48, right, y);
    c.Text(Weight::SemiBold, 20, label_x, y + 25, Rgb(color::Paper), labels[5]);
    c.Text(Weight::Regular, 19, value_x, y + 25, Rgb(color::Mist), c.Fit(Weight::Regular, 19, right - value_x - 56, T("files.updates_detail")).c_str());
    const auto toggle = Item("##check-updates", c, right - 38, y + 22, right, y + 44);
    art::Toggle(c, right - 38, y + 22, s.check_updates, toggle.hovered);
    if (toggle.clicked) s.SetCheckUpdates(!s.check_updates);
    y += 60;
    c.Rule(48, right, y);
    c.Text(Weight::SemiBold, 20, label_x, y + 25, Rgb(color::Paper), labels[6]);
    c.Text(Weight::Regular, 19, value_x, y + 25, Rgb(color::Mist), c.Fit(Weight::Regular, 19, right - value_x - 56, T("files.discord_detail")).c_str());
    const auto discord_toggle = Item("##discord-presence", c, right - 38, y + 22, right, y + 44);
    art::Toggle(c, right - 38, y + 22, s.discord_presence, discord_toggle.hovered);
    if (discord_toggle.clicked) s.SetDiscordPresence(!s.discord_presence);
    ImGui::SetCursorScreenPos(c.P(0, y + 54));
    ImGui::Dummy(ImVec2(1, 1));
    ImGui::EndChild();
}

void SupportPage(Session& s, const Canvas& c, float W, float) {
    Header(c, T("support.title"), T("support.subtitle"), W);
    const float right = W-48;
    const bool busy = s.report_job.valid();
    c.Rule(48,right,200);
    c.Text(Weight::SemiBold,22,48,224,Rgb(color::Paper),T("support.report"));
    // Translations can wrap to more lines; each block starts below the previous one.
    float y=260;
    c.Text(Weight::Regular,19,48,y,Rgb(color::Mist),T("support.contents"),right-48);
    y+=c.Measure(Weight::Regular,19,T("support.contents"),right-48).y+14;
    c.Text(Weight::Regular,19,48,y,Rgb(color::Mist),T("support.privacy"),right-48);
    y=std::max(382.f,y+c.Measure(Weight::Regular,19,T("support.privacy"),right-48).y+24);
    const float generate_w=std::max({196.f,ButtonWidth(c,T("support.generate")),ButtonWidth(c,T("support.generating"))});
    const float open_w=ButtonWidth(c,T("support.open_reports"),172);
    if(Button("Generate##support",c,48,y,48+generate_w,y+42,T(busy?"support.generating":"support.generate"),true,!busy)) s.GenerateReport();
    if(Button("Folder##support",c,64+generate_w,y,64+generate_w+open_w,y+42,T("support.open_reports"),false)) {
        const auto folder=paths::Root()/"reports";std::error_code ec;fs::create_directories(folder,ec);
        OpenLocation(files::Utf8(folder));
    }
    y+=68;
    c.Rule(48,right,y);
    if(!s.report.error.empty()) c.Text(Weight::Regular,19,48,y+20,Rgb(color::Warning),s.report.error.c_str(),right-48);
    else if(!s.report.zip.empty()) {
        const float show=c.Measure(Weight::Medium,19,T("support.show_zip")).x;
        c.Text(Weight::Medium,19,48,y+20,Rgb(color::Paper),c.Fit(Weight::Medium,19,right-48-show-40,files::Utf8(s.report.zip.filename())).c_str());
        if(Link("Show##support",c,right,y+20,T("support.show_zip"),true)) support::OpenReport(s.report.zip);
    } else c.Text(Weight::Regular,19,48,y+20,Rgb(color::Mist),T("support.no_report"));
    const float bug_w=ButtonWidth(c,T("support.report_bug"),166);
    c.Text(Weight::Regular,19,48,y+68,Rgb(color::Mist),T("support.describe"),right-48-bug_w-24);
    if(Button("Report##support",c,right-bug_w,y+62,right,y+104,T("support.report_bug"),false)) {
#ifdef _WIN32
        constexpr std::string_view platform="Windows";
#else
        constexpr std::string_view platform="Linux";
#endif
        // Crash form when a dump is included; the clipboard copy covers the graphics/audio forms.
        const auto kind=s.report.dump_included?support::IssueKind::Crash:support::IssueKind::Bug;
        const auto environment=support::IssueEnvironment(s.settings,mods::RecompVersion,platform);
        ImGui::SetClipboardText(environment.c_str());
        OpenLocation(support::IssueLink(kind,environment));
    }
}

void AboutPage(Session&, const Canvas& c, float W, float) {
    Header(c, T("about.title"), F("about.version", {{"version", mods::RecompVersion}}).c_str(), W);
    const float right = W - 48, wrap = std::min(720.f, right - 48);
    if (Link("Licenses##about", c, right, 112, T("about.licenses"), true)) OpenLocation(files::Utf8(app::ExeDir() / "licenses"));
    const float licenses = c.Measure(Weight::Medium, 19, T("about.licenses")).x;
    if (Link("Project page##about", c, right - licenses - 28, 112, T("about.project"), true)) OpenLocation("https://github.com/OAleex/OpenShao");
    c.Rule(48, right, 200);
    struct Paragraph { Weight weight; float size; uint32_t rgb; const char* text; };
    const Paragraph paragraphs[] = {
        {Weight::Regular, 20, color::Paper, T("about.summary")},
        {Weight::Regular, 19, color::Mist, T("about.license")},
    };
    float y = 224;
    for (const auto& paragraph : paragraphs) {
        c.Text(paragraph.weight, paragraph.size, 48, y, Rgb(paragraph.rgb), paragraph.text, wrap);
        y += c.Measure(paragraph.weight, paragraph.size, paragraph.text, wrap).y + 16;
    }
    // Keep this list in step with licenses/ and the package contents.
    struct Component { const char* name; const char* license; };
    const Component components[] = {
        {"XboxXBERecomp", "MIT"}, {"SDL 3", "zlib"}, {"Dear ImGui", "MIT"}, {"toml++", "MIT"},
        {"Lua 5.4", "MIT"}, {"miniz", "MIT"}, {"XZ Embedded", "0BSD"}, {"Vulkan-Headers", "Apache-2.0 or MIT"},
        {"glslang", T("about.bsd_others")}, {"Hanken Grotesk", "SIL Open Font License 1.1"},
        {"discord-rpc and RapidJSON", "MIT"}, {T("about.keyboard"), "GPL-2.0-or-later"},
    };
    constexpr int count = int(sizeof(components) / sizeof(components[0]));
    const int rows = (count + 1) / 2;
    const float column = (right - 48) / 2;
    float name_w[2] = {};
    for (int i = 0; i < count; ++i) name_w[i / rows] = std::max(name_w[i / rows], c.Measure(Weight::Medium, 18, components[i].name).x);
    y += 6;
    for (int i = 0; i < count; ++i) {
        const int side = i / rows;
        const float x = 48 + float(side) * column, row_y = y + float(i % rows) * 30;
        c.Text(Weight::Medium, 18, x, row_y, Rgb(color::Paper), components[i].name);
        c.Text(Weight::Regular, 18, x + name_w[side] + 24, row_y, Rgb(color::Mist),
               c.Fit(Weight::Regular, 18, column - name_w[side] - 48, components[i].license).c_str());
    }
}

void PopupOverlay(Session& s, const Canvas& c, float W, float H) {
    if (!s.popup) return;
    c.Rect(0, 0, W, H, Rgb(0x15171A, 0.62f));
    const float width = 440;
    const float x0 = (W - width) / 2;
    const float title_h = c.Measure(Weight::SemiBold, 22, s.popup->title.c_str(), width - 56).y;
    const float text_h = c.Measure(Weight::Regular, 19, s.popup->text.c_str(), width - 56).y;
    const float height = 28 + title_h + 14 + text_h + 28 + 36 + 24;
    const float y0 = (H - height) / 2;
    c.Rect(x0, y0, x0 + width, y0 + height, Rgb(color::Slate), 14);
    c.Outline(x0, y0, x0 + width, y0 + height, Rgb(color::Line), 14);
    c.Text(Weight::SemiBold, 22, x0 + 28, y0 + 28, Rgb(color::Paper), s.popup->title.c_str(), width - 56);
    c.Text(Weight::Regular, 19, x0 + 28, y0 + 28 + title_h + 14, Rgb(color::Mist), s.popup->text.c_str(), width - 56);
    const float by = y0 + height - 24 - 36, bx = x0 + width - 28;
    if (s.popup->confirm.empty()) {
        const float ok_w = ButtonWidth(c, T("common.ok"), 96);
        if (Button("OK##popup", c, bx - ok_w, by, bx, by + 36, T("common.ok"), true)) s.Dismiss();
    } else {
        const char* cancel = s.popup->cancel.empty() ? T("common.cancel") : s.popup->cancel.c_str();
        const float confirm_w = ButtonWidth(c, s.popup->confirm.c_str(), 96), cancel_w = ButtonWidth(c, cancel, 96);
        if (Button("Confirm##popup", c, bx - confirm_w, by, bx, by + 36, s.popup->confirm.c_str(), true)) s.Accept();
        else if (Button("Cancel##popup", c, bx - confirm_w - 12 - cancel_w, by, bx - confirm_w - 12, by + 36, cancel, false)) s.Dismiss();
    }
}

void GamePage(Session& s, const Canvas& c, float W, float H) {
    const float right = W - 48;
    Header(c, T(s.game_required ? "game.choose_title" : "game.change_title"), T("game.subtitle"), W);
    if (!s.game_required && Link("Back##game", c, right, 112, T("common.back"), true, !s.preparing)) s.LeaveGame();
    float y = 200;
    if (!s.game_error.empty()) {
        c.Text(Weight::Regular, 18.5f, 48, 172, Rgb(color::Warning), c.Fit(Weight::Regular, 18.5f, right - 48, s.game_error).c_str());
        y = 208;
    }
    if (s.preparing) {
        const bool importing = s.preparing->Importing();
        c.Rule(48, right, y);
        const char* status = T(s.preparing->Cancelling() ? "game.cancelling" : importing ? "game.importing" : "game.checking");
        c.Text(Weight::Medium, 20, 48, y + 30, Rgb(color::Paper), status);
        if (importing && !s.preparing->Cancelling() && Link("Cancel##gameimport", c, right, y + 30, T("common.cancel"), true)) s.preparing->Cancel();
        art::ProgressBar(c, 48, right, y + 70, importing ? s.preparing->Progress() : 0.f);
        return;
    }
    struct Choice { const char* id; const char* title; const char* text; const char* action; bool folder; };
    const Choice choices[] = {
        {"##game-file", T("game.file"), T("game.file_detail"), T("game.choose_file"), false},
        {"##game-folder", T("game.folder"), T("game.folder_detail"), T("game.choose_folder"), true},
    };
    for (const auto& choice : choices) {
        c.Rule(48, right, y);
        const float text_x = 124, button_w = c.Measure(Weight::SemiBold, 19, choice.action).x + 40;
        const float text_w = right - text_x - button_w - 24;
        // Long translations wrap to a second line; the row grows to keep its bottom margin.
        const float row_h = std::max(104.f, 56 + c.Measure(Weight::Regular, 19, choice.text, text_w).y + 22);
        const auto hit = Item(choice.id, c, 48, y + 1, right, y + row_h);
        if (hit.hovered) c.Rect(36, y + 8, right + 12, y + row_h - 8, Rgb(color::Slate, 0.7f), 12);
        if (hit.clicked) s.ChooseGame(choice.folder);
        c.Rect(48, y + 24, 104, y + 80, Rgb(color::Slate), 12);
        const ImU32 ink = Rgb(color::Ember, hit.hovered ? 1.f : 0.85f);
        if (choice.folder) {
            c.Rect(62, y + 39, 76, y + 46, ink, 2);
            c.Rect(62, y + 43, 90, y + 64, ink, 3);
        } else {
            c.list->AddCircleFilled(c.P(76, y + 52), c.S(16), ink, 40);
            c.list->AddCircleFilled(c.P(76, y + 52), c.S(4.5f), Rgb(color::Slate), 24);
            c.list->AddCircle(c.P(76, y + 52), c.S(9), Rgb(color::Slate, 0.35f), 32, c.S(1));
        }
        c.Text(Weight::SemiBold, 22, text_x, y + 24, Rgb(color::Paper), choice.title);
        c.Text(Weight::Regular, 19, text_x, y + 56, Rgb(color::Mist), choice.text, text_w);
        const float by = y + (row_h - 38) / 2;
        if (!choice.folder) c.Rect(right - button_w, by, right, by + 38, Rgb(hit.hovered ? 0xE8826C : color::Ember), 9);
        else c.Outline(right - button_w, by, right, by + 38, Rgb(hit.hovered ? color::Dim : color::Line), 9);
        const auto label = c.Measure(Weight::SemiBold, 19, choice.action);
        c.Text(Weight::SemiBold, 19, right - button_w + (button_w - label.x) / 2, by + (38 - label.y) / 2 - 0.5f,
               Rgb(choice.folder ? color::Paper : color::Ink), choice.action);
        y += row_h;
    }
    c.Rule(48, right, y);
    (void)H;
}

void LanguagePage(Session& s, const Canvas& c, float W, float H) {
    const float right = W - 48, left = 48;
    c.Text(Weight::SemiBold, 36, left, 72, Rgb(color::Paper), T("language.title"), right - left - 120);
    const float title_h = c.Measure(Weight::SemiBold, 36, T("language.title"), right - left - 120).y;
    if (!s.language_required && Link("Back##language", c, right, 84, T("language.back"), true)) { s.LeaveLanguage(); return; }
    const float subtitle_y = 72 + title_h + 8;
    c.Text(Weight::Regular, 20, left, subtitle_y, Rgb(color::Mist), T("language.subtitle"), right - left);
    const float top = subtitle_y + c.Measure(Weight::Regular, 20, T("language.subtitle"), right - left).y + 24;
    c.Rule(left, right, top);

    // Rows fill two columns on a wide window. More languages scroll inside the list.
    const auto& languages = text::Languages();
    const int columns = right - left >= 640 ? 2 : 1;
    s.language_columns = columns;
    const float gap = 14, row_h = 72, column_w = (right - left - gap * float(columns - 1)) / float(columns);
    const float footer_top = H - 96;
    ImGui::SetCursorScreenPos(c.P(0, top + 1));
    ImGui::BeginChild("##language-list", ImVec2(c.S(W), c.S(std::max(80.f, footer_top - top - 12))), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoBackground);
    const Canvas list{ImGui::GetWindowDrawList(), c.scale, ImGui::GetCursorScreenPos()};
    for (size_t i = 0; i < languages.size(); ++i) {
        const auto& language = languages[i];
        const int column = int(i) % columns, line = int(i) / columns;
        const float x0 = left + float(column) * (column_w + gap), y0 = 16 + float(line) * (row_h + gap);
        const float x1 = x0 + column_w, y1 = y0 + row_h;
        ImGui::PushID(language.code.c_str());
        const auto hit = Item("##language", list, x0, y0, x1, y1);
        ImGui::PopID();
        if (hit.clicked) s.ChooseLanguage(language.code);
        const bool selected = language.code == s.language_choice;
        if (selected || hit.hovered) list.Rect(x0, y0, x1, y1, Rgb(color::Slate, selected ? 1.f : 0.7f), 12);
        list.Outline(x0, y0, x1, y1, Rgb(selected ? color::Ember : color::Line), 12, selected ? 1.8f : 1.2f);
        if (!art::LanguageFlag(list, {x0 + 42, y0 + row_h / 2}, 40, language.code))
            art::Enso(list, {x0 + 42, y0 + row_h / 2}, 14, 4.5f, 1.f, 1.f, 7, 10);
        const float text_x = x0 + 80;
        list.Text(Weight::SemiBold, 22, text_x, y0 + 23, Rgb(color::Paper), list.Fit(Weight::SemiBold, 22, x1 - 44 - text_x, language.name).c_str());
        const ImVec2 mark = list.P(x1 - 24, y0 + row_h / 2);
        list.list->AddCircle(mark, list.S(8), Rgb(selected ? color::Ember : color::Dim), 24, list.S(1.6f));
        if (selected) list.list->AddCircleFilled(mark, list.S(4.2f), Rgb(color::Ember), 20);
        if (selected && ImGui::IsWindowAppearing()) ImGui::SetScrollHereY(0.5f);
    }
    const int lines = (int(languages.size()) + columns - 1) / columns;
    ImGui::SetCursorScreenPos(list.P(0, 16 + float(lines) * (row_h + gap)));
    ImGui::Dummy(ImVec2(1, 1));
    ImGui::EndChild();

    c.Rule(left, right, footer_top);
    const float button_w = ButtonWidth(c, T("language.continue"), 148);
    if (Button("Continue##language", c, right - button_w, footer_top + 24, right, footer_top + 66, T("language.continue"), true))
        s.ConfirmLanguage();
}

void Draw(Session& s, float scale) {
    s.PollGame();
    s.PollReport();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float W = viewport->Size.x / scale, H = viewport->Size.y / scale;
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::Begin("##launcher", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);
    const Canvas c{ImGui::GetWindowDrawList(), scale, viewport->Pos};
    const float footer = H - 76;
    art::Background(c, W, H, s.page == Page::Play ? ImVec2(W / 2, 96 + (footer - 96) / 2) : ImVec2(W / 2, -80));
    const bool modal = s.popup.has_value();
    ImGui::BeginDisabled(modal);
    TopBar(s, c, W);
    switch (s.page) {
        case Page::Play: PlayPage(s, c, W, H); break;
        case Page::Mods: ModsPage(s, c, W, H); break;
        case Page::Files: FilesPage(s, c, W, H); break;
        case Page::Support: SupportPage(s, c, W, H); break;
        case Page::About: AboutPage(s, c, W, H); break;
        case Page::Game: GamePage(s, c, W, H); break;
        case Page::Language: LanguagePage(s, c, W, H); break;
    }
    ImGui::EndDisabled();
    ImGui::End();
    if (modal) {
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowFocus();
        ImGui::Begin("##popup", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                         ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);
        PopupOverlay(s, Canvas{ImGui::GetWindowDrawList(), scale, viewport->Pos}, W, H);
        ImGui::End();
    }

    const ImGuiIO& io = ImGui::GetIO();
    s.reveal = std::min(1.f, s.reveal + io.DeltaTime / 0.9f);
    if (modal && s.popup) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) s.Dismiss();
        else if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
            if (s.popup->confirm.empty()) s.Dismiss(); else s.Accept();
        }
    } else if (!modal && s.page == Page::Language) {
        const auto pressed = [](ImGuiKey key) { return ImGui::IsKeyPressed(key, true); };
        if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) s.ConfirmLanguage();
        else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) s.LeaveLanguage();
        else if (pressed(ImGuiKey_LeftArrow)) s.MoveLanguage(-1);
        else if (pressed(ImGuiKey_RightArrow)) s.MoveLanguage(1);
        else if (pressed(ImGuiKey_UpArrow)) s.MoveLanguage(-s.language_columns);
        else if (pressed(ImGuiKey_DownArrow)) s.MoveLanguage(s.language_columns);
    } else if (!modal && !io.NavVisible) {
        if (s.page == Page::Play && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))) s.Play();
        else if (s.page == Page::Game && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) s.LeaveGame();
        else if (s.page != Page::Play && s.page != Page::Game && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) s.page = s.game_required ? Page::Game : Page::Play;
    }
}

// A, B and the shoulder buttons work in the launcher. The game configures controllers again when it starts.
class Gamepads {
public:
    Gamepads() { active_ = SDL_InitSubSystem(SDL_INIT_GAMEPAD); }
    ~Gamepads() {
        for (auto* pad : pads_) SDL_CloseGamepad(pad);
        if (active_) SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    }
    void Poll(Session& s) {
        if (!active_) return;
        SDL_UpdateGamepads();
        int count = 0;
        if (SDL_JoystickID* ids = SDL_GetGamepads(&count)) {
            for (int i = 0; i < count; ++i)
                if (std::none_of(pads_.begin(), pads_.end(), [&](SDL_Gamepad* pad) { return SDL_GetGamepadID(pad) == ids[i]; }))
                    if (SDL_Gamepad* pad = SDL_OpenGamepad(ids[i])) pads_.push_back(pad);
            SDL_free(ids);
        }
        constexpr int count_buttons = 8;
        bool down[count_buttons] = {};
        const SDL_GamepadButton buttons[count_buttons] = {SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
            SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT,
            SDL_GAMEPAD_BUTTON_DPAD_RIGHT};
        for (auto* pad : pads_)
            for (int b = 0; b < count_buttons; ++b) down[b] |= SDL_GetGamepadButton(pad, buttons[b]);
        bool pressed[count_buttons];
        for (int b = 0; b < count_buttons; ++b) { pressed[b] = down[b] && !previous_[b]; previous_[b] = down[b]; }
        if (s.popup) {
            if (pressed[0]) { if (s.popup->confirm.empty()) s.Dismiss(); else s.Accept(); }
            else if (pressed[1]) s.Dismiss();
            return;
        }
        if (s.page == Page::Language) {
            if (pressed[0]) s.ConfirmLanguage();
            else if (pressed[1]) s.LeaveLanguage();
            else if (pressed[4]) s.MoveLanguage(-s.language_columns);
            else if (pressed[5]) s.MoveLanguage(s.language_columns);
            else if (pressed[6]) s.MoveLanguage(-1);
            else if (pressed[7]) s.MoveLanguage(1);
            return;
        }
        if (s.page == Page::Game) {
            if (pressed[2] || pressed[3]) { s.page = Page::Support; return; }
            if (pressed[0]) s.ChooseGame(false);
            if (pressed[1]) s.LeaveGame();
            return;
        }
        if (s.game_required) {
            if (pressed[1] || pressed[2] || pressed[3]) s.page = Page::Game;
            else if (pressed[0] && s.page == Page::Support) s.GenerateReport();
            return;
        }
        if (pressed[0] && s.page == Page::Play) s.Play();
        else if (pressed[0] && s.page == Page::Support) s.GenerateReport();
        if (pressed[1]) s.page = Page::Play;
        if (pressed[2] || pressed[3]) s.page = Page((int(s.page) + (pressed[3] ? 1 : 4)) % 5);
    }
private:
    bool active_ = false;
    std::vector<SDL_Gamepad*> pads_;
    bool previous_[8] = {};
};

// --launcher-test: windowless; tests append to commands.txt and read state.json.
class Automation {
public:
    explicit Automation(fs::path folder) : folder_(std::move(folder)) { fs::create_directories(folder_); }
    void Run(Session& s) {
        std::ifstream file(folder_ / "commands.txt", std::ios::binary);
        if (!file) return;
        file.seekg(std::streamoff(offset_));
        std::string line;
        while (std::getline(file, line)) {
            if (file.eof()) break;  // an unfinished line is read again next frame
            offset_ += line.size() + 1;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            Execute(s, line);
        }
    }
    void Publish(const Session& s) {
        std::ostringstream out;
        const auto view = s.mods->View();
        out << "{\"page\":\"" << PageName(s.page) << "\",\"result\":" << s.result << ",\"source\":\"" << Escape(files::Utf8(s.source))
            << "\",\"popup\":";
        if (s.popup) out << "{\"title\":\"" << Escape(s.popup->title) << "\",\"text\":\"" << Escape(s.popup->text) << "\"}";
        else out << "null";
        out << ",\"renderer\":\"" << s.settings.renderer << "\",\"renderer_selectable\":"
#ifdef _WIN32
            << "true"
#else
            << "false"
#endif
            << ",\"mods_busy\":" << (view.busy ? "true" : "false") << ",\"mods_error\":\"" << Escape(view.error) << "\",\"mods\":[";
        bool first = true;
        for (const auto& mod : mods::DisplayMods(view)) {
            const mods::Installed* installed = nullptr;
            for (const auto& i : view.installed) if (i.mod.id == mod.id) installed = &i;
            out << (first ? "" : ",") << "{\"id\":\"" << Escape(mod.id) << "\",\"installed\":" << (installed ? "true" : "false")
                << ",\"enabled\":" << (installed && installed->enabled ? "true" : "false") << "}";
            first = false;
        }
        out << "],\"check_updates\":" << (s.check_updates ? "true" : "false") << ",\"game_busy\":" << (s.preparing ? "true" : "false")
            << ",\"discord_presence\":" << (s.discord_presence ? "true" : "false")
            << ",\"game_error\":\"" << Escape(s.game_error) << "\",\"report_busy\":" << (s.report_job.valid()?"true":"false")
            << ",\"report_path\":\"" << Escape(files::Utf8(s.report.zip)) << "\",\"report_error\":\"" << Escape(s.report.error) << "\""
            << ",\"launcher_language\":\"" << Escape(text::Current()) << "\",\"system_language\":\"" << Escape(s.system_language)
            << "\",\"language_required\":" << (s.language_required ? "true" : "false") << ",\"play_label\":\"" << Escape(T("play.play")) << "\"}";
        const auto text = out.str();
        if (text == last_) return;
        // A test that is reading the file can block the replacement; the next frame tries again.
        try { files::AtomicWrite(folder_ / "state.json", text); last_ = text; }
        catch (const std::exception&) {}
    }
private:
    static std::string Escape(const std::string& text) {
        std::string out;
        for (const char ch : text) {
            if (ch == '"' || ch == '\\') { out += '\\'; out += ch; }
            else if (ch == '\n') out += "\\n";
            else if (static_cast<unsigned char>(ch) < 32) out += ' ';
            else out += ch;
        }
        return out;
    }
    void Execute(Session& s, const std::string& line) {
        const auto space = line.find(' ');
        const auto command = line.substr(0, space);
        const auto argument = space == line.npos ? std::string{} : line.substr(space + 1);
        LOGI("launcher test: %s", line.c_str());
        if (command == "page") {
            if (argument == "support") s.page = Page::Support;
            else if (!s.game_required)
                for (Page page : {Page::Play, Page::Mods, Page::Files, Page::About}) if (argument == PageName(page)) s.page = page;
        }
        else if (command == "generate-report") s.GenerateReport();
        else if (command == "play") s.Play();
        else if (command == "close") s.Close();
        else if (command == "confirm") s.Accept();
        else if (command == "dismiss") s.Dismiss();
        else if (command == "choose-game") s.ChangeGame(argument.empty() ? fs::path{} : files::FromUtf8(argument));
        else if (command == "game-back") s.LeaveGame();
        else if (command == "restore-video") s.RestoreVideo();
        else if (command == "renderer") s.SetRenderer(argument);
        else if (command == "check-updates") s.SetCheckUpdates(argument == "on");
        else if (command == "discord-presence") s.SetDiscordPresence(argument == "on");
        else if (command == "mods-refresh") s.mods->Refresh();
        else if (command == "mod-install") s.InstallMod(argument);
        else if (command == "mod-install-file") s.InstallModFile(files::FromUtf8(argument));
        else if (command == "mod-toggle") s.ToggleMod(argument);
        else if (command == "language-page") s.OpenLanguage(false);
        else if (command == "language") s.ChooseLanguage(argument);
        else if (command == "language-confirm") s.ConfirmLanguage();
        else if (command == "language-back") s.LeaveLanguage();
        else if (command == "mod-remove") {
            for (const auto& mod : mods::DisplayMods(s.mods->View())) if (mod.id == argument) s.RemoveMod(mod);
        }
        else LOGE("launcher test: unknown command %s", line.c_str());
    }
    fs::path folder_;
    size_t offset_ = 0;
    std::string last_;
};

bool CheckUpdatesEnabled() {
    try { return Disk()["launcher"]["check_updates"].value_or(true); } catch (...) { return true; }
}

bool RunAutomated(Session& s) {
    s.mods = std::make_unique<mods::Browser>(paths::Mods());
    if (!paths::MoveWarning().empty()) s.Message(T("message.folder_move"), paths::MoveWarning());
    Automation automation(options.launcher_test);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1040, 640);
    // The real renderers accept large meshes; the brush enso needs more than 64K vertices.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    art::Prepare(1.f);
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    s.reveal = 1.f;
    while (s.result < 0) {
        io.DeltaTime = 1.f / 60.f;
        ImGui::NewFrame();
        automation.Run(s);
        Draw(s, 1.f);
        ImGui::Render();
        automation.Publish(s);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    automation.Publish(s);
    ImGui::DestroyContext();
    s.mods.reset();
    return s.result == 1;
}
}

bool Show() {
    update::RemovePreviousFiles(app::ExeDir());
    Session s;
    // Tests keep English unless OPENSHAO_LAUNCHER_LANGUAGE_PROMPT=1 requests the first-run choice.
    const char* prompt = std::getenv("OPENSHAO_LAUNCHER_LANGUAGE_PROMPT");
    const bool first_run_test = prompt && *prompt == '1', automated = !options.launcher_test.empty();
    s.language_saved = SavedLauncherLanguage();
    s.system_language = text::FromLocales(SystemLocales());
    text::Select(!s.language_saved.empty() ? s.language_saved : automated && !first_run_test ? std::string("en") : s.system_language);
    LOGI("Launcher language: %s (system %s, saved %s)", text::Current().c_str(), s.system_language.c_str(),
         s.language_saved.empty() ? "none" : s.language_saved.c_str());
    s.source = !options.disc.empty() ? options.disc : !options.game.empty() ? options.game : setup::RememberedDisc();
    std::error_code unavailable;
    const bool incomplete_folder = !s.source.empty() && fs::is_directory(s.source, unavailable) &&
        !fs::is_regular_file(s.source / "default.xbe", unavailable);
    if (!options.configure_only && options.disc.empty() && options.game.empty() &&
        (options.setup || s.source.empty() || incomplete_folder || !fs::exists(s.source, unavailable))) {
        if (!s.source.empty() && !options.setup)
            s.game_error = incomplete_folder ? std::string(T("game.missing_xbe")) : F("game.moved", {{"path", files::Utf8(s.source)}});
        s.source.clear();
        s.game_required = true;
        s.page = Page::Game;
    }
    try { s.settings = Read(Disk()); }
    catch (const std::exception& e) { paths::ReportError(L"Cannot read settings.toml.\n" + Wide(e.what())); return false; }
    s.data_folder = paths::Root(); s.save_folder = paths::SaveRoot();
    s.check_updates = CheckUpdatesEnabled();
    s.discord_presence = DiscordPresenceEnabled();
    if (s.language_saved.empty() && (!automated || first_run_test)) s.OpenLanguage(true);
    if (!options.launcher_test.empty()) return RunAutomated(s);

    Window window;
    if (!window.Open("OpenShao", 1040, 640, art::Prepare)) {
        if (s.game_required) {
            fs::path selected;
            if (!setup::ChooseDisc(selected)) return false;
            setup::Source prepared; std::vector<uint8_t> executable; std::string error;
            if (!setup::PrepareGame(selected, prepared, executable, error, true) || !setup::RememberDisc(prepared.path, error)) {
                paths::ReportError(Wide(error)); return false;
            }
            s.source = prepared.path;
        }
        if (options.disc.empty() && options.game.empty() && !s.source.empty()) options.disc = s.source;
        return PlayWithSavedSettings();
    }
    s.mods = std::make_unique<mods::Browser>(paths::Mods());
    s.updater = std::make_unique<update::Updater>(app::ExeDir(), paths::Cache() / "update", mods::RecompVersion);
    if (s.check_updates) s.updater->Tick();
    if (!paths::MoveWarning().empty()) s.Message(T("message.folder_move"), paths::MoveWarning());
    {
        Gamepads gamepads;
        while (s.result < 0) {
            if (!window.BeginFrame(s.Animating())) s.Close();
            if (s.check_updates && s.updater) s.updater->Tick();
            s.OfferUpdate();
            gamepads.Poll(s);
            Draw(s, window.Scale());
            window.EndFrame();
            if (s.updater && s.updater->Get().state == update::State::Ready) { s.relaunch = true; s.result = 0; }
        }
    }
    window.Close();
    s.mods.reset();
    s.updater.reset();
    if (s.relaunch) {
        std::string error;
        paths::ReleaseDataLock();
        if (!update::Relaunch(app::ExeDir(), error)) paths::ReportError(L"The update is installed. Start OpenShao again to use it.\n" + Wide(error));
        return false;
    }
    return s.result == 1;
}
bool DiscordPresenceEnabled() {
    try { return Disk()["launcher"]["discord_presence"].value_or(true); } catch (...) { return true; }
}
bool ShouldPersist() { return persist.load(); }
namespace {
// F9 updates the Show FPS option and saves it at once, so the menus and the next start agree with the screen.
void SaveShowFps(bool visible) {
    std::lock_guard guard(settings_mutex);
    if (current.show_fps == visible) return;
    current.show_fps = visible;
    Publish();
    try {
        auto table = Disk();
        table.insert_or_assign("show_fps", visible);
        std::string error;
        if (!Write(paths::Config(), table, error)) LOGE("Show FPS could not be saved: %s", error.c_str());
    } catch (const std::exception& e) { LOGE("Show FPS could not be saved: %s", e.what()); }
}
}
void Load(const void* raw) {
    const auto& table = *static_cast<const toml::table*>(raw);
    current = Read(table); app::Get().fps_cap = FrameRate;
    frames::Configure(current.fps_cap,current.vsync);
    controller::Load(raw); keyboard::Configure(current.keyboard); mouse::Configure(current.mouse); performance::SetVisible(current.show_fps);
    performance::OnToggle(&SaveShowFps);
    cheat::SetEnabled(current.cheats_enabled); cheat::SetHotkey(current.cheat_menu_key); cheat::SetStatueFullHeal(current.statue_full_heal); cheat::SetMenuPosition(current.cheat_menu_x, current.cheat_menu_y);
    if (!options.language_pack.empty() && !options.resume_main_menu) current.language = "pt-BR";
    Publish();
}
void Save(void* raw) {
    std::lock_guard guard(settings_mutex);
    auto& table = *static_cast<toml::table*>(raw);
    table.insert_or_assign("renderer",current.renderer);
    table.insert_or_assign("language", requested_language.empty()?current.language:requested_language); table.insert_or_assign("master_volume", current.master_volume);
    table.insert_or_assign("voice_language",requested_voice_language.empty()?current.voice_language:requested_voice_language);
    table.erase("blur"); table.erase("bloom");
    table.insert_or_assign("bloom_effect", current.bloom_effect);
    table.insert_or_assign("aspect_ratio",current.aspect_ratio);
    table.insert_or_assign("show_fps", performance::Visible()); controller::Save(raw);
    table.insert_or_assign("screenshot_notifications", current.screenshot_notifications);
    const auto extras = Merge({}, current);
    table.insert_or_assign("enhancements", *extras["enhancements"].as_table());
    table.insert_or_assign("game", *extras["game"].as_table());
    table.insert_or_assign("mouse", *extras["mouse"].as_table());
    table.insert_or_assign("keyboard", *extras["keyboard"].as_table());
}
bool Persist(const toml::table& table, std::string& error) {
    if (!ShouldPersist()) return true;
    try { return Write(paths::Config(), Merge(Disk(), Read(table)), error); }
    catch (const std::exception& e) { error = e.what(); return false; }
}
unsigned VoiceLanguageIndex(){return GameLanguage(Current().voice_language);}
unsigned Language() { return GameLanguage(Current().language); }
float AudioGain() { return audio_gain.load(); }
const Settings& Current() { thread_local std::shared_ptr<const Settings> snapshot; snapshot=published.load();return *snapshot; }
Settings Saved(){try{return Read(Disk());}catch(...){return Current();}}
bool LanguageAvailable(std::string_view language){return !NeedsLanguagePack(language);}
bool ApplyInGame(const Settings& settings,bool save,std::string& error) {
    std::lock_guard guard(settings_mutex);
    if(!Validate(settings,error))return false;
    if(NeedsLanguagePack(settings.language) || NeedsLanguagePack(settings.voice_language)){error="Install and enable the language pack in the launcher's Mods page first.";return false;}
    if(save) {
        try {if(!Write(paths::Config(),Merge(Disk(),settings),error))return false;}
        catch(const std::exception& e){error=e.what();return false;}
        persist=true;
        requested_language=settings.language;requested_voice_language=settings.voice_language;
    }
    const auto previous=Current();
    current=settings;
    // Resource archives and font atlases stay coherent until the next boot.
    current.language=previous.language;current.voice_language=previous.voice_language;
    current.renderer=previous.renderer;
    Publish();
    auto& host=app::Get();
    host.display_mode=settings.display_mode=="fullscreen"?app::DisplayMode::Fullscreen:
        settings.display_mode=="windowed"?app::DisplayMode::Windowed:app::DisplayMode::Borderless;
    host.width=settings.width;host.height=settings.height;host.render_percent=settings.render_percent;
    host.vsync=settings.vsync;host.aniso=settings.aniso;
    frames::Configure(settings.fps_cap,settings.vsync);
    controller::Configure(settings);keyboard::Configure(settings.keyboard);mouse::Configure(settings.mouse);performance::SetVisible(settings.show_fps);
    cheat::SetEnabled(settings.cheats_enabled);cheat::SetHotkey(settings.cheat_menu_key);cheat::SetStatueFullHeal(settings.statue_full_heal);cheat::SetMenuPosition(settings.cheat_menu_x,settings.cheat_menu_y);
    if(previous.display_mode!=settings.display_mode || previous.width!=settings.width || previous.height!=settings.height)
        app::ApplyDisplay();
    error.clear();return true;
}
}
