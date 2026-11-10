#include "linux/linux_platform.h"

#include <SDL3/SDL.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits.h>

namespace fs = std::filesystem;

namespace linux_platform {
namespace {
std::vector<std::wstring> g_arguments;

std::wstring Wide(const char* text) {
    const std::string bytes = text ? text : "";
    return fs::path(std::u8string(bytes.begin(), bytes.end())).wstring();
}

fs::path FromUtf8(const std::string& text) { return fs::path(std::u8string(text.begin(), text.end())); }

bool OnPath(const char* program) {
    const char* path = std::getenv("PATH");
    if (!path) return false;
    std::string list = path;
    size_t start = 0;
    while (start <= list.size()) {
        const size_t end = list.find(':', start);
        const std::string dir = list.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!dir.empty() && access((dir + "/" + program).c_str(), X_OK) == 0) return true;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return false;
}

// Runs a fixed helper command (no user text in it) and returns its first output line.
bool Capture(const std::string& command, std::string& line) {
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return false;
    char buffer[PATH_MAX + 2] = {};
    const bool read = std::fgets(buffer, sizeof buffer, pipe) != nullptr;
    const int status = pclose(pipe);
    if (!read || status != 0) return false;
    line = buffer;
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
    return !line.empty();
}

struct DialogResult {
    std::atomic<bool> done{false};
    std::string path;
};

void SDLCALL DialogDone(void* data, const char* const* files, int) {
    auto* result = static_cast<DialogResult*>(data);
    if (files && files[0]) result->path = files[0];
    result->done = true;
}

bool ChooseWithSdl(fs::path& selected, bool folder, const std::string& title) {
    if (!SDL_WasInit(SDL_INIT_VIDEO) && !SDL_InitSubSystem(SDL_INIT_VIDEO)) return false;
    DialogResult result;
    static const SDL_DialogFileFilter filters[] = {
        {"Xbox game files (ISO, XISO, ZIP, default.xbe)", "iso;xiso;zip;xbe"},
        {"Mod archives and texture packs (.tar.xz, .ostp)", "xz;ostp"},
        {"All files", "*"},
    };
    (void)title;
    if (folder) SDL_ShowOpenFolderDialog(DialogDone, &result, nullptr, nullptr, false);
    else SDL_ShowOpenFileDialog(DialogDone, &result, nullptr, filters, 3, nullptr, false);
    while (!result.done) {
        SDL_PumpEvents();
        SDL_Delay(10);
    }
    if (result.path.empty()) return false;
    selected = FromUtf8(result.path);
    return true;
}
}

void SetArguments(int argc, char** argv) {
    g_arguments.clear();
    for (int i = 0; i < argc; ++i) g_arguments.push_back(Wide(argv[i]));
}

std::vector<std::wstring> Arguments() { return g_arguments; }

fs::path ExecutablePath() {
    std::error_code ec;
    auto path = fs::read_symlink("/proc/self/exe", ec);
    return ec ? fs::path{} : path;
}

fs::path DataHome() {
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg == '/') return FromUtf8(xdg);
    const char* home = std::getenv("HOME");
    if (!home || !*home) throw std::runtime_error("HOME is not set");
    return FromUtf8(home) / ".local" / "share";
}

void ShowError(const char* title, const std::string& text) {
    if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, text.c_str(), nullptr))
        std::fprintf(stderr, "%s: %s\n", title, text.c_str());
}

void OpenFolder(const fs::path& folder) {
    if (!OnPath("xdg-open")) return;
    const pid_t child = fork();
    if (child == 0) {
        // A grandchild runs the file manager so this process never leaves a zombie behind.
        if (fork() == 0) {
            setsid();
            execlp("xdg-open", "xdg-open", folder.c_str(), static_cast<char*>(nullptr));
        }
        _exit(0);
    }
    if (child > 0) waitpid(child, nullptr, 0);
}

bool ChooseFile(fs::path& selected, bool folder, const std::string& title) {
    const char* session = std::getenv("XDG_CURRENT_DESKTOP");
    const bool kde = session && std::strstr(session, "KDE");
    std::string line;
    if (kde && OnPath("kdialog")) {
        const std::string command = folder ? "kdialog --title 'OpenShao' --getexistingdirectory \"$HOME\" 2>/dev/null"
                                           : "kdialog --title 'OpenShao' --getopenfilename \"$HOME\" "
                                             "'*.iso *.xiso *.zip default.xbe *.tar.xz *.ostp|Xbox game files and mods' 2>/dev/null";
        if (Capture(command, line)) { selected = FromUtf8(line); return true; }
        return false;
    }
    if (OnPath("zenity")) {
        const std::string command = folder ? "zenity --file-selection --directory --title='OpenShao - Select your game folder' 2>/dev/null"
                                           : "zenity --file-selection --title='OpenShao - Select your game' "
                                             "--file-filter='Xbox game files | *.iso *.ISO *.xiso *.zip *.ZIP default.xbe' "
                                             "--file-filter='Mod archives and texture packs | *.tar.xz *.ostp' "
                                             "--file-filter='All files | *' 2>/dev/null";
        if (Capture(command, line)) { selected = FromUtf8(line); return true; }
        return false;
    }
    return ChooseWithSdl(selected, folder, title);
}

}
