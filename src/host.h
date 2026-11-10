#pragma once
#include <filesystem>
#include <cstdint>
#include <string>
#include "save_storage.h"

namespace openshao {
struct Options {
    bool background = false;
    bool info = false;
    bool host_test = false;
    bool portable_saves = false;
    bool setup = false;
    bool no_launcher = false;
    bool configure_only = false;
    bool paths = false;
    bool support_report = false;
    bool validate_disc = false;
    bool register_disc = false;
    bool validate_language_pack = false;
    bool resume_main_menu = false;
    bool restart_preflight = false;
    uint32_t restart_parent = 0;
    uintptr_t restart_ready = 0;
    std::string restart_language;
    std::string restart_voice_language;
    unsigned virtual_gamepads = 0; // isolated hidden diagnostics only
    unsigned timeout = 0;
    std::filesystem::path game;
    std::filesystem::path disc;
    std::filesystem::path save_dir;
    std::filesystem::path data_dir;
    std::filesystem::path language_pack;
    std::filesystem::path launcher_test; // hidden: runs the launcher without a window for automated checks
};
extern Options options;
// Exit code of a start that found another OpenShao using the same data folder.
inline constexpr int kAlreadyOpen = 9;
// Returns 0 to start, 1 after a completed diagnostic, or an error code.
int Configure();
void StartWatchdog();
bool RequestExit();
int CheckHiddenWindow(void* window, bool display_changed);
bool OpenSaveDirectory(const std::wstring& name, std::filesystem::path& result, storage::DirectoryLock& lock);
}
