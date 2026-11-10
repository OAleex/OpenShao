#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#include <vector>
#include "linux/linux_platform.h"
#include <SDL3/SDL.h>
#endif

#include <cstdio>
#include <cwchar>
#include <chrono>
#include <thread>

#include "host.h"
#include "cpu/x86_context.h"
#include "core/log.h"
#include "core/input.h"
#include "app/app.h"
#include "performance_overlay.h"
#include "runtime_paths.h"
#include "support_report.h"
#include "restart.h"
#include "netplay/session.h"
#include "launcher_settings.h"
#include <cerrno>
#include <limits>
#include <algorithm>

namespace openshao {
Options options;

bool RequestExit() {
#ifdef _WIN32
    return PostMessageW(static_cast<HWND>(app::Window()),WM_CLOSE,0,0)!=FALSE;
#else
    SDL_Event event{};event.type=SDL_EVENT_QUIT;return SDL_PushEvent(&event);
#endif
}

// Another instance holds the data folder: bring its window forward or say that it is open.
void ShowAlreadyOpen() {
    std::fprintf(stderr, "OpenShao is already open.\n");
    if (options.background || !options.launcher_test.empty()) return;
#ifdef _WIN32
    for (const wchar_t* kind : {L"OpenShaoLauncher", app::kGame.name}) {
        if (HWND window = FindWindowW(kind, nullptr)) {
            if (IsIconic(window)) ShowWindow(window, SW_RESTORE);
            SetForegroundWindow(window);
            return;
        }
    }
    MessageBoxW(nullptr, L"OpenShao is already open. Close it before starting another copy.", L"OpenShao", MB_OK | MB_ICONINFORMATION);
#else
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "OpenShao", "OpenShao is already open. Close it before starting another copy.", nullptr);
#endif
}

