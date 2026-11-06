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
}
bool NeedsLanguagePack(std::string_view code) {
    return !mods::IsOriginalLanguage(code)&&options.language_pack.empty()&&language::FindInstalledPack(paths::Mods(),code).empty();
}
std::string MissingLanguagePack(const Settings& settings) {
    if(NeedsLanguagePack(settings.language))
        return "The selected text language ("+settings.language+") requires an enabled language pack. Install or enable it in Mods.";
    if(NeedsLanguagePack(settings.voice_language))
        return "The selected voice language ("+settings.voice_language+") requires an enabled language pack. Install or enable it in Mods.";
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
bool ChooseFolder(fs::path& path, const char* title) {
#ifdef _WIN32
    return ChooseWithDialog(path, true, Wide(title).c_str());
#else
    return linux_platform::ChooseFile(path, true, title);
#endif
}
bool ChooseModArchive(fs::path& path) {
#ifdef _WIN32
    const COMDLG_FILTERSPEC filter[] = {{L"Mod archives and texture packs (*.tar.xz, *.ostp)", L"*.tar.xz;*.ostp"}};
    return ChooseWithDialog(path, false, L"Install a mod from file", filter, 1);
#else
    return linux_platform::ChooseFile(path, false, "Install a mod from file");
#endif
}

bool ChooseGameSource(fs::path& path, bool folder) {
#ifdef _WIN32
    const COMDLG_FILTERSPEC filters[] = {{L"Xbox game files (*.iso; *.xiso; *.zip; default.xbe)", L"*.iso;*.xiso;*.zip;default.xbe"},
                                         {L"All files", L"*.*"}};
    return ChooseWithDialog(path, folder, folder ? L"Choose the extracted game folder" : L"Choose your game file", filters, 2);
#else
    return linux_platform::ChooseFile(path, folder, folder ? "Choose the extracted game folder" : "Choose your game file");
#endif
}

// Game is the source selection page. It replaces the tabs until a usable game is chosen.
enum class Page { Play, Mods, Files, Support, About, Game };
const char* PageName(Page page) {
    switch (page) {
        case Page::Mods: return "mods"; case Page::Files: return "files"; case Page::About: return "about";
        case Page::Support: return "support";
        case Page::Game: return "game"; default: return "play";
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
            ok_ = ok; error_ = cancel_ ? "The game import was cancelled." : error; path_ = prepared.path; finished_ = true;
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
            Message("Mod operation in progress", "Wait for the mod operation to finish, or cancel it in Mods.");
            return;
        }
        if (const auto missing = MissingLanguagePack(settings); !missing.empty()) {
            page = Page::Mods;
            Message("Language pack required", missing);
            return;
        }
        std::string error;
        if (!Validate(settings, error)) { Message("Check your settings", error); return; }
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
            Message("Cannot install mod", error.empty() ? "Choose your game in Files before installing mods." : error);
            return {};
        }
        source = prepared.path;
        return prepared.path;
    }
    void InstallMod(const std::string& id) { if (const auto game = ModGame(); !game.empty()) mods->Install(id, game); }
    void InstallModFile(const fs::path& file) { if (const auto game = ModGame(); !game.empty()) mods->InstallFile(file, game); }
    void RemoveMod(const mods::Mod& mod) {
        const auto id = mod.id;
        Confirm("Remove " + mod.name + "?", "The mod's files are removed from your data folder. You can install it again later.", "Remove",
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
        catch (const std::exception& e) { Message("Cannot restore video settings", e.what()); return; }
        settings = safe;
        Message("Video settings restored", "The game starts borderless at your desktop resolution with the original aspect ratio.");
    }
    void SetRenderer(const std::string& renderer) {
#ifdef _WIN32
        if(renderer!="d3d12" && renderer!="vulkan")return;
        if(settings.renderer==renderer)return;
        auto desired=settings;desired.renderer=renderer;
        std::string error;
        try { if(!Write(paths::Config(),Merge(Disk(),desired),error))throw std::runtime_error(error); }
        catch(const std::exception& e){Message("Cannot save graphics API",e.what());return;}
        settings=std::move(desired);
#endif
    }
    void MoveFolder(bool data, const fs::path& chosen = {}) {
        fs::path selected = chosen;
        if (selected.empty() && !ChooseFolder(selected, data ? "Choose an empty data folder" : "Choose an empty saves folder")) return;
        std::string error;
        if (!paths::ChangeFolders(data ? selected : data_folder, data ? save_folder : selected, error)) {
            Message("Cannot move the folder", error);
            return;
        }
        Message("Folder move scheduled", "Open OpenShao again to move your files to the new folder.", true);
    }
    void OpenSaves() {
        const auto root = !options.save_dir.empty() ? fs::absolute(options.save_dir) :
                          options.portable_saves ? app::ExeDir() / "hdd" : paths::SaveRoot() / "hdd";
        std::error_code ec;
        if (fs::is_directory(root, ec)) OpenLocation(files::Utf8(root));
        else Message("No saves yet", "The game creates this folder the first time it saves:\n" + files::Utf8(root));
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
        } catch (const std::exception& e) { Message("Cannot save the update preference", e.what()); }
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
        } catch (const std::exception& e) { Message("Cannot save the Discord preference", e.what()); }
    }
    void StartUpdate() {
        if (!updater) return;
        if (Busy()) { page = Page::Mods; Message("Mod operation in progress", "Wait for the mod operation to finish before updating."); return; }
        updater->Download();
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
    if (s.page == Page::Game || s.game_required) {
        const bool support = s.page == Page::Support;
        if (Link("Support##selection", c, W-40, 28, support ? "Choose game" : "Support", true))
            s.page = support ? Page::Game : Page::Support;
        return;
    }
    const Page pages[] = {Page::Play, Page::Mods, Page::Files, Page::Support, Page::About};
    const char* labels[] = {"Play", "Mods", "Files", "Support", "About"};
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
        const char* label = options.configure_only ? "Close" : "Play";
        const float label_h = c.Measure(Weight::Medium, 54, label).y;
        c.TextCentered(Weight::Medium, 54, center.x + 4, center.y - label_h / 2 - 2, Rgb(color::Paper, hit.hovered ? 1.f : 0.94f), label);
    } else {
        const float value = view.total ? float(double(view.done) / double(view.total)) : 0.f;
        art::Enso(c, center, R, stroke, 1.f, 0.09f, 11, 44, false, color::Paper);
        art::Enso(c, center, R, stroke, view.state == update::State::Ready ? 1.f : std::clamp(value, 0.f, 1.f), 1.f);
        char percent[16];
        std::snprintf(percent, sizeof percent, "%d%%", int(std::clamp(value, 0.f, 1.f) * 100.f));
        c.TextCentered(Weight::Medium, 54, center.x + 4, center.y - 30, Rgb(color::Paper), view.state == update::State::Ready ? "100%" : percent);
        c.TextCentered(Weight::Regular, 18.5f, center.x + 4, center.y + 22, Rgb(color::Mist), ("Updating to " + view.release.version).c_str());
    }

