#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <shellapi.h>
#else
#include <cstdio>
#include <cstdlib>
#include "linux/linux_platform.h"
#endif
#include <sstream>
#include <atomic>
#include <memory>
#include <toml++/toml.hpp>
#include "runtime_paths.h"
#include "support_report.h"
#include "core/log.h"
#include "host.h"
#include "legacy_folders.h"
#include "path_preferences.h"
#include "save_storage.h"

namespace fs = std::filesystem;
namespace openshao::paths {
namespace {
fs::path root, executable_directory;
fs::path save_root, locator;
std::string move_warning;
bool data_overridden = false;
std::unique_ptr<storage::DirectoryLock> data_lock;
bool data_in_use = false;
bool isolated_run = false;
std::string Utf8(const fs::path& value) {
    const auto bytes = value.u8string();
    return {bytes.begin(), bytes.end()};
}
constexpr const char* kDataFolder = "OpenShao";
// Moves the default data and saves folders of an earlier release, found by their contents, to the current names.
// Read-only queries and explicit data folders keep using the earlier folders without moving them.
std::string AdoptPreviousName(const fs::path& parent, bool keep) {
    std::vector<LegacyFolder> folders{{FindPreviousFolder(parent, root, IsDataFolder), root}};
#ifdef _WIN32
    folders.push_back({FindPreviousFolder(save_root.parent_path(), save_root, IsSavesFolder), save_root});
#endif
    std::string warning;
    const auto adopted = AdoptLegacyFolders(folders, !keep, warning);
    root = adopted[0];
#ifdef _WIN32
    save_root = adopted[1];
#else
    // The Linux saves folder lives inside the data folder.
    if (root != folders[0].current) save_root = root / "saves";
#endif
    if (keep) return warning;
    std::vector<LegacyFolder> moved;
    for (size_t i = 0; i < folders.size(); ++i)
        if (!folders[i].previous.empty() && adopted[i] == folders[i].current) moved.push_back(folders[i]);
    if (moved.empty()) return warning;
    for (const auto& file : {root / "paths.toml", root / "game-source.toml"}) {
        std::string error;
        if (!RewriteLegacyPaths(file, moved, error)) { if (!warning.empty()) warning += "\n"; warning += error; }
    }
    return warning;
}
}
bool Initialize(const fs::path& executable, bool isolated, const fs::path& requested, std::string& error, bool read_only) {
    try {
        executable_directory = fs::absolute(executable);
        isolated_run = isolated;
        data_overridden = isolated || !requested.empty();
        move_warning.clear();
        data_lock.reset();
        data_in_use = false;
        std::string save_error, legacy_warning;
        if (isolated) {
            root = requested.empty() ? executable_directory / "runtime-data" : fs::absolute(requested);
            save_root = root / "saves";
        } else {
#ifndef _WIN32
            const auto parent = linux_platform::DataHome();
#else
            PWSTR value = nullptr;
            const HRESULT status = SHGetKnownFolderPath(FOLDERID_LocalAppData, read_only ? KF_FLAG_DEFAULT : KF_FLAG_CREATE, nullptr, &value);
            if (FAILED(status)) throw std::runtime_error("Windows LocalAppData is unavailable");
            const fs::path parent(value);
            CoTaskMemFree(value);
#endif
            root = parent / kDataFolder;
            save_root = storage::SavedGamesRoot(save_error, false);
            if (save_root.empty()) throw std::runtime_error(save_error);
            legacy_warning = AdoptPreviousName(parent, read_only || !requested.empty());
        }
        const auto default_data = root;
        locator = default_data / "paths.toml";
        if (!isolated) {
            Locations selected;
            if (!LoadLocations(locator, {default_data, save_root}, selected, move_warning, !read_only && requested.empty()))
                throw std::runtime_error(move_warning);
            root = selected.data; save_root = selected.saves;
        }
        if (!legacy_warning.empty()) move_warning = move_warning.empty() ? legacy_warning : move_warning + "\n" + legacy_warning;
        if (!requested.empty()) root = fs::absolute(requested);
        if (read_only) return true;
        for (const auto& directory : {root, Logs(), Cache(), Crashes()}) fs::create_directories(directory);
        if (!isolated) {
            data_lock = std::make_unique<storage::DirectoryLock>();
            if (!data_lock->Acquire(root, error, &data_in_use)) { data_lock.reset(); return false; }
        }
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
const fs::path& Root() { return root; }
fs::path Config() { return root / "settings.toml"; }
fs::path Logs() { return root / "logs"; }
fs::path Cache() { return root / "cache"; }
fs::path Crashes() { return root / "crashes"; }
fs::path Captures() { return isolated_run ? executable_directory : Cache(); }
fs::path Selection() { return root / "game-source.toml"; }
fs::path Mods() { return root / "mods"; }
const fs::path& SaveRoot() { return save_root; }
const std::string& MoveWarning() { return move_warning; }
bool DataFolderOverridden() { return data_overridden; }
void ReleaseDataLock() { data_lock.reset(); }
bool DataInUse() { return data_in_use; }
bool ChangeFolders(const fs::path& data, const fs::path& saves, std::string& error) {
    if (data_overridden) { error = "Folder preferences are overridden by this launch's command-line options"; return false; }
    return ScheduleLocations(locator, {root, save_root}, {data, saves}, error);
}
std::string Description() {
    toml::table values{{"data", Utf8(Root())}, {"config", Utf8(Config())},
                       {"logs", Utf8(Logs())}, {"cache", Utf8(Cache())}, {"crashes", Utf8(Crashes())},
                       {"mods", Utf8(Mods())}, {"saves", Utf8(SaveRoot())}};
    std::ostringstream stream;
    stream << toml::json_formatter{values};
    return stream.str();
}
void ReportError(const std::wstring& reason, const fs::path& dump) {
    LOGE("Application error: %s", Utf8(fs::path(reason)).c_str());
    logx::Flush();
    static std::atomic_flag displayed = ATOMIC_FLAG_INIT;
    if (options.background || displayed.test_and_set()) return;
    support::StopMonitor();
    const auto text = Utf8(fs::path(reason));
    const auto& path = logx::Path();
    const auto report = support::Generate({Root(), fs::path(std::u8string(path.begin(),path.end())), OPENSHAO_RECOMP_VERSION, "error", text, dump});
    support::ShowReport(report, text);
    LOGR("Error dialog closed.");
    logx::Flush();
}
}