int Configure() {
#ifdef _WIN32
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 2;
#else
    std::vector<std::wstring> arguments = linux_platform::Arguments();
    std::vector<wchar_t*> pointers;
    for (auto& argument : arguments) pointers.push_back(argument.data());
    int argc = int(pointers.size());
    wchar_t** argv = pointers.data();
#endif
    if (argc == 3 && std::wstring(argv[1]) == L"--support-worker") {
        const auto task = std::filesystem::path(argv[2]);
#ifdef _WIN32
        LocalFree(argv);
#endif
        return support::RunWorker(task) == 0 ? 1 : 10;
    }
    bool invalid = false, explicit_timeout = false;
    auto number=[&](const wchar_t* value,uint64_t maximum) {
        wchar_t* end=nullptr;errno=0;
        const auto result=std::wcstoull(value,&end,10);
        if(!*value || *value==L'-' || *value==L'+' || *end || errno || result==0 || result>maximum)invalid=true;
        return result;
    };
    for (int i = 1; i < argc && !invalid; ++i) {
        const std::wstring arg = argv[i];
        if (arg == L"--background") options.background = true;
        else if (arg == L"--info") options.info = true;
        else if (arg == L"--host-test") options.host_test = true;
        else if (arg == L"--setup") options.setup = true;
        else if (arg == L"--no-launcher") options.no_launcher = true;
        else if (arg == L"--resume-main-menu") {options.resume_main_menu=true;options.no_launcher=true;}
        else if (arg == L"--restart-parent" && i+1<argc) options.restart_parent=uint32_t(number(argv[++i],UINT32_MAX));
        else if (arg == L"--restart-ready-handle" && i+1<argc) options.restart_ready=uintptr_t(number(argv[++i],std::numeric_limits<uintptr_t>::max()));
        else if (arg == L"--restart-language" && i+1<argc) {
            const std::wstring value=argv[++i];options.restart_language.assign(value.begin(),value.end());
        }
        else if(arg==L"--restart-voice-language" && i+1<argc) {
            const std::wstring value=argv[++i];options.restart_voice_language.assign(value.begin(),value.end());
        }
        else if (arg == L"--configure") options.configure_only = true;
        else if (arg == L"--validate-language-pack") options.validate_language_pack = true;
        else if (arg == L"--language-pack" && i + 1 < argc) options.language_pack = argv[++i];
        else if (arg == L"--paths") options.paths = true;
        else if (arg == L"--support-report") options.support_report = true;
        else if (arg == L"--data-dir" && i + 1 < argc) options.data_dir = argv[++i];
        else if ((arg == L"--register-disc" || arg == L"--validate-disc") && i + 1 < argc) {
            options.validate_disc = true;
            options.register_disc = arg == L"--register-disc";
            options.disc = argv[++i];
        }
        else if (arg == L"--portable-saves") options.portable_saves = true;
        else if (arg == L"--save-dir" && i + 1 < argc) {
            options.save_dir = argv[++i];
            if (options.save_dir.empty()) invalid = true;
        }
        else if (arg == L"--virtual-gamepads" && i + 1 < argc) {
            const std::wstring count=argv[++i];
            if (count.size()!=1 || count[0]<L'1' || count[0]>L'3') invalid=true;
            else options.virtual_gamepads=unsigned(count[0]-L'0');
        }
        else if (arg == L"--launcher-test" && i + 1 < argc) options.launcher_test = argv[++i];
        else if (arg == L"--game" && i + 1 < argc) options.game = argv[++i];
        else if (arg == L"--disc" && i + 1 < argc) options.disc = argv[++i];
        else if (arg == L"--record" && i + 1 < argc) options.record = argv[++i];
        else if (arg == L"--replay" && i + 1 < argc) options.replay = argv[++i];
        else if (arg == L"--hash-out" && i + 1 < argc) options.hash_out = argv[++i];
        else if (arg == L"--hash-every" && i + 1 < argc) options.hash_every = unsigned(number(argv[++i], 36000));
        else if (arg == L"--timeout" && i + 1 < argc) {
            const wchar_t* value = argv[++i];
            wchar_t* end = nullptr;
            unsigned long seconds = std::wcstoul(value, &end, 10);
            // A replay runs as long as its recording, so it may take up to an hour.
            if (!*value || *end || seconds > 3600 || *value == L'-') invalid = true;
            else { options.timeout = unsigned(seconds); explicit_timeout = true; }
        } else invalid = true;
    }
#ifdef _WIN32
    LocalFree(argv);
#endif
    if (options.background && !explicit_timeout) options.timeout = 20;
    if (options.timeout > 600 && options.replay.empty()) invalid = true;
    if (!options.record.empty() && !options.replay.empty()) invalid = true;
    if ((options.hash_every || !options.hash_out.empty()) && options.record.empty() && options.replay.empty()) invalid = true;
    if (!options.record.empty() && (options.hash_every != 0) != !options.hash_out.empty()) invalid = true;
    if (!options.replay.empty() && options.hash_out.empty() && options.hash_every) invalid = true;
    if ((!options.record.empty() || !options.replay.empty()) && (options.host_test || options.setup || options.configure_only ||
        options.validate_disc || options.validate_language_pack || options.resume_main_menu || options.virtual_gamepads)) invalid = true;
    if ((options.background && !options.timeout) || (options.host_test && !options.background)) invalid = true;
    if (options.virtual_gamepads && (!options.background || options.host_test)) invalid=true;
    if (options.portable_saves && !options.save_dir.empty()) invalid = true;
    if (options.setup && (options.background || options.host_test || options.validate_disc || options.validate_language_pack)) invalid = true;
    if (options.configure_only && (options.background || options.setup || options.no_launcher || options.validate_disc || options.validate_language_pack)) invalid = true;
    if (!options.disc.empty() && !options.game.empty()) invalid = true;
    if (!options.launcher_test.empty() && (options.background || options.no_launcher || options.setup || options.validate_disc ||
                                           options.validate_language_pack || options.data_dir.empty())) invalid = true;
    if(options.restart_parent || options.restart_ready || !options.restart_language.empty() || !options.restart_voice_language.empty()) {
        if(std::find(launcher::voice_languages.begin(),launcher::voice_languages.end(),options.restart_voice_language)==launcher::voice_languages.end() ||
           !options.restart_parent || !options.restart_ready || !options.resume_main_menu ||
           std::find(launcher::languages.begin(),launcher::languages.end(),options.restart_language)==launcher::languages.end() || options.data_dir.empty() || options.save_dir.empty() ||
           options.configure_only || options.setup || options.info || options.paths || options.host_test ||
           options.validate_disc || options.validate_language_pack)invalid=true;
    }
    if (invalid) {
        std::fprintf(stderr, "Usage: OpenShao [--configure | --no-launcher | --setup] [--disc game.iso | --game folder] [--register-disc game.iso | --validate-disc game.iso] [--validate-language-pack] [--language-pack folder] [--paths] [--data-dir folder] [--save-dir folder | --portable-saves] [--record file.smrec | --replay file.smrec] [--hash-every frames --hash-out report.jsonl]\n");
        return 2;
    }
#ifdef _WIN32
    if (options.background) SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
#endif
    if (options.info) {
        size_t count = 0;
        while (X86FuncMappings[count].host) ++count;
        std::printf("{\"host_bits\":%zu,\"title_id\":\"0x4D570029\",\"entry\":\"0x%08X\",\"mapped_functions\":%zu}\n",
                    sizeof(void*) * 8, X86EntryPoint, count);
        std::fflush(stdout);
        return 1;
    }
    std::string error;
    if (!paths::Initialize(app::ExeDir(), options.background, options.data_dir, error, options.paths || options.support_report || options.restart_parent!=0)) {
        if (paths::DataInUse()) { ShowAlreadyOpen(); return kAlreadyOpen; }
        // The normal log cannot open until the data folder is ready.
        try {
            const auto fallback = std::filesystem::temp_directory_path() / "OpenShao" / "startup";
#ifdef _WIN32
            const auto name = "openshao-startup-" + std::to_string(GetCurrentProcessId());
#else
            const auto name = "openshao-startup-" + std::to_string(getpid());
#endif
            logx::Init(fallback.string(), name);
        } catch (...) {}
        LOGE("Cannot prepare application data: %s", error.c_str());
        const auto detail = std::filesystem::path(std::u8string(error.begin(), error.end())).wstring();
        paths::ReportError(L"The application folders could not be prepared.\n\n" + detail);
        return 6;
    }
    if (options.paths) {
        std::printf("%s\n", paths::Description().c_str());
        return 1;
    }
    if (options.support_report) {
        const auto report = support::Generate({paths::Root(), {}, OPENSHAO_RECOMP_VERSION});
        if (report.zip.empty()) { std::fprintf(stderr, "%s\n", report.error.c_str()); return 10; }
        const auto path = report.zip.u8string();
        std::printf("%s\n", std::string(path.begin(), path.end()).c_str());
        return 1;
    }
    options.data_dir = paths::Root();
    netplay::session::Configure({options.record, options.replay, options.hash_out, options.hash_every});
    return 0;
}