#ifdef _WIN32
    const float api_y=footer-48, api_x=W/2-208;
    c.Text(Weight::Medium,19,api_x,api_y+10,Rgb(color::Mist),"Graphics API");
    if(Button("##renderer-d3d12",c,api_x+138,api_y,api_x+278,api_y+40,"Direct3D 12",s.settings.renderer=="d3d12",!updating))s.SetRenderer("d3d12");
    if(Button("##renderer-vulkan",c,api_x+290,api_y,api_x+416,api_y+40,"Vulkan",s.settings.renderer=="vulkan",!updating))s.SetRenderer("vulkan");
#endif
    c.Rule(40, W - 40, footer);
    const float y = rows.primary;
    const bool ready = !s.source.empty();
    c.list->AddCircleFilled(c.P(46, y + 9), c.S(4), Rgb(ready ? color::Moss : color::Warning));
    const char* status = ready ? "Game ready" : "No game selected";
    c.Text(Weight::Medium, 19, 60, y, Rgb(color::Paper), status);
    const float detail_x = 60 + c.Measure(Weight::Medium, 19, status).x + 10;
    if (!ready && Link("Choose game##footer", c, detail_x, y, "Choose game")) s.ChangeGame();

    const float right = W - 40;
    const auto notes_label = "Release notes (" + std::string(mods::RecompVersion) + ")";
    if (Link("Release notes##installed", c, 40, rows.secondary, notes_label.c_str()))
        OpenLocation(update::ReleaseNotesUrl(mods::RecompVersion));
    const bool can_check = s.updater && view.state != update::State::Checking &&
                           view.state != update::State::Downloading && view.state != update::State::Ready;
    if (can_check) {
        if (Link("Check game updates##footer", c, right, rows.secondary, "Check for updates", true))
            s.CheckGameUpdates();
    }
    const bool cached_check_loading = can_check &&
        s.cached_check_pulse.Active(std::chrono::steady_clock::now());
    if (cached_check_loading) {
        UpdateSpinner(c, right - 11.f, y + 10.f);
    } else switch (view.state) {
        case update::State::Available:
            if (Button("Update##footer", c, right - 96, rows.button_top, right, rows.button_bottom, "Update", true)) s.StartUpdate();
            c.TextRight(Weight::Regular, 19, right - 112, y, Rgb(color::Mist), ("OpenShao " + view.release.version + " is available").c_str());
            break;
        case update::State::Downloading: {
            if (Button("Cancel##update", c, right - 96, rows.button_top, right, rows.button_bottom, "Cancel", false)) s.updater->Cancel();
            char progress[64];
            std::snprintf(progress, sizeof progress, "%.1f of %.1f MB", double(view.done) / 1048576.0, double(view.total) / 1048576.0);
            c.TextRight(Weight::Regular, 19, right - 112, y, Rgb(color::Mist), view.total ? progress : "Downloading update...");
            break;
        }
        case update::State::Ready:
            c.TextRight(Weight::Regular, 19, right, y, Rgb(color::Mist), "Restarting OpenShao...");
            break;
        case update::State::Checking:
            c.TextRight(Weight::Regular, 19, right - 27.f, y, Rgb(color::Dim), "Checking for updates...");
            UpdateSpinner(c, right - 11.f, y + 10.f);
            break;
        case update::State::Current:
            c.TextRight(Weight::Regular, 19, right, y, Rgb(color::Dim), "OpenShao is up to date");
            break;
        case update::State::Failed: {
            const bool offered = !view.release.version.empty();
            if (Link("Release page##update", c, right, y, "View releases", true))
                OpenLocation(view.release.page.empty() ? update::kReleasesPage : view.release.page);
            const float link = c.Measure(Weight::Medium, 19, "View releases").x;
            if (offered && Link("Retry##update", c, right - link - 18, y, "Retry", true)) s.StartUpdate();
            const float retry = offered ? c.Measure(Weight::Medium, 19, "Retry").x + 18 : 0.f;
            c.TextRight(Weight::Regular, 19, right - link - retry - 18, y, Rgb(color::Warning),
                        offered ? "Couldn't install the update" : "Couldn't check for updates");
            if (ImGui::IsMouseHoveringRect(c.P(W / 2, y - 6), c.P(right, y + 24)) && !view.error.empty()) ImGui::SetTooltip("%s", view.error.c_str());
            break;
        }
        default: break;
    }
}