bool OpenSaveDirectory(const std::wstring& name, std::filesystem::path& result, storage::DirectoryLock& lock) {
    std::string error;
    try {
        const auto legacy = app::ExeDir() / name;
        const bool migrate = !options.background && !options.portable_saves && options.save_dir.empty();
        if (!options.save_dir.empty()) result = std::filesystem::absolute(options.save_dir);
        else if (!migrate) result = legacy;
        else {
            auto base = paths::SaveRoot();
            if (!base.empty()) result = base / name;
        }
        if (!result.empty() && storage::PrepareDirectory(migrate ? legacy : std::filesystem::path{}, result, error) && lock.Acquire(result, error)) {
#ifdef _WIN32
            LOGR("Save directory: %ls%s", result.c_str(), migrate ? " (configured save location)" : " (explicit/portable/isolated)");
#else
            LOGR("Save directory: %s%s", result.string().c_str(), migrate ? " (configured save location)" : " (explicit/portable/isolated)");
#endif
            restart::SaveDirectory(result);
            if (netplay::session::Requested() && !netplay::session::Start(result, error)) {
                // A replay under other conditions would measure the wrong thing; stop before boot.
                LOGE("netplay: %s", error.c_str());
                logx::Flush();
                std::fprintf(stderr, "%s\n", error.c_str());
                std::exit(netplay::session::kMismatch);
            }
            return true;
        }
    } catch (const std::exception& e) { error = e.what(); }
    LOGE("Could not prepare save directory: %s", error.c_str());
    paths::ReportError(L"Your save folder could not be opened. Close other copies of the game and check folder permissions. Your existing saves remain intact.");
    return false;
}