void Header(const Canvas& c, const char* title, const char* subtitle) {
    c.Text(Weight::SemiBold, 36, 48, 104, Rgb(color::Paper), title);
    c.Text(Weight::Regular, 20, 48, 148, Rgb(color::Mist), subtitle);
}

void ModsPage(Session& s, const Canvas& c, float W, float H) {
    const auto view = s.mods->View();
    Header(c, "Mods", "Official OpenShao mods. Changes apply the next time you play.");
    const float right = W - 48;
    if (Link("Check for updates##mods", c, right, 112, "Check for updates", true, !view.busy)) s.mods->Refresh();
    const float refresh = c.Measure(Weight::Medium, 19, "Check for updates").x;
    if (Link("Install from file##mods", c, right - refresh - 28, 112, "Install from file", true, !view.busy)) {
        fs::path file;
        if (ChooseModArchive(file)) s.InstallModFile(file);
    }
    float top = 200;
    if (view.busy && !view.checking) {
        c.Text(Weight::Medium, 18.5f, 48, 176, Rgb(color::Paper), view.status.c_str());
        if (Link("Cancel##modop", c, right, 176, "Cancel", true)) s.mods->Cancel();
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
        const float text_x = 48 + 76, actions = installed ? 38 + 24 + (update ? 160 : 0) : 120;
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
        if (installed) meta = "Version " + installed->mod.version + (installed->enabled ? "" : ", disabled");
        else meta = release ? "Version " + release->version + " available" : "Not available for this version";
        if (!mod.authors.empty()) {
            meta += ", by ";
            for (size_t i = 0; i < mod.authors.size(); ++i) meta += (i ? " and " : "") + mod.authors[i];
        }
        if (!mod.unsupported_reason.empty()) meta = mod.unsupported_reason;
        const float meta_y = ty + 32 + summary_h + 6;
        list.Text(Weight::Regular, 18, text_x, meta_y, Rgb(mod.unsupported_reason.empty() ? color::Dim : color::Warning), meta.c_str());
        if (installed) {
            const float meta_w = list.Measure(Weight::Regular, 18, meta.c_str()).x;
            const float remove_x = text_x + meta_w + 14;
            ImGui::BeginDisabled(view.busy);
            const auto remove = Item("##remove", list, remove_x, meta_y - 1, remove_x + list.Measure(Weight::Medium, 18, "Remove").x, meta_y + 17);
            ImGui::EndDisabled();
            list.Text(Weight::Medium, 18, remove_x, meta_y, Rgb(remove.hovered ? color::Paper : color::Mist, view.busy ? 0.5f : 1.f), "Remove");
            if (remove.clicked) s.RemoveMod(mod);
            ImGui::BeginDisabled(view.busy);
            const auto toggle = Item("##toggle", list, right - 38, ty + 6, right, ty + 28);
            ImGui::EndDisabled();
            art::Toggle(list, right - 38, ty + 6, installed->enabled, toggle.hovered);
            if (toggle.clicked) s.ToggleMod(mod.id);
            if (update) {
                const std::string label = "Update to " + release->version;
                if (Button("##update", list, right - 38 - 24 - 148, ty, right - 38 - 24, ty + 34, label.c_str(), false, !view.busy)) s.InstallMod(mod.id);
            }
        } else if (Button("##install", list, right - 96, ty, right, ty + 34, "Install", true, release && !view.busy && mod.unsupported_reason.empty())) {
            s.InstallMod(mod.id);
        }
        y += row_h;
        list.Rule(48, right, y);
        ImGui::PopID();
    }
    const char* empty = rows.empty() ? "The catalog is unavailable right now. You can install a downloaded .tar.xz package or .ostp texture pack with Install from file."
                                     : "New official mods appear here when they are published.";
    list.Text(Weight::Regular, 18.5f, 48, y + 24, Rgb(color::Dim), empty, right - 48);
    ImGui::SetCursorScreenPos(list.P(0, y + 64));
    ImGui::Dummy(ImVec2(1, 1));
    ImGui::EndChild();
}

void FilesPage(Session& s, const Canvas& parent, float W, float H) {
    Header(parent, "Files", "Where OpenShao finds your game and keeps your saves.");
    ImGui::SetCursorScreenPos(parent.P(0, 200));
    ImGui::BeginChild("##files-list", ImVec2(parent.S(W), parent.S(std::max(40.f, H - 224))),
                      ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    const Canvas c{ImGui::GetWindowDrawList(), parent.scale, ImGui::GetCursorScreenPos()};
    const float right = W - 48, label_x = 48, value_x = 170;
    const bool busy = s.mods->View().busy;
    const bool locked = paths::DataFolderOverridden() || busy;
    const bool saves_locked = locked || !options.save_dir.empty() || options.portable_saves;
    float y = 0;
    auto row = [&](const char* label, const std::string& value, int buttons) {
        c.Rule(48, right, y);
        c.Text(Weight::SemiBold, 20, label_x, y + 25, Rgb(color::Paper), label);
        const float width = right - value_x - float(buttons) * 96 - 16;
        c.Text(Weight::Regular, 19, value_x, y + 25, Rgb(color::Mist), c.Fit(Weight::Regular, 19, width, value).c_str());
    };
    row("Game", s.source.empty() ? std::string("No game selected") : files::Utf8(s.source), 1);
    if (Button("Change##game", c, right - 84, y + 16, right, y + 50, "Change", false, !busy)) s.ChangeGame();
    y += 60;
    row("Saves", files::Utf8(s.save_folder), 2);
    if (Button("Move##saves", c, right - 84, y + 16, right, y + 50, "Move", false, !saves_locked)) s.MoveFolder(false);
    if (Button("Open##saves", c, right - 180, y + 16, right - 96, y + 50, "Open", false)) s.OpenSaves();
    y += 60;
    row("Data", files::Utf8(s.data_folder), 2);
    if (Button("Move##data", c, right - 84, y + 16, right, y + 50, "Move", false, !locked)) s.MoveFolder(true);
    if (Button("Open##data", c, right - 180, y + 16, right - 96, y + 50, "Open", false)) OpenLocation(files::Utf8(paths::Root()));
    y += 60;
    row("Reports", "Generate a diagnostic ZIP in Support.", 1);
    if (Button("Support##files", c, right - 84, y + 16, right, y + 50, "Support", false)) s.page = Page::Support;
    y += 60;
    row("Video", "Restore the display settings if the game does not start correctly.", 1);
    if (Button("Restore##video", c, right - 84, y + 16, right, y + 50, "Restore", false)) s.RestoreVideo();
    y += 60;
    c.Rule(48, right, y);
    c.Text(Weight::SemiBold, 20, label_x, y + 25, Rgb(color::Paper), "Updates");
    c.Text(Weight::Regular, 19, value_x, y + 25, Rgb(color::Mist), "Check for a new version when the launcher opens");
    const auto toggle = Item("##check-updates", c, right - 38, y + 22, right, y + 44);
    art::Toggle(c, right - 38, y + 22, s.check_updates, toggle.hovered);
    if (toggle.clicked) s.SetCheckUpdates(!s.check_updates);
    y += 60;
    c.Rule(48, right, y);
    c.Text(Weight::SemiBold, 20, label_x, y + 25, Rgb(color::Paper), "Discord");
    c.Text(Weight::Regular, 19, value_x, y + 25, Rgb(color::Mist), "Show OpenShao in your Discord activity");
    const auto discord_toggle = Item("##discord-presence", c, right - 38, y + 22, right, y + 44);
    art::Toggle(c, right - 38, y + 22, s.discord_presence, discord_toggle.hovered);
    if (discord_toggle.clicked) s.SetDiscordPresence(!s.discord_presence);
    ImGui::SetCursorScreenPos(c.P(0, y + 54));
    ImGui::Dummy(ImVec2(1, 1));
    ImGui::EndChild();
}

void SupportPage(Session& s, const Canvas& c, float W, float) {
    Header(c, "Support", "Create a diagnostic ZIP and attach it to your GitHub issue.");
    const float right = W-48;
    const bool busy = s.report_job.valid();
    c.Rule(48,right,200);
    c.Text(Weight::SemiBold,22,48,224,Rgb(color::Paper),"Diagnostic report");
    c.Text(Weight::Regular,19,48,260,Rgb(color::Mist),
        "Includes session logs, selected settings, mod versions, and the latest available crash dump.", right-48);
    c.Text(Weight::Regular,19,48,310,Rgb(color::Mist),
        "Text logs filter account and computer names, personal paths and credentials.\nSaves, game files and screenshots are excluded.", right-48);
    if(Button("Generate##support",c,48,382,244,424,busy?"Generating...":"Generate report",true,!busy)) s.GenerateReport();
    if(Button("Folder##support",c,260,382,432,424,"Open reports",false)) {
        const auto folder=paths::Root()/"reports";std::error_code ec;fs::create_directories(folder,ec);
        OpenLocation(files::Utf8(folder));
    }
    c.Rule(48,right,450);
    if(!s.report.error.empty()) c.Text(Weight::Regular,19,48,470,Rgb(color::Warning),s.report.error.c_str(),right-48);
    else if(!s.report.zip.empty()) {
        c.Text(Weight::Medium,19,48,470,Rgb(color::Paper),c.Fit(Weight::Medium,19,right-210,files::Utf8(s.report.zip.filename())).c_str());
        if(Link("Show##support",c,right,470,"Show ZIP",true)) support::OpenReport(s.report.zip);
    } else c.Text(Weight::Regular,19,48,470,Rgb(color::Mist),"No diagnostic ZIP has been generated yet.");
    c.Text(Weight::Regular,19,48,518,Rgb(color::Mist),"Describe what happened and how to reproduce it.",right-250);
    if(Button("Report##support",c,right-166,512,right,554,"Report a bug",false)) {
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
    Header(c, "About", ("Version " + std::string(mods::RecompVersion)).c_str());
    const float right = W - 48, wrap = std::min(720.f, right - 48);
    if (Link("Licenses##about", c, right, 112, "Licenses", true)) OpenLocation(files::Utf8(app::ExeDir() / "licenses"));
    const float licenses = c.Measure(Weight::Medium, 19, "Licenses").x;
    if (Link("Project page##about", c, right - licenses - 28, 112, "Project page", true)) OpenLocation("https://github.com/OAleex/OpenShao");
    c.Rule(48, right, 200);
    struct Paragraph { Weight weight; float size; uint32_t rgb; const char* text; };
    const Paragraph paragraphs[] = {
        {Weight::Regular, 20, color::Paper, "OpenShao runs Mortal Kombat: Shaolin Monks natively on PC through static recompilation of "
                                            "the original Xbox game. It includes no game files: you play with your own copy."},
        {Weight::Regular, 19, color::Mist, "OpenShao is free software under the GNU General Public License, version 2 or later. The "
                                           "license does not cover the original game's code, assets, names or artwork. It is built "
                                           "with the components below. Their full license texts are in the licenses folder of every "
                                           "release and of the project repository."},
    };
    float y = 224;
    for (const auto& paragraph : paragraphs) {
        c.Text(paragraph.weight, paragraph.size, 48, y, Rgb(paragraph.rgb), paragraph.text, wrap);
        y += c.Measure(paragraph.weight, paragraph.size, paragraph.text, wrap).y + 16;
    }
    // Keep this list in step with licenses/ and the package contents.
    struct Component { const char* name; const char* license; };
    static constexpr Component components[] = {
        {"XboxXBERecomp", "MIT"}, {"SDL 3", "zlib"}, {"Dear ImGui", "MIT"}, {"toml++", "MIT"},
        {"Lua 5.4", "MIT"}, {"miniz", "MIT"}, {"XZ Embedded", "0BSD"}, {"Vulkan-Headers", "Apache-2.0 or MIT"},
        {"glslang", "BSD-3-Clause and others"}, {"Hanken Grotesk", "SIL Open Font License 1.1"},
        {"discord-rpc and RapidJSON", "MIT"}, {"Keyboard input integration", "GPL-2.0-or-later"},
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
        if (Button("OK##popup", c, bx - 96, by, bx, by + 36, "OK", true)) s.Dismiss();
    } else {
        const float confirm_w = std::max(96.f, c.Measure(Weight::SemiBold, 19, s.popup->confirm.c_str()).x + 36);
        if (Button("Confirm##popup", c, bx - confirm_w, by, bx, by + 36, s.popup->confirm.c_str(), true)) s.Accept();
        else if (Button("Cancel##popup", c, bx - confirm_w - 12 - 96, by, bx - confirm_w - 12, by + 36, "Cancel", false)) s.Dismiss();
    }
}

void GamePage(Session& s, const Canvas& c, float W, float H) {
    const float right = W - 48;
    Header(c, s.game_required ? "Choose your game" : "Change game", "Use your own copy of the original USA/Europe Xbox release.");
    if (!s.game_required && Link("Back##game", c, right, 112, "Back", true, !s.preparing)) s.LeaveGame();
    float y = 200;
    if (!s.game_error.empty()) {
        c.Text(Weight::Regular, 18.5f, 48, 172, Rgb(color::Warning), c.Fit(Weight::Regular, 18.5f, right - 48, s.game_error).c_str());
        y = 208;
    }
    if (s.preparing) {
        const bool importing = s.preparing->Importing();
        c.Rule(48, right, y);
        const char* status = s.preparing->Cancelling() ? "Cancelling..." : importing ? "Importing your game files..." : "Checking your game...";
        c.Text(Weight::Medium, 20, 48, y + 30, Rgb(color::Paper), status);
        if (importing && !s.preparing->Cancelling() && Link("Cancel##gameimport", c, right, y + 30, "Cancel", true)) s.preparing->Cancel();
        art::ProgressBar(c, 48, right, y + 70, importing ? s.preparing->Progress() : 0.f);
        return;
    }
    struct Choice { const char* id; const char* title; const char* text; const char* action; bool folder; };
    const Choice choices[] = {
        {"##game-file", "Game file", "An ISO, XISO or ZIP image, or default.xbe. ZIP files are imported into your data folder.", "Choose file", false},
        {"##game-folder", "Extracted folder", "The complete folder that contains default.xbe. OpenShao reads it in place.", "Choose folder", true},
    };
    for (const auto& choice : choices) {
        c.Rule(48, right, y);
        const float row_h = 104;
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
        const float text_x = 124, button_w = c.Measure(Weight::SemiBold, 19, choice.action).x + 40;
        c.Text(Weight::SemiBold, 22, text_x, y + 24, Rgb(color::Paper), choice.title);
        c.Text(Weight::Regular, 19, text_x, y + 56, Rgb(color::Mist), c.Fit(Weight::Regular, 19, right - text_x - button_w - 24, choice.text).c_str());
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
        bool down[4] = {};
        const SDL_GamepadButton buttons[4] = {SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER};
        for (auto* pad : pads_)
            for (int b = 0; b < 4; ++b) down[b] |= SDL_GetGamepadButton(pad, buttons[b]);
        bool pressed[4];
        for (int b = 0; b < 4; ++b) { pressed[b] = down[b] && !previous_[b]; previous_[b] = down[b]; }
        if (s.popup) {
            if (pressed[0]) { if (s.popup->confirm.empty()) s.Dismiss(); else s.Accept(); }
            else if (pressed[1]) s.Dismiss();
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
    bool previous_[4] = {};
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
            << ",\"report_path\":\"" << Escape(files::Utf8(s.report.zip)) << "\",\"report_error\":\"" << Escape(s.report.error) << "\"}";
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
    if (!paths::MoveWarning().empty()) s.Message("Folder move", paths::MoveWarning());
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
    s.source = !options.disc.empty() ? options.disc : !options.game.empty() ? options.game : setup::RememberedDisc();
    std::error_code unavailable;
    const bool incomplete_folder = !s.source.empty() && fs::is_directory(s.source, unavailable) &&
        !fs::is_regular_file(s.source / "default.xbe", unavailable);
    if (!options.configure_only && options.disc.empty() && options.game.empty() &&
        (options.setup || s.source.empty() || incomplete_folder || !fs::exists(s.source, unavailable))) {
        if (!s.source.empty() && !options.setup)
            s.game_error = incomplete_folder ? "The saved game folder does not contain default.xbe. Choose your complete game folder or image." :
                "Your game is no longer at " + files::Utf8(s.source) + ". Choose it again.";
        s.source.clear();
        s.game_required = true;
        s.page = Page::Game;
    }
    try { s.settings = Read(Disk()); }
    catch (const std::exception& e) { paths::ReportError(L"Cannot read settings.toml.\n" + Wide(e.what())); return false; }
    s.data_folder = paths::Root(); s.save_folder = paths::SaveRoot();
    s.check_updates = CheckUpdatesEnabled();
    s.discord_presence = DiscordPresenceEnabled();
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
    if (!paths::MoveWarning().empty()) s.Message("Folder move", paths::MoveWarning());
    {
        Gamepads gamepads;
        while (s.result < 0) {
            if (!window.BeginFrame(s.Animating())) s.Close();
            if (s.check_updates && s.updater) s.updater->Tick();
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
void Load(const void* raw) {
    const auto& table = *static_cast<const toml::table*>(raw);
    current = Read(table); app::Get().fps_cap = FrameRate;
    frames::Configure(current.fps_cap,current.vsync);
    controller::Load(raw); keyboard::Configure(current.keyboard); mouse::Configure(current.mouse); performance::SetVisible(current.show_fps);
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
    if(previous.display_mode!=settings.display_mode || previous.width!=settings.width || previous.height!=settings.height)
        app::ApplyDisplay();
    error.clear();return true;
}
}