void StartWatchdog() {
    if (!options.timeout) return;
    std::thread([seconds = options.timeout] {
#ifdef _WIN32
        Sleep(seconds * 1000);
#else
        std::this_thread::sleep_for(std::chrono::seconds(seconds));
#endif
        LOGR("OpenShao diagnostic timeout (%u seconds)", seconds);
        logx::Flush();
#ifdef _WIN32
        TerminateProcess(GetCurrentProcess(), 7);
#else
        _exit(7);
#endif
    }).detach();
}

#ifndef _WIN32
int CheckHiddenWindow(void*, bool) {
    std::printf("{\"error\":\"--host-test checks the Windows message path\"}\n");
    return 4;
}
#else
int CheckHiddenWindow(void* raw, bool display_changed) {
    HWND window = static_cast<HWND>(raw);
    bool visible = IsWindowVisible(window) != FALSE;
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    bool owned = IsWindow(window) && pid == GetCurrentProcessId();
    const auto pad = input::Poll(0);
    const auto other = input::Poll(1);
    bool isolated = pad.connected && !pad.buttons && !pad.lx && !pad.ly && !pad.rx && !pad.ry && !other.connected;
    for (auto analog : pad.analog) isolated = isolated && !analog;
    // Exercise WndProc and SDK Poll only through this process's verified hidden window.
    bool keyboard = owned && !visible && options.background;
    if (keyboard) {
        SendMessageW(window, WM_KEYDOWN, VK_RETURN, 0);
        keyboard = input::Poll(0).buttons == 0x10 && !input::Poll(1).connected;
        SendMessageW(window, WM_KEYDOWN, VK_SPACE, 0);
        keyboard = keyboard && input::Poll(0).analog[0] == 255 && input::Poll(0).buttons == 0x10;
        SendMessageW(window, WM_KEYUP, VK_RETURN, 0);
        keyboard = keyboard && !input::Poll(0).buttons && input::Poll(0).analog[0] == 255;
        SendMessageW(window, WM_KEYUP, VK_SPACE, 0);
        keyboard = keyboard && !input::Poll(0).analog[0];
        SendMessageW(window, WM_KEYDOWN, VK_LEFT, 0);
        SendMessageW(window, WM_KEYDOWN, 'W', 0);
        SendMessageW(window, WM_KEYDOWN, 'A', 0);
        keyboard = keyboard && input::Poll(0).buttons == 4 && input::Poll(0).ly == 32767 && input::Poll(0).lx == -32768;
        SendMessageW(window, WM_KEYDOWN, 'D', 0);
        keyboard = keyboard && !input::Poll(0).lx;
        SendMessageW(window, WM_KILLFOCUS, 0, 0);
        auto released = input::Poll(0);
        keyboard = keyboard && !released.buttons && !released.lx && !released.ly && !released.analog[0];
        const bool initial_overlay = performance::Visible();
        SendMessageW(window, WM_KEYDOWN, VK_F9, 0);
        keyboard = keyboard && performance::Visible() != initial_overlay;
        SendMessageW(window, WM_KEYDOWN, VK_F9, LPARAM(1) << 30);
        SendMessageW(window, WM_KEYUP, VK_F9, 0);
        keyboard = keyboard && performance::Visible() != initial_overlay;
        SendMessageW(window, WM_KEYDOWN, VK_F9, 0);
        SendMessageW(window, WM_KEYUP, VK_F9, 0);
        keyboard = keyboard && performance::Visible() == initial_overlay;
    }
    std::printf("{\"background\":%s,\"visible\":%s,\"display_changed\":%s,\"isolated_input\":%s,\"keyboard_messages\":%s}\n",
                options.background ? "true" : "false", visible ? "true" : "false", display_changed ? "true" : "false", isolated ? "true" : "false", keyboard ? "true" : "false");
    std::fflush(stdout);
    return owned && options.background && !visible && !display_changed && isolated && keyboard ? 0 : 4;
}
#endif
}
