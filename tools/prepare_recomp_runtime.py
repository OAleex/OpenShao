"""Apply narrow host adaptations to a pinned SDK, writing only build outputs."""
import argparse
from pathlib import Path


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"SDK source changed at required host adaptation: {old[:70]!r}")
    return text.replace(old, new, 1)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sdk", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--shadow-diagnostics", action="store_true", help="Include optional D3D12 shadow diagnostics")
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    log = (args.sdk / "runtime/core/log.cpp").read_text(encoding="utf-8")
    log = '#include "diagnostic_privacy.h"\n' + log
    log = replace_once(log, '    va_end(ap);', '    va_end(ap);\n    const auto safe = openshao::privacy::Redact(msg);')
    log = log.replace('size_t(tid), msg)', 'size_t(tid), safe.c_str())').replace(' : "", msg)', ' : "", safe.c_str())')
    (args.out / "log.cpp").write_text(log, encoding="utf-8")
    text = (args.sdk / "runtime/app/main.cpp").read_text(encoding="utf-8")
    text = '#include "support_report.h"\n#include "host.h"\n#include "launcher.h"\n#include "language_pack.h"\n#include "window.h"\n#include "performance_overlay.h"\n#include "runtime_paths.h"\n#include <cstdlib>\n' + text
    text = replace_once(text, '#include <windows.h>', '#include <windows.h>\n#include <shobjidl.h>')
    text = replace_once(text, 'auto dir = ExeDir() / "CrashDumps";', 'auto dir = openshao::paths::Crashes();')
    text = replace_once(text, 'logx::Init((app::ExeDir() / "logs").string(), log, relaunched);',
        'logx::Init(openshao::paths::Logs().string(), log, relaunched);')
    text = replace_once(text, '    else app::LogSystem();', '')
    text = replace_once(text, '    // the dump holds no guest memory: the guest registers and return addresses go to the log\n',
        '    if (openshao::support::NotifyCrash(e && e->ExceptionRecord ? e->ExceptionRecord->ExceptionCode : 0,\n'
        '            e && e->ExceptionRecord ? reinterpret_cast<uintptr_t>(e->ExceptionRecord->ExceptionAddress) : 0, e))\n'
        '        return EXCEPTION_EXECUTE_HANDLER;\n'
        '    // Native launcher failures have no guest thread yet. Log them independently.\n'
        '    if (e && e->ExceptionRecord) {\n'
        '        const auto* record = e->ExceptionRecord;\n'
        '        LOGE("Native exception %08lX at %p (Windows thread %lu)", record->ExceptionCode, record->ExceptionAddress, GetCurrentThreadId());\n'
        '        if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2)\n'
        '            LOGE("Access violation: operation=%llu address=%p", static_cast<unsigned long long>(record->ExceptionInformation[0]), reinterpret_cast<void*>(record->ExceptionInformation[1]));\n'
        '    }\n')
    text = replace_once(text, '    CloseHandle(f);\n    logx::Flush();',
        '    CloseHandle(f);\n    logx::Flush();\n'
        '    openshao::paths::ReportError(L"OpenShao stopped because of an unexpected error.", dir / name);')
    text = replace_once(text, 'if (f == INVALID_HANDLE_VALUE) return EXCEPTION_CONTINUE_SEARCH;',
        'if (f == INVALID_HANDLE_VALUE) { LOGE("Cannot create crash report (Windows error %lu)", GetLastError()); openshao::paths::ReportError(L"OpenShao stopped. A crash report could not be written."); return EXCEPTION_CONTINUE_SEARCH; }')
    message_callback = '    if (intptr_t result = 0; kGame.message && kGame.message(hwnd, msg, wp, lp, &result)) return result;'
    text = replace_once(text, message_callback + '\n', '')
    text = replace_once(text, 'LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {',
        'LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {\n'
        '    if (openshao::performance::Message(msg, wp, lp)) return 0;\n' + message_callback)
    text = replace_once(text, "if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler", "if (!openshao::options.background && ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler")
    text = replace_once(text, "void ApplyNow() {", "void ApplyNow() {\n    if (openshao::options.background) return;")
    window_begin = text.index('    if (s.display_mode == DisplayMode::Windowed) {')
    window_end = text.index('\n}\n\nLRESULT CALLBACK WndProc', window_begin)
    text = replace_once(text, text[window_begin:window_end],
        '    if (!openshao::window::Place(g_hwnd, s.display_mode == DisplayMode::Windowed, s.width, s.height))\n'
        '        LOGE("Cannot apply the selected window mode (%lu)", GetLastError());')
    text = replace_once(text, '    Mode out = Output();\n', '')
    text = replace_once(text, '    LONG_PTR visible = IsWindowVisible(g_hwnd) ? WS_VISIBLE : 0;  // a window held back for a splash or handover stays so\n', '')
    text = replace_once(text, "case WM_ACTIVATE:", "case WM_ACTIVATE:\n            if (openshao::options.background) return 0;")
    text = replace_once(text, "case kOpenWindow:", "case kOpenWindow:\n            if (openshao::options.background) return 0;")
    text = replace_once(text, "int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {", "int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {\n    const int configured = openshao::Configure();\n    if (configured) return configured == 1 ? 0 : configured;")
    text = replace_once(text, "app::Load();",
        'LOGR("OpenShao version: %s", OPENSHAO_RECOMP_VERSION);\n'
        '    if (!openshao::options.background && openshao::options.launcher_test.empty() && !openshao::support::StartMonitor({openshao::paths::Root(), logx::Path(), OPENSHAO_RECOMP_VERSION}))\n'
        '        LOGE("Automatic crash report worker is unavailable; using the local error handler.");\n'
        '    LOGR("Startup stage: settings");\n'
        '    app::Load();\n    app::LogSystem();\n'
        '    LOGR("Startup stage: launcher");\n'
        '    if (openshao::options.background) {\n        app::Get().display_mode = app::DisplayMode::Windowed;\n        app::Get().width = 640; app::Get().height = 480;\n    }\n    openshao::StartWatchdog();')
    text = replace_once(text, '    openshao::StartWatchdog();',
        '    if (openshao::options.configure_only || (!openshao::options.background && !openshao::options.validate_disc && !openshao::options.validate_language_pack && !openshao::options.setup && !openshao::options.no_launcher)) {\n'
        '        if (!openshao::launcher::Show()) return 0;\n'
        '    }\n    openshao::StartWatchdog();')
    text = replace_once(text, '        app::Get().width = 640; app::Get().height = 480;',
        '        app::Get().width = 640; app::Get().height = 480;\n'
        '        if (std::getenv("XBE_CAPTURE_DIR")) if (const char* v = std::getenv("OPENSHAO_CAPTURE_RENDER_PERCENT")) {\n'
        '            const int percent = std::atoi(v);\n'
        '            if (percent >= 25 && percent <= 400) app::Get().render_percent = percent;\n'
        '        }')
    text = replace_once(text, "if (!app::FindGame(game)) return 1;", "if (!openshao::options.host_test && !app::FindGame(game)) return 3;\n    if (openshao::options.validate_disc) { std::printf(\"{\\\"valid\\\":true}\\n\"); return 0; }")
    text = replace_once(text, '    if (openshao::options.validate_disc) {',
        '    if (openshao::options.validate_language_pack) {\n'
        '        const auto& stats = openshao::language::Statistics();\n'
        '        std::printf("{\\"language_pack_valid\\":true,\\"direct_loading\\":true,\\"resources\\":%u,\\"voice_names\\":%u,\\"voice_occurrences\\":%u,\\"movies\\":%u}\\n",\n'
        '            stats.archive.resources, stats.archive.voice_names, stats.archive.voice_occurrences, stats.movies);\n'
        '        return 0;\n'
        '    }\n'
        '    if (openshao::options.validate_disc) {')
    text = replace_once(text, '    wc.hbrBackground =',
        '    wc.hIconSm = static_cast<HICON>(LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));\n    wc.hbrBackground =')
    text = replace_once(text, '    RegisterClassExW(&wc);',
        '    SetCurrentProcessExplicitAppUserModelID(L"OpenShao");\n    RegisterClassExW(&wc);')
    text = replace_once(text, "bool splash = app::OpenSplash(), handover = app::StartHandover();", "bool splash = !openshao::options.background && app::OpenSplash();\n    bool handover = !openshao::options.background && app::StartHandover();")
    text = replace_once(text, "CreateWindowExW(0, wc.lpszClassName", "CreateWindowExW(openshao::options.background ? WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW : 0, wc.lpszClassName")
    text = replace_once(text, "} else if (!splash) {", "} else if (!splash && !openshao::options.background) {")
    text = replace_once(text, "    IMGUI_CHECKVERSION();", "    if (!app::g_hwnd) return 4;\n    if (openshao::options.host_test) {\n        app::OpenGameWindow(); app::ApplyDisplay();\n        MSG pending;\n        while (PeekMessageW(&pending, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&pending);\n        return openshao::CheckHiddenWindow(app::g_hwnd, app::g_mode_changed);\n    }\n    IMGUI_CHECKVERSION();")
    text = replace_once(text, '        MessageBoxW(app::g_hwnd, L"The game failed to start, see the log in the logs folder.", app::kGame.short_title,\n                    MB_ICONERROR);',
        '        openshao::paths::ReportError(L"The game failed to start.");')
    text = replace_once(text, "    input::Init();", "    if (!openshao::options.background || openshao::options.virtual_gamepads) input::Init();")
    text = replace_once(text, '(app::ExeDir() / hdd).string()', 'save_directory.string()')
    text = replace_once(text, '    if (app::Instance())\n        SetWindowTextW',
        '    std::filesystem::path save_directory;\n'
        '    openshao::storage::DirectoryLock save_lock;\n'
        '    if (!openshao::OpenSaveDirectory(hdd, save_directory, save_lock)) return 6;\n'
        '    if (app::Instance())\n        SetWindowTextW')
    text = '#include "restart.h"\n' + text
    text = replace_once(text, 'bool relaunched = std::wcsstr(GetCommandLineW(), L"--relaunched") != nullptr;',
        'bool relaunched = openshao::options.resume_main_menu || std::wcsstr(GetCommandLineW(), L"--relaunched") != nullptr;')
    text = replace_once(text, '    app::Load();',
        '    app::Load();\n    { std::string error; if (!openshao::restart::ContinueStartup(error)) { LOGE("Language restart: %s", error.c_str()); return 8; } }')
    text = replace_once(text, 'bool splash = !openshao::options.background && app::OpenSplash();',
        'bool splash = !openshao::options.background && !openshao::options.resume_main_menu && app::OpenSplash();')
    (args.out / "main.cpp").write_text(text, encoding="utf-8")
    settings = (args.sdk / "runtime/app/settings.cpp").read_text(encoding="utf-8")
    settings = '#include "launcher.h"\n#include "runtime_paths.h"\n' + settings
    settings = replace_once(settings, 'return ExeDir() / kGame.settings;', 'return openshao::paths::Config();')
    wide_begin = settings.index('bool Widescreen() {')
    wide_end = settings.index('\nfloat RenderScale(', wide_begin)
    settings = settings[:wide_begin] + 'bool Widescreen() {\n    return openshao::aspect::Current() > openshao::aspect::Original + .0001f;\n}\n' + settings[wide_end:]
    # Linux: the desktop size comes from the SDL platform layer.
    settings = replace_once(settings, '#include <windows.h>\n', '#ifdef _WIN32\n#include <windows.h>\n#endif\n')
    settings = replace_once(settings, 'Mode Desktop() {\n', 'Mode PlatformDesktop();\nMode Desktop() {\n#ifndef _WIN32\n    return PlatformDesktop();\n#else\n')
    settings = replace_once(settings, '    return {uint32_t(GetSystemMetrics(SM_CXSCREEN)), uint32_t(GetSystemMetrics(SM_CYSCREEN))};\n}',
        '    return {uint32_t(GetSystemMetrics(SM_CXSCREEN)), uint32_t(GetSystemMetrics(SM_CYSCREEN))};\n#endif\n}')
    settings = replace_once(settings, '    std::ofstream f(File());\n    f << Table() << "\\n";',
        '    std::string error;\n    if (!openshao::launcher::Persist(Table(), error)) LOGE("Cannot save settings: %s", error.c_str());')
    (args.out / "settings.cpp").write_text(settings, encoding="utf-8")
    files = (args.sdk / "runtime/kernel/files.cpp").read_text(encoding="utf-8")
    files = '#ifdef _WIN32\n#ifndef NOMINMAX\n#define NOMINMAX\n#endif\n#include <windows.h>\n#undef CreateSymbolicLink\n#endif\n#include "save_storage.h"\n#include "runtime_paths.h"\n#include "language_pack.h"\n#include <cerrno>\n' + files
    for partition in ("X", "Y", "Z"):
        files = replace_once(files, f'= d / "{partition}";', f'= openshao::paths::Cache() / "xbox/{partition}";')
    files = replace_once(files, '    bool delete_on_close = false;', '    bool delete_on_close = false;\n    bool backed_up = false;')
    files = replace_once(files, '    fs::path d = data_dir;', '    openshao::storage::SetDataRoot(data_dir);\n    fs::path d = data_dir;')
    files = replace_once(files, '    FILE* fp = std::fopen(host.c_str(), mode);',
        '    bool backed_up = false;\n'
        '    if (exists && mode[0] == \'w\') {\n'
        '        uint32_t status = openshao::storage::BeforeWrite(host);\n'
        '        if (status) { LOGE("save backup failed before overwrite: %s (%08X)", host.c_str(), status); IoStatus(iosb, status, 0); return status; }\n'
        '        backed_up = true;\n'
        '    }\n'
        '    FILE* fp = std::fopen(host.c_str(), mode);')
    files = replace_once(files, '    f->delete_on_close = options & 0x1000;', '    f->backed_up = backed_up;\n    f->delete_on_close = options & 0x1000;')
    files = replace_once(files, '    bool backed_up = false;\n    if (exists && mode[0]',
        '    if (exists && (options & 0x1000)) {\n'
        '        uint32_t status = openshao::storage::BeforeWrite(host);\n'
        '        if (status) { IoStatus(iosb, status, 0); return status; }\n'
        '    }\n'
        '    bool backed_up = false;\n    if (exists && mode[0]')
    files = replace_once(files, '    uint64_t pos = Offset(*f, off);',
        '    if (write && !f->backed_up) {\n'
        '        uint32_t status = openshao::storage::BeforeWrite(f->host);\n'
        '        if (status) { LOGE("save backup failed before write: %s (%08X)", f->host.string().c_str(), status); return Finish(*f, status, 0, event, apc, apc_ctx, iosb); }\n'
        '        f->backed_up = true;\n'
        '    }\n'
        '    uint64_t pos = Offset(*f, off);')
    files = replace_once(files, '    if (write && n < len) return Finish(*f, 0xC000007F, n, event, apc, apc_ctx, iosb);  // disk full',
        '    if (write && n < len) return Finish(*f, errno == ENOSPC ? openshao::storage::kDiskFull : openshao::storage::kIoError, n, event, apc, apc_ctx, iosb);\n'
        '    if (write && openshao::storage::IsSave(f->host)) {\n'
        '        uint32_t status = openshao::storage::Flush(f->fp);\n'
        '        if (status) { LOGE("save write flush failed: %s (%08X)", f->host.string().c_str(), status); return Finish(*f, status, n, event, apc, apc_ctx, iosb); }\n'
        '    }')
    files = replace_once(files, '    if (f->fp) {\n        std::fflush(f->fp);\n        size = fs::file_size(f->host, ec);\n    }',
        '    if (f->fp) {\n'
        '        uint32_t status = openshao::storage::Flush(f->fp);\n'
        '        if (status) { IoStatus(iosb, status, 0); return status; }\n'
        '        size = fs::file_size(f->host, ec);\n'
        '        if (ec) { IoStatus(iosb, openshao::storage::kIoError, 0); return openshao::storage::kIoError; }\n'
        '    }')
    files = replace_once(files, '                std::fflush(f->fp);\n                uint64_t size = fs::file_size(f->host, ec);\n                if (cls == 20 || R64(in) < size) fs::resize_file(f->host, R64(in), ec);',
        '                uint32_t status = openshao::storage::Flush(f->fp);\n'
        '                if (status) { IoStatus(iosb, status, 0); return status; }\n'
        '                uint64_t size = fs::file_size(f->host, ec);\n'
        '                if (ec) { IoStatus(iosb, openshao::storage::kIoError, 0); return openshao::storage::kIoError; }\n'
        '                if ((cls == 20 || R64(in) < size) && R64(in) != size) {\n'
        '                    if (!f->backed_up) { status = openshao::storage::BeforeWrite(f->host); if (status) { IoStatus(iosb, status, 0); return status; } f->backed_up = true; }\n'
        '                    fs::resize_file(f->host, R64(in), ec);\n'
        '                    if (ec) { IoStatus(iosb, openshao::storage::kIoError, 0); return openshao::storage::kIoError; }\n'
        '                }')
    files = replace_once(files, '            f->delete_on_close = G<uint8_t>(in) != 0;',
        '            if (G<uint8_t>(in) && !f->backed_up) {\n'
        '                uint32_t status = openshao::storage::BeforeWrite(f->host);\n'
        '                if (status) { IoStatus(iosb, status, 0); return status; }\n'
        '                f->backed_up = true;\n'
        '            }\n'
        '            f->delete_on_close = G<uint8_t>(in) != 0;')
    files = replace_once(files, '            fs::rename(f->host, host, ec);',
        '            uint32_t status = openshao::storage::BeforeWrite(f->host);\n'
        '            if (!status) status = openshao::storage::BeforeWrite(host);\n'
        '            if (status) { IoStatus(iosb, status, 0); return status; }\n'
        '            fs::rename(f->host, host, ec);')
    files = replace_once(files, '    if (host.empty() || !fs::remove(host, ec)) return kObjectNameNotFound;',
        '    if (host.empty()) return kObjectNameNotFound;\n'
        '    uint32_t status = openshao::storage::BeforeWrite(host);\n'
        '    if (status) { LOGE("save backup failed before delete: %s (%08X)", host.c_str(), status); return status; }\n'
        '    if (!fs::remove(host, ec)) return kObjectNameNotFound;')
    flush_begin = files.index('KERNEL_EXPORT(198, NtFlushBuffersFile, 2) {')
    flush_end = files.index('\nKERNEL_EXPORT(200,', flush_begin)
    files = files[:flush_begin] + '''KERNEL_EXPORT(198, NtFlushBuffersFile, 2) {
    auto f = FileFromHandle(a[0]);
    uint32_t status = kInvalidHandle;
    if (f && f->fp) {
        std::lock_guard<std::mutex> lock(f->io);
        status = openshao::storage::Flush(f->fp);
        if (status) LOGE("file flush failed: %s (%08X)", f->host.string().c_str(), status);
    }
    IoStatus(a[1], status, 0);
    return status;
}
''' + files[flush_end:]
    volume_begin = files.index('    std::memset(HostPtr(out), 0, len);', files.index('KERNEL_EXPORT(218,'))
    files = files[:volume_begin] + '''    auto file = FileFromHandle(a[0]);
    if (!file) { IoStatus(iosb, kInvalidHandle, 0); return kInvalidHandle; }
    uint32_t needed = cls == 3 ? 0x18 : cls == 7 ? 0x20 : cls == 1 ? 0x18 : cls == 5 ? 0x10 : 0;
    if (needed && len < needed) { IoStatus(iosb, kBufferTooSmall, 0); return kBufferTooSmall; }
    uint64_t capacity = 0, available = 0, free = 0;
    if (cls == 3 || cls == 7) {
        std::error_code ec;
        const auto path = file->entry ? MountedDisc()->Path().parent_path() : file->dir ? file->host : file->host.parent_path();
        const auto space = fs::space(path, ec);
        if (ec) { IoStatus(iosb, openshao::storage::kIoError, 0); return openshao::storage::kIoError; }
        capacity = space.capacity / 16384; available = space.available / 16384; free = space.free / 16384;
    }
''' + files[volume_begin:]
    files = replace_once(files, 'case 3:  // FileFsSizeInformation: 16 GiB total, 8 GiB free in 16 KiB units', 'case 3:  // FileFsSizeInformation, real host space in 16 KiB units')
    files = files.replace('W64(out, 0x100000);', 'W64(out, capacity);').replace('W64(out + 8, 0x80000);', 'W64(out + 8, available);').replace('W64(out + 0x10, 0x80000);', 'W64(out + 0x10, free);')
    files = replace_once(files, '    bool dir = false;', '    bool read_only = false;\n    bool dir = false;')
    files = replace_once(files, '    bool disc = false;', '    bool read_only = false;\n    bool disc = false;')
    files = replace_once(files, '    if (best->first == kCdRom)\n',
        '    if (best->first == kCdRom) {\n'
        '        const auto overlay = openshao::language::OverlayFile(parts);\n'
        '        if (overlay) return {{}, true, false, nullptr, overlay};\n'
        '    }\n'
        '    if (best->first == kCdRom)\n')
    files = replace_once(files, 'return {{}, true, disc->Find(parts)};', 'return {{}, true, true, disc->Find(parts)};')
    files = replace_once(files, '    return {MatchCase(best->second, parts).string()};', '''    const bool read_only = best->first == kCdRom;
    if (read_only) {
        auto current = best->second;
        for (const auto& part : parts) {
            if (part == "..") return {};
            current /= part;
#ifdef _WIN32
            if (GetFileAttributesW(current.c_str()) != INVALID_FILE_ATTRIBUTES &&
                (GetFileAttributesW(current.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT)) return {};
#else
            std::error_code link_error;
            if (std::filesystem::is_symlink(current, link_error)) return {};
#endif
        }
    }
    return {MatchCase(best->second, parts).string(), read_only};''')
    files = replace_once(files,
        '    if (f->entry) FillNetworkInfo(out + 8, *f->disc_entries[i]);\n'
        '    else FillNetworkInfo(out + 8, f->entries[i].path());',
        '    const auto child = (f->read_only || f->entry) ? Resolve(f->xbox + "\\\\" + fname, 0) : Target{};\n'
        '    if (child.virtual_file) FillNetworkInfo(out + 8, 0, child.virtual_file->Size(), false);\n'
        '    else if (child.read_only && !child.disc && !child.host.empty()) FillNetworkInfo(out + 8, child.host);\n'
        '    else if (f->entry) FillNetworkInfo(out + 8, *f->disc_entries[i]);\n'
        '    else FillNetworkInfo(out + 8, f->entries[i].path());')
    files = replace_once(files, '        fs::create_directories(v, ec);', '        if (k != kCdRom) fs::create_directories(v, ec);')
    files = replace_once(files, '    bool exists = fs::exists(host, ec);', '''    bool exists = fs::exists(host, ec);
    if (t.read_only && (!exists || (disposition != 1 && disposition != 3) || (options & 0x1000))) {
        const auto status = exists ? kAccessDenied : kObjectNameNotFound;
        IoStatus(iosb, status, 0);
        return status;
    }''')
    files = files.replace('\n    f->host = host;', '\n    f->read_only = t.read_only;\n    f->host = host;')
    files = replace_once(files, '\n        f->host = host;', '\n        f->read_only = t.read_only;\n        f->host = host;')
    files = replace_once(files, '    FILE* fp = std::fopen(host.c_str(), mode);',
        '    if (t.read_only) mode = "rb";\n    FILE* fp = std::fopen(host.c_str(), mode);')
    files = replace_once(files, '    auto f = std::make_shared<File>();\n    f->read_only = t.read_only;\n    f->host = host;\n    f->xbox = Normalize(name);\n    f->dir = fs::is_directory(host, ec);',
        '    if (t.read_only && (options & 0x1000)) return kAccessDenied;\n    auto f = std::make_shared<File>();\n    f->read_only = t.read_only;\n    f->host = host;\n    f->xbox = Normalize(name);\n    f->dir = fs::is_directory(host, ec);')
    files = replace_once(files, 'f->fp = std::fopen(host.c_str(), "r+b");', 'f->fp = std::fopen(host.c_str(), t.read_only ? "rb" : "r+b");')
    files = replace_once(files, 'if (write && f->entry)', 'if (write && (f->entry || f->read_only))')
    files = replace_once(files, 'if (f->entry && cls != 14 && cls != 4)', 'if ((f->entry || f->read_only) && cls != 14 && cls != 4)')
    files = replace_once(files, '    if (f->fp) {\n        uint32_t status = openshao::storage::Flush(f->fp);',
        '    if (f->fp && !f->read_only) {\n        uint32_t status = openshao::storage::Flush(f->fp);')
    files = replace_once(files, '            std::string host = ResolvePath(target, root);',
        '            const auto destination = Resolve(target, root);\n            if (destination.read_only || destination.disc) return kAccessDenied;\n            std::string host = destination.host;')
    files = replace_once(files, '    std::string host = ResolvePath(ReadAnsiString(R32(a[0] + 4)), R32(a[0]));',
        '    const auto destination = Resolve(ReadAnsiString(R32(a[0] + 4)), R32(a[0]));\n    if (destination.read_only || destination.disc) return kAccessDenied;\n    std::string host = destination.host;')
    files = replace_once(files, '    if (f && f->fp) {\n        std::lock_guard<std::mutex> lock(f->io);',
        '    if (f && (f->read_only || f->entry)) status = kSuccess;\n    else if (f && f->fp) {\n        std::lock_guard<std::mutex> lock(f->io);')
    files = replace_once(files, '    fs::path host;',
        '    std::shared_ptr<const openshao::language::VirtualFile> virtual_file;\n    fs::path host;')
    files = replace_once(files, '    const Disc::Entry* entry = nullptr;  // there: the file or folder, null when missing',
        '    const Disc::Entry* entry = nullptr;  // there: the file or folder, null when missing\n'
        '    std::shared_ptr<const openshao::language::VirtualFile> virtual_file;')
    files = replace_once(files, 'uint32_t OpenOnDisc(', '''uint32_t OpenVirtual(const Target& target, const std::string& name, uint32_t handle_out,
                     uint32_t iosb, uint32_t disposition, uint32_t options) {
    const auto status = (disposition != 1 && disposition != 3) || (options & 0x1000) ? kAccessDenied :
                        options & 1 ? kNotADirectory : kSuccess;
    if (status) { IoStatus(iosb, status, 0); return status; }
    auto file = std::make_shared<File>();
    file->virtual_file = target.virtual_file; file->read_only = true; file->xbox = Normalize(name);
    file->host = MountedDisc() ? MountedDisc()->Path() : g_devices.at(kCdRom) / "virtual";
    W32(handle_out, NewHandle(file)); IoStatus(iosb, kSuccess, 1);
    LOG_FIRST(40, "open direct language file %s", name.c_str());
    return kSuccess;
}

uint32_t OpenOnDisc(''')
    files = replace_once(files, '    if (t.disc) return OpenOnDisc(t.entry, name, handle_out, iosb, disposition, options);',
        '    if (t.virtual_file) return OpenVirtual(t, name, handle_out, iosb, disposition, options);\n'
        '    if (t.disc) return OpenOnDisc(t.entry, name, handle_out, iosb, disposition, options);')
    files = replace_once(files, '    if (t.disc) return OpenOnDisc(t.entry, name, handle_out, iosb, 1, options);',
        '    if (t.virtual_file) return OpenVirtual(t, name, handle_out, iosb, 1, options);\n'
        '    if (t.disc) return OpenOnDisc(t.entry, name, handle_out, iosb, 1, options);')
    files = replace_once(files, '    Target t = Resolve(xbox_path, 0);\n    if (t.disc) {',
        '    Target t = Resolve(xbox_path, 0);\n'
        '    if (t.virtual_file) {\n'
        '        out.resize(size_t(t.virtual_file->Size())); size_t count;\n'
        '        return t.virtual_file->Read(0, out, count) && count == out.size();\n'
        '    }\n    if (t.disc) {')
    files = replace_once(files, 'if (!f || (!f->fp && !f->entry))', 'if (!f || (!f->fp && !f->entry && !f->virtual_file))')
    files = replace_once(files, '        size_t done = f->entry ?', '''        size_t done = 0;
        if (f->virtual_file) {
            if (!f->virtual_file->Read(pos + n, std::span<uint8_t>(static_cast<uint8_t*>(HostPtr(at)), chunk), done))
                return Finish(*f, openshao::storage::kIoError, n, event, apc, apc_ctx, iosb);
        } else done = f->entry ?''')
    files = replace_once(files, 'uint64_t size = f->dir ? 0 : f->entry ?',
        'uint64_t size = f->dir ? 0 : f->virtual_file ? f->virtual_file->Size() : f->entry ?')
    files = replace_once(files, 'uint64_t t = f->entry ?', 'uint64_t t = f->virtual_file ? 0 : f->entry ?')
    files = replace_once(files, '            if (f->entry) FillNetworkInfo(out, *f->entry);',
        '            if (f->virtual_file) FillNetworkInfo(out, 0, f->virtual_file->Size(), false);\n'
        '            else if (f->entry) FillNetworkInfo(out, *f->entry);')
    files = replace_once(files, '    if (t.disc ? !t.entry : t.host.empty() || !fs::exists(t.host, ec)) {',
        '    if (!t.virtual_file && (t.disc ? !t.entry : t.host.empty() || !fs::exists(t.host, ec))) {')
    files = replace_once(files, '    if (t.disc) FillNetworkInfo(a[1], *t.entry);',
        '    if (t.virtual_file) FillNetworkInfo(a[1], 0, t.virtual_file->Size(), false);\n'
        '    else if (t.disc) FillNetworkInfo(a[1], *t.entry);')
    (args.out / "files.cpp").write_text(files, encoding="utf-8")
    boot = (args.sdk / "runtime/kernel/boot.cpp").read_text(encoding="utf-8")
    boot = '#include "support_report.h"\n#include "host.h"\n#include "runtime_paths.h"\n#include "language_pack.h"\n#include "mods/runtime.h"\n' + boot
    boot = replace_once(boot, '    if (app::kGame.loaded) app::kGame.loaded();',
        '    std::string language_error;\n'
        '    if (!openshao::language::ApplyFonts(std::span<uint8_t>(g_base, X86ImageEnd), language_error)) {\n'
        '        LOGE("Language font setup failed: %s", language_error.c_str());\n'
        '        return false;\n'
        '    }\n'
        '    if (openshao::language::CreditsEnabled()) LOGR("Translated fonts applied to guest memory");\n'
        '    if (app::kGame.loaded) app::kGame.loaded();')
    boot = replace_once(boot,'    dispatch::Init();',
        '    dispatch::Init();\n'
        '    std::string mod_error;\n'
        '    if (!openshao::mods::StartRoutines(g_base, mod_error)) {\n'
        '        LOGE("Mod routine setup failed: %s", mod_error.c_str());\n'
        '        openshao::paths::ReportError(std::filesystem::path(std::u8string(mod_error.begin(), mod_error.end())).wstring());\n'
        '        return false;\n'
        '    }')
    boot = replace_once(boot, 'return g_launch_file.substr(0, g_launch_file.find_last_of("/\\\\") + 1) + "display.bin";',
        'return (openshao::paths::Cache() / "display.bin").string();')
    boot = replace_once(boot, '    std::_Exit(code);',
        '    if (code) openshao::paths::ReportError(L"The game stopped with error code " + std::to_wstring(code) + L".");\n    openshao::support::StopMonitor();\n    std::_Exit(code);')
    boot = replace_once(boot, "void Relaunch(const char* exe) {", "void Relaunch(const char* exe) {\n    if (openshao::options.background) { LOGE(\"Relaunch refused during isolated background test\"); Shutdown(50); }")
    (args.out / "boot.cpp").write_text(boot, encoding="utf-8")
    pads = (args.sdk / "runtime/core/input.cpp").read_text(encoding="utf-8")
    pads = '#include "host.h"\n' + pads
    pads = replace_once(pads, "Pad Poll(int port) {", "Pad Poll(int port) {\n    if (openshao::options.background) {\n        Pad p{}; p.connected = port == 0;\n        if (app::kGame.pad) app::kGame.pad(port, p);\n        return p;\n    }")
    pads = replace_once(pads, "void Rumble(int port, uint16_t left, uint16_t right) {", "void Rumble(int port, uint16_t left, uint16_t right) {\n    if (openshao::options.background) return;")
    (args.out / "input.cpp").write_text(pads, encoding="utf-8")
    xapi = (args.sdk / "runtime/xdk/xapi.cpp").read_text(encoding="utf-8")
    # Publish initial insertion through the asynchronous device poller. Immediate
    # publication is consumed by this title's startup XGetDevices before it opens
    # handles; subsequent XGetDeviceChanges then never reports an insertion.
    xapi = replace_once(xapi, "    UpdatePorts();\n    if (!g_poller)", "    if (!g_poller)")
    # The original main-menu callback stops when the character selector opens.
    # XInputGetState remains active on the guest thread. Switch ports before
    # the first state is returned to the new player, not on menu highlighting.
    xapi = replace_once(xapi, "HLE_STDCALL(XInputGetState, 2) {",
        "void SyncRetailFrontendMode();\nHLE_STDCALL(XInputGetState, 2) {\n    SyncRetailFrontendMode();")
    xapi = replace_once(xapi, "    g_handles[h] = {port, 0, {}};", "    g_handles[h] = {port, 0, {}};\n    LOGI(\"openshao input: open port=%u handle=%08X caller=%08X\", port, h, R32(ctx.esp));")
    xapi = replace_once(xapi, "    h.last = p;", """    if (!h.last.connected || h.last.buttons != p.buttons ||
        std::memcmp(h.last.analog, p.analog, sizeof(p.analog)) ||
        h.last.lx != p.lx || h.last.ly != p.ly || h.last.rx != p.rx || h.last.ry != p.ry)
        LOGI("openshao input: state port=%u packet=%u buttons=%04X A=%u B=%u X=%u Y=%u black=%u white=%u LT=%u RT=%u left=%d,%d right=%d,%d caller=%08X", port, h.packet, p.buttons, p.analog[0], p.analog[1], p.analog[2], p.analog[3], p.analog[4], p.analog[5], p.analog[6], p.analog[7], p.lx, p.ly, p.rx, p.ry, R32(ctx.esp));
    h.last = p;""")
    xapi += '\nX86_EXTERN_FUNC(__imp__sub_0048E398);\nX86_FUNC(sub_0048E398) {\n    const uint32_t handle = kernel::R32(ctx.esp + 4);\n    __imp__sub_0048E398(ctx, base);\n    LOGI("openshao input: original capabilities handle=%08X result=%u", handle, ctx.eax);\n}\n'
    xapi += '\nX86_EXTERN_FUNC(__imp__sub_0048E2A7);\nX86_FUNC(sub_0048E2A7) {\n    const uint32_t type = kernel::R32(ctx.esp + 4);\n    __imp__sub_0048E2A7(ctx, base);\n    LOGI("openshao input: enumeration type=%08X result=%u current=%u previous=%u", type, ctx.eax, kernel::R32(kGamepadType), kernel::R32(kGamepadType + 8));\n}\n'
    xapi += '\nX86_EXTERN_FUNC(__imp__sub_0048E2C9);\nX86_FUNC(sub_0048E2C9) {\n    const uint32_t ins = kernel::R32(ctx.esp + 8), rem = kernel::R32(ctx.esp + 12);\n    __imp__sub_0048E2C9(ctx, base);\n    static unsigned n = 0;\n    if (n++ < 3 || ctx.eax) LOGI("openshao input: changes result=%u insert=%u remove=%u initial=%u", ctx.eax, kernel::R32(ins), kernel::R32(rem), kernel::R32(0x50FA88) & 255);\n}\n'
    cap_begin = xapi.index('X86_FUNC(sub_0048E398) {')
    cap_end = xapi.index('\nX86_EXTERN_FUNC(__imp__sub_0048E2A7)', cap_begin)
    xapi = '#include "controller.h"\n' + xapi[:cap_begin] + '#include "controller_compat.inl"\n' + xapi[cap_end:]
    # Reuse the actual override body in the synthetic packed-ABI regression.
    (args.out / "capabilities_test.inl").write_text('#include "controller_compat.inl"\n', encoding="utf-8")
    state_begin = xapi.index('HLE_STDCALL(XInputGetState, 2) {')
    state_end = xapi.index('\n}', state_begin) + 2
    state_body = xapi[state_begin:state_end].replace('HLE_STDCALL(XInputGetState, 2)',
        'uint32_t ReadInputState(X86Context& ctx, const uint32_t* a)', 1)
    (args.out / "input_state_test.inl").write_text(state_body + '\n', encoding="utf-8")
    xapi += '\n#include "controller_mode.inl"\n'
    (args.out / "xapi.cpp").write_text(xapi, encoding="utf-8")
    audio = (args.sdk / "runtime/audio/xaudio2.cpp").read_text(encoding="utf-8")
    audio = '#include "host.h"\n#include "launcher.h"\n#include "scorpion_preview_audio.h"\n' + audio
    audio = replace_once(audio, "        voice->Start();", "        master->SetVolume(openshao::options.background ? 0.0f : openshao::launcher::AudioGain());\n        voice->Start();")
    audio = replace_once(audio, "            render(b, kFrames);", "            master->SetVolume(openshao::options.background ? 0.0f : openshao::launcher::AudioGain());\n            render(b, kFrames);")
    audio = replace_once(audio, "            render(b, kFrames);", "            render(b, kFrames);\n            openshao::scorpion::audio_preview::Mix(b, kFrames);")
    (args.out / "xaudio2.cpp").write_text(audio, encoding="utf-8")
    d3d = (args.sdk / "runtime/xdk/d3d8.cpp").read_text(encoding="utf-8")
    d3d = '#include "aspect_ratio.h"\n#include "sfd_lock_compat.h"\n#include "sfd_filter_compat.h"\n#include "vblank_compat.h"\n#include <array>\nstatic std::array<uint32_t, 13> g_openshao_present{};\n' + d3d
    # cstdint must precede the snapshot declaration when this TU is compiled alone.
    d3d = '#include "platform_compat.h"\n#include <cstdint>\n#include "core/diag.h"\n' + d3d
    d3d = replace_once(d3d, '    if (!h) h = 480;\n', '    if (!h) h = 480;\n    std::memcpy(g_openshao_present.data(), HostPtr(pp), sizeof(g_openshao_present));\n    g_openshao_present[0] = w; g_openshao_present[1] = h; g_openshao_present[3] = count;\n')
    d3d = replace_once(d3d, 'HLE_WRAP(Lock2DSurface, 6) {\n    uint32_t res = a[0];',
        'HLE_WRAP(Lock2DSurface, 6) {\n    uint32_t res = a[0];\n'
        '    if (openshao::sfd::NeedsWriteInvalidation(res, a[1], a[2], a[4], a[5])) {\n'
        '        gpu::MarkDirty(res);\n'
        '        LOG_FIRST(1, "openshao sfd: invalidating writable movie texture on LockRect");\n'
        '    }')
    d3d = replace_once(d3d, '// what the library\'s vertical blank interrupt leaves for the title: count, event, callback',
        'static openshao::VBlankClock g_openshao_vblank;\n'
        '// what the library\'s vertical blank interrupt leaves for the title: count, event, callback')
    d3d = replace_once(d3d, '        SetDev(XDK_CDevice_VBlankCount, count);',
        '        SetDev(XDK_CDevice_VBlankCount, count);\n        g_openshao_vblank.Tick();')
    d3d = replace_once(d3d, 'HLE_STDCALL(D3DDevice_BlockUntilVerticalBlank, 0) {\n    std::this_thread::sleep_for(std::chrono::milliseconds(16));',
        'HLE_STDCALL(D3DDevice_BlockUntilVerticalBlank, 0) {\n'
        '    if constexpr (kVBlank) g_openshao_vblank.WaitNext();\n'
        '    else std::this_thread::sleep_for(std::chrono::milliseconds(16));')
    d3d = replace_once(d3d, 'HLE_STDCALL(D3DDevice_SetSoftDisplayFilter, 1) { return 0; }',
        'HLE_STDCALL(D3DDevice_SetSoftDisplayFilter, 1) {\n'
        '    openshao::sfd::SetSoftDisplayFilter(a[0]);\n'
        '    LOG_FIRST(4, "openshao sfd: soft display filter=%u", a[0]);\n'
        '    return 0;\n'
        '}')
    # SFD playback performance: preserve the original guest frame counter and
    # report actual swaps once per second. Do not alter playback timing.
    d3d = replace_once(d3d, '    g_presents++;',
        '    g_presents++;\n'
        '    { static uint64_t last_tick=0; static uint32_t last_swap=0;\n'
        '      const uint64_t tick=GetTickCount64();\n'
        '      if (diag::Perf() && tick-last_tick>=1000) {\n'
        '          LOGI("openshao media: swaps_per_interval=%u ms=%llu media_frame=%u",\n'
        '               g_presents.load()-last_swap, (unsigned long long)(last_tick?tick-last_tick:0),\n'
        '               R32(0x006394BC));\n'
        '          last_tick=tick; last_swap=g_presents.load();\n'
        '      } }')
    d3d = replace_once(d3d, 'if (!(handle & 1) && (handle & 0xE) == 4) LOG_FIRST(1, "d3d: pretransformed immediate mode vertices are not supported");', 'if (!(handle & 1) && (handle & 0xE) == 4) LOG_FIRST(1, "openshao d3d: screen-coordinate immediate vertices");')
    # The original SetRenderTarget reaches SetViewport through
    # D3D_CommonSetMultiSampleModeAndScale. Match SetViewport's clipping reset.
    d3d = replace_once(d3d, '// SetRenderTarget resets the viewport to the whole target',
        '// SetRenderTarget resets the viewport and clipping to the whole target\n'
        '    g_state.scissor_count = 0;')
    d3d = replace_once(d3d, '    for (uint32_t i = 0; i < g_state.scissor_count * 4; i++) g_state.scissors[i] = R32(a[2] + i * 4);',
        '    for (uint32_t i = 0; i < g_state.scissor_count * 4; i++) g_state.scissors[i] = R32(a[2] + i * 4);\n'
        '    if (g_state.scissor_count) LOG_FIRST(64, "openshao scissor: count=%u exclusive=%u rect=%d,%d,%d,%d caller=%08X", g_state.scissor_count, a[1], int32_t(g_state.scissors[0]), int32_t(g_state.scissors[1]), int32_t(g_state.scissors[2]), int32_t(g_state.scissors[3]), R32(ctx.esp));')
    d3d += '\n#include "d3d_reset.inl"\n'
    (args.out / "d3d8.cpp").write_text(d3d, encoding="utf-8")
    # Compile the actual adapted API body in the state regression, with synthetic
    # surface/refcount services. Keep this fixture tied to the pinned SDK source.
    begin = d3d.index('HLE_STDCALL(D3DDevice_SetRenderTarget, 2) {')
    end = d3d.index('\nHLE_STDCALL(D3DDevice_GetBackBuffer2', begin)
    (args.out / "set_render_target_test.inl").write_text(d3d[begin:end], encoding="utf-8")
    dsound = (args.sdk / "runtime/xdk/dsound.cpp").read_text(encoding="utf-8")
    dsound += '\n#include "audio_compat.inl"\n'
    (args.out / "dsound.cpp").write_text(dsound, encoding="utf-8")
    for call in (
        'if (g_backend) g_backend->Draw(g_state, a[0], a[1], a[2], nullptr);',
        'if (g_backend) g_backend->Draw(g_state, a[0], 0, a[1], indices);',
        'g_backend->Draw(g_state, prim, 0, count, nullptr);',
    ):
        d3d = replace_once(d3d, call,
            'openshao::aspect::WithCanvasClip(g_state, [&] { ' + call + ' });')
    d3d = replace_once(d3d,
        '    g_state.streams[0] = {UpVertices(vertices, count * stride), stride};',
        '    g_state.streams[0] = {UpVertices(vertices, count * stride), stride};\n'
        '    const bool viewport_tint = openshao::aspect::ExpandViewportTint(\n'
        '        HostPtr(mem::VirtualFromPhysical(R32(g_state.streams[0].vb + 4))), count, stride);')
    d3d = replace_once(d3d,
        'openshao::aspect::WithCanvasClip(g_state, [&] { g_backend->Draw(g_state, prim, 0, count, nullptr); });',
        'openshao::aspect::WithCanvasClip(g_state, [&] { g_backend->Draw(g_state, prim, 0, count, nullptr); }, viewport_tint);')
    from prepare_pipeline_precompile import adapt_d3d8 as adapt_pipeline_programs
    d3d = adapt_pipeline_programs(d3d, replace_once)
    (args.out / "d3d8.cpp").write_text(d3d, encoding="utf-8")
    backend = (args.sdk / "runtime/gpu/d3d12.cpp").read_text(encoding="utf-8")
    backend = '#include "aspect_ratio.h"\n#include "host.h"\n#include "launcher.h"\n#include "bloom_effect_runtime.h"\n#include "runtime_paths.h"\n#include "raster_compat.h"\n#include "sfd_filter_compat.h"\n#include "shadow_depth_format.h"\n' + backend
    backend = replace_once(backend,
        '    bool wide = app::Widescreen();\n    float aspect = wide ? 16.0f / 9.0f : 4.0f / 3.0f;',
        '    float aspect = openshao::aspect::Output(bb_w_, bb_h_);')
    backend = replace_once(backend, '    if (!BindStageTexturesAndTargets(s, stages)) return;\n    PixelKey pk = MakePixelKey(s, stages.info);',
        '    if (!BindStageTexturesAndTargets(s, stages)) return;\n'
        '    if (!openshao::launcher::Current().bloom_effect &&\n'
        '        openshao::IsOriginalBloomEffectComposite(s, prim, count, stages.rt[0])) return;\n'
        '    PixelKey pk = MakePixelKey(s, stages.info);')
    backend = replace_once(backend,'    uint32_t bb_w_ = 0;',
        '    uint32_t last_presented_ = UINT32_MAX;\n    uint32_t bb_w_ = 0;')
    backend = replace_once(backend,'        float mul = 1;\n    };',
        '        float mul = 1;\n        bool direct = false;\n    };')
    backend = replace_once(backend,'    DrawFrame(t, bi);',
        '    DrawFrame(t, bi);\n    last_presented_ = bi;')
    backend = replace_once(backend,'    CapStep(Surface{});',
        '    CapStep(Surface{});\n'
        '    if (last_presented_ < kFrames) {\n'
        '        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_PRESENT;\n'
        '        CapSave({.res=bb_[last_presented_].Get(), .state=&state, .fmt=DXGI_FORMAT_B8G8R8A8_UNORM, .w=bb_w_, .h=bb_h_, .direct=true}, "presentation");\n'
        '    }')
    backend = replace_once(backend,
        '    ComPtr<ID3D12Resource> down = Shrink(img.res, *img.state, img.fmt, img.w, img.h, {.mul = img.mul});\n'
        '    std::vector<uint8_t> px;\n    if (!down || !ReadBack(down.Get(), img.w, img.h, px)) return;',
        '    const auto original_state = *img.state;\n'
        '    ComPtr<ID3D12Resource> down;\n'
        '    if (img.direct) {\n'
        '        down = img.res;\n'
        '        Transition(img.res, *img.state, D3D12_RESOURCE_STATE_COPY_SOURCE);\n'
        '    } else down = Shrink(img.res, *img.state, img.fmt, img.w, img.h, {.mul = img.mul});\n'
        '    std::vector<uint8_t> px;\n'
        '    const bool copied = down && ReadBack(down.Get(), img.w, img.h, px);\n'
        '    if (img.direct) Transition(img.res, *img.state, original_state);\n'
        '    if (!copied) return;')
    backend = replace_once(backend, '    bool tearing_ = false;',
        '    bool depth32_supported_ = false;\n    bool tearing_ = false;')
    backend = replace_once(backend, '    tearing_ = tear;\n',
        '    tearing_ = tear;\n'
        '    // Without tearing support, a disabled VSync still presents at the display refresh rate.\n'
        '    LOGR("D3D12 tearing support: %s", tearing_ ? "yes" : "no");\n')
    backend = replace_once(backend, '    if (adapter) adapter->GetDesc1(&ad);',
        '    if (adapter) adapter->GetDesc1(&ad);\n'
        '    D3D12_FEATURE_DATA_FORMAT_SUPPORT depth32{DXGI_FORMAT_D32_FLOAT}, sample32{DXGI_FORMAT_R32_FLOAT};\n'
        '    // Feature level 11_0 hardware is expected to pass. A failed check keeps offscreen depth maps in D24S8.\n'
        '    depth32_supported_ = SUCCEEDED(dev_->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &depth32, sizeof(depth32))) &&\n'
        '        SUCCEEDED(dev_->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &sample32, sizeof(sample32))) &&\n'
        '        (depth32.Support1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL) &&\n'
        '        (sample32.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) &&\n'
        '        (sample32.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE_COMPARISON);\n'
        '    if (!depth32_supported_) LOGE("d3d12: D32 depth sampling is unavailable; offscreen depth maps use D24S8");')
    backend = replace_once(backend, '        td.Format = DXGI_FORMAT_R24G8_TYPELESS;',
        '        const uint32_t guest_format = (s.format >> 8) & 0xFF;\n'
        '        const bool floating = openshao::FloatShadowDepth(depth32_supported_, guest_format, t.gw != guest_w_ || t.gh != guest_h_);\n'
        '        td.Format = floating ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_R24G8_TYPELESS;')
    backend = replace_once(backend, '        cv.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;',
        '        cv.Format = openshao::ShadowFormats(td.Format).view;')
    backend = replace_once(backend, '        dv.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;',
        '        dv.Format = openshao::ShadowFormats(t.res->GetDesc().Format).view;')
    backend = replace_once(backend, 'D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, one, 0, 0, nullptr);',
        'D3D12_CLEAR_FLAGS(D3D12_CLEAR_FLAG_DEPTH | (openshao::ShadowFormats(t.res->GetDesc().Format).stencil ? D3D12_CLEAR_FLAG_STENCIL : 0)), one, 0, 0, nullptr);')
    backend = replace_once(backend, '((c.flags & kClearStencil) ? D3D12_CLEAR_FLAG_STENCIL : 0)',
        '((c.flags & kClearStencil) && openshao::ShadowFormats(t.res->GetDesc().Format).stencil ? D3D12_CLEAR_FLAG_STENCIL : 0)')
    backend = replace_once(backend, '        cl_->ClearDepthStencilView(Cpu(dsv_heap_.Get(), t.view, dsv_inc_), cf,',
        '        if (cf) cl_->ClearDepthStencilView(Cpu(dsv_heap_.Get(), t.view, dsv_inc_), cf,')
    backend = replace_once(backend, '    k.sten = s.rs[kRsStencilEnable] != 0;',
        '    k.sten = s.rs[kRsStencilEnable] != 0 && openshao::ShadowFormats(bound_ds_->res->GetDesc().Format).stencil;')
    backend = replace_once(backend, 'struct PsoKey {', 'struct PsoKey {\n    uint32_t dsfmt;')
    backend = replace_once(backend, '    k.has_ds = bound_ds_ != nullptr;',
        '    k.has_ds = bound_ds_ != nullptr;\n'
        '    k.dsfmt = bound_ds_ ? openshao::ShadowFormats(bound_ds_->res->GetDesc().Format).view : 0;')
    backend = replace_once(backend, 'k.has_ds ? DXGI_FORMAT_D24_UNORM_S8_UINT : DXGI_FORMAT_UNKNOWN',
        'k.has_ds ? DXGI_FORMAT(k.dsfmt) : DXGI_FORMAT_UNKNOWN')
    backend = replace_once(backend, 'sv.Format = t.depth ? DXGI_FORMAT_R24_UNORM_X8_TYPELESS : t.fmt;',
        'sv.Format = t.depth ? openshao::ShadowFormats(t.res->GetDesc().Format).sample : t.fmt;')
    backend = replace_once(backend, 'DXGI_FORMAT fmt = t.depth ? DXGI_FORMAT_R24_UNORM_X8_TYPELESS : t.fmt;',
        'DXGI_FORMAT fmt = t.depth ? openshao::ShadowFormats(t.res->GetDesc().Format).sample : t.fmt;')
    # A D32 target scales each constant bias unit by the exponent of the primitive's depth.
    backend = replace_once(backend,
        "    // polygon offset: bias in depth buffer units, the target is always 24 bit; the slope is per console pixel,\n"
        "    // and an upscaled target's pixels are smaller\n",
        "    // polygon offset: the constant bias is in 24 bit UNORM units. A D32 target scales each unit by the exponent\n"
        "    // of the primitive's depth: equal from 0.5 to 1, smaller below. The slope is per console pixel, and an\n"
        "    // upscaled target's pixels are smaller\n")
    backend = replace_once(backend, '        k.zbias = int32_t(std::lround(Bits(SimpleState(s, kMDepthOffset)) * kDepth24Max / s.zscale));',
        '        k.zbias = int32_t(std::lround(Bits(SimpleState(s, kMDepthOffset)) * kDepth24Max / s.zscale));\n'
        '        if (k.zbias && bound_ds_->res->GetDesc().Format == DXGI_FORMAT_R32_TYPELESS)\n'
        '            LOG_FIRST(8, "d3d12: constant depth bias %d on a D32 depth map, depth scale %.0f", k.zbias, s.zscale);')
    backend = replace_once(backend, '    ID3D12PipelineState* EnsureDepthCopyPipeline();',
        '    ID3D12PipelineState* EnsureDepthCopyPipeline(DXGI_FORMAT format);')
    backend = replace_once(backend, 'ID3D12PipelineState* D3D12Backend::EnsureDepthCopyPipeline() {',
        'ID3D12PipelineState* D3D12Backend::EnsureDepthCopyPipeline(DXGI_FORMAT format) {')
    backend = replace_once(backend, 'auto& pso = blit_psos_[kDepthCopyKey];',
        'auto& pso = blit_psos_[kDepthCopyKey - (uint64_t(format) << 8)];')
    backend = replace_once(backend, '    p.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;', '    p.DSVFormat = format;')
    backend = replace_once(backend, '    sv.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;',
        '    sv.Format = openshao::ShadowFormats(src.res->GetDesc().Format).sample;')
    backend = replace_once(backend, 'ID3D12PipelineState* pso = EnsureDepthCopyPipeline();',
        'ID3D12PipelineState* pso = EnsureDepthCopyPipeline(openshao::ShadowFormats(dst.res->GetDesc().Format).view);')
    backend = replace_once(backend, '        res ? "" : " unbound");',
        '        res ? "" : " unbound");\n'
        '        const auto& texture_state = s.tss[size_t(i)];\n'
        '        fprintf(cap_, "\\n     material_stage %d levels=%u min=%u mag=%u mip=%u lod_bias=%.9g lod_bits=%08X max_level=%u aniso=%u address=%u,%u,%u border=%08X alpha_kill=%u color_key_op=%u color_key=%08X color_sign=%08X",\n'
        '            i, d.levels, texture_state[kTssMinFilter], texture_state[kTssMagFilter], texture_state[kTssMipFilter],\n'
        '            Bits(texture_state[kTssMipLodBias]), texture_state[kTssMipLodBias], texture_state[kTssMaxMipLevel], texture_state[kTssMaxAnisotropy],\n'
        '            texture_state[kTssAddressU], texture_state[kTssAddressV], texture_state[kTssAddressW], texture_state[kTssBorderColor],\n'
        '            texture_state[kTssAlphaKill], texture_state[kTssColorKeyOp], texture_state[kTssColorKeyColor], texture_state[kTssColorSign]);\n'
        '        if (res && stages.info[i].shadow) {\n'
        '            const auto gpu = res->GetDesc();\n'
        '            fprintf(cap_, "\\n     shadow_stage %d mode=%u gpu=%llux%u view_format=%u min=%u mag=%u address=%u,%u",\n'
        '                i, unsigned(stages.info[i].shadow), (unsigned long long)gpu.Width, gpu.Height, unsigned(stages.sv[i].Format),\n'
        '                s.tss[size_t(i)][kTssMinFilter], s.tss[size_t(i)][kTssMagFilter], s.tss[size_t(i)][kTssAddressU], s.tss[size_t(i)][kTssAddressV]);\n'
        '        }')
    backend = replace_once(backend, 'app::ExeDir() / diag::CaptureDir() / stamp',
        'openshao::paths::Captures() / diag::CaptureDir() / stamp')
    # Immediate XYZRHW sprites remain 2D even when the title orders its menu
    # layers with Z enabled. Their FLOAT4 UVs also need the unit-texture guard.
    backend = replace_once(backend, 'sh.quad = count <= kQuadVertices && !s.rs[kRsZEnable] && !points;',
        'sh.quad = count <= kQuadVertices && (!s.rs[kRsZEnable] || (!sh.prog && a.pretransformed)) && !points;')
    backend = replace_once(backend, 'bool UnitTexcoords(const Assembled& a) {',
        'bool UnitTexcoords(const Assembled& a, const DrawTextures& stages) {\n'
        '    if (openshao::UnitSpriteTexcoords(a)) {\n'
        '        bool guarded = true;\n'
        '        for (int i=0;i<4;++i) if (stages.res[i])\n'
        '            guarded &= !stages.rt[i] && stages.info[i].kind == kTex2D && !stages.info[i].rect;\n'
        '        if (guarded) return true;\n'
        '    }')
    backend = replace_once(backend, 'EnsureStageSamplers(s, stages, UnitTexcoords(a))',
        'EnsureStageSamplers(s, stages, UnitTexcoords(a, stages))')
    backend = replace_once(backend, '    Blit(t.res.Get(), t.fmt, uv, {.gamma = gamma_});',
        '    Blit(t.res.Get(), t.fmt, uv, {.gamma = gamma_, .taps = openshao::sfd::PresentationTaps()});')
    backend = replace_once(backend, 'struct PsoKey {', 'struct PsoKey {\n    uint32_t atlas_guard;')
    backend = replace_once(backend, '    BuildPipelineKey(s, a, sh.vh, sh.ph, k);',
        '    BuildPipelineKey(s, a, sh.vh, sh.ph, k);\n'
        '    bool atlas = false, target_texture = false;\n'
        '    for (int i = 0; i < 4; ++i) if (stages.res[i]) {\n'
        '        target_texture |= stages.rt[i] || stages.info[i].kind != kTex2D || stages.info[i].rect;\n'
        '        atlas |= !stages.rt[i] && stages.info[i].kind == kTex2D;\n'
        '    }\n'
        '    k.atlas_guard = sh.quad && a.topology == Topology::Triangles && atlas && !target_texture;')
    backend = replace_once(backend, '    if (k.topo == uint32_t(Topology::Points)) ExpandPoints(d);',
        '    if (k.topo == uint32_t(Topology::Points)) ExpandPoints(d);\n'
        '    if (k.atlas_guard) {\n'
        '        uint64_t hash;\n'
        '        ID3DBlob* gs = Compile(openshao::AtlasBoundsHlsl(), "main", "gs_5_0", hash);\n'
        '        if (!gs) return nullptr;\n'
        '        d.GS = {gs->GetBufferPointer(), gs->GetBufferSize()};\n'
        '    }')
    # The debug overlay's Win32 backend polls global keys/cursor in NewFrame.
    # Hidden probes use only their own window messages and do not need this UI.
    backend = replace_once(backend, "void D3D12Backend::RenderUi() {", "void D3D12Backend::RenderUi() {\n    if (openshao::options.background) return;")
    backend = replace_once(backend, '    CaptureTextures(s, stages);',
        '    fprintf(cap_, " viewport %u %u %u %u scissors %u blendop %X\\n", s.viewport.x, s.viewport.y, s.viewport.w, s.viewport.h, s.scissor_count, k.op);\n'
        '    fprintf(cap_, "     raster_state front_face=%X cull_mode=%X fill=%X back_fill=%X two_sided=%u normalize_normals=%u host_cull=%u host_ccw=%u\\n",\n'
        '        s.rs[kRsFrontFace], s.rs[kRsCullMode], s.rs[kRsFillMode], s.rs[kRsBackFillMode],\n'
        '        s.rs[kRsTwoSidedLighting], s.rs[kRsNormalizeNormals], k.cull, k.ccw);\n'
        '    fprintf(cap_, "     material_state alpha_test=%u alpha_func=%X alpha_ref=%u fog_enable=%u fog_table=%u fog_color=%08X sample_alpha=%08X msaa_enable=%u msaa_mask=%08X msaa_mode=%08X msaa_target_mode=%08X edge_aa=%u ps_texture_modes=%08X\\n",\n'
        '        SimpleState(s, kMAlphaTestEnable), SimpleState(s, kMAlphaFunc), SimpleState(s, kMAlphaRef) & 0xFF,\n'
        '        s.rs[kRsFogEnable], s.rs[kRsFogTableMode], s.rs[kRsFogColor], s.rs[kRsSampleAlpha],\n'
        '        s.rs[kRsMultiSampleAntiAlias], s.rs[kRsMultiSampleMask], s.rs[kRsMultiSampleMode], s.rs[kRsMultiSampleRenderTargetMode],\n'
        '        s.rs[kRsEdgeAntiAlias], s.rs[kRsPsTextureModes]);\n'
        '    fprintf(cap_, "     depth_state enable=%u shadow_func=%X viewport_z=%.9g,%.9g scale=%.9g dsv_format=%u\\n", s.rs[kRsZEnable], s.rs[kRsShadowFunc], s.viewport.min_z, s.viewport.max_z, s.zscale, k.dsfmt);\n'
        '    if (s.scissor_count) fprintf(cap_, "     scissor0 %d %d %d %d\\n", int32_t(s.scissors[0]), int32_t(s.scissors[1]), int32_t(s.scissors[2]), int32_t(s.scissors[3]));\n'
        '    if (auto* thread = kernel::Current(); thread && thread->ctx) fprintf(cap_, "     caller %08X\\n", kernel::R32(thread->ctx->esp));\n'
        '    CaptureTextures(s, stages);')
    backend = replace_once(backend, '    fprintf(cap_, "\\n     verts");',
        '    fprintf(cap_, "\\n     verts");\n'
        '    if (std::getenv("OPENSHAO_CAPTURE_HUD") && a.vertices.empty()) {\n'
        '        for (uint32_t i = 0; i < std::min(a.vertex_count, 4u); ++i) {\n'
        '            fprintf(cap_, "\\n     rawvertex %u", i);\n'
        '            for (uint32_t r = 0; r < 16; ++r) if (a.regs & (1u << r)) {\n'
        '                const auto& e = a.elements[r]; const auto& b = a.buffers[e.buffer];\n'
        '                const auto* p = b.src + i * b.stride + e.offset;\n'
        '                fprintf(cap_, " r%u fmt%u:", r, e.format);\n'
        '                for (uint32_t j = 0; j < std::min(16u, b.stride - uint32_t(e.offset)); ++j) fprintf(cap_, "%02X", p[j]);\n'
        '            }\n'
        '        }\n'
        '    }\n'
        '    if (a.vertices.empty() && a.buffer_count && a.elements[0].format == DXGI_FORMAT_R32G32B32A32_FLOAT) {\n'
        '        const auto& e = a.elements[0]; const auto& b = a.buffers[e.buffer];\n'
        '        for (uint32_t i = 0; i < std::min(a.vertex_count, 4u); ++i) {\n'
        '            float p[4]; std::memcpy(p, b.src + i * b.stride + e.offset, sizeof(p));\n'
        '            fprintf(cap_, " rawpos[%u]=%g,%g,%g,%g", i, p[0],p[1],p[2],p[3]);\n'
        '        }\n'
        '    }')
    backend = replace_once(backend, '    EndCountedDraw(query);',
        '    EndCountedDraw(query);\n'
        '    if (cap_) if (const char* cursor = std::getenv("OPENSHAO_CAPTURE_DRAW_STEPS")) {\n'
        '        const uint32_t draw_id = cap_i_ - 1;\n'
        '        while (*cursor) {\n'
        '            if (*cursor < \'0\' || *cursor > \'9\') break;\n'
        '            char* end = nullptr;\n'
        '            const unsigned long selected = std::strtoul(cursor, &end, 10);\n'
        '            if (end == cursor || (*end && *end != \',\')) break;\n'
        '            if (selected == draw_id) {\n'
        '                fprintf(cap_, "     selected_geometry draw=%u regs=%04X decode=%016llX vertex_count=%u draw_count=%u index_count=%u index16=%u base_vertex=%d topology=%u strip=%u pretransformed=%u buffer_count=%u converted_floats=%zu\\n",\n'
        '                    draw_id, unsigned(a.regs), (unsigned long long)a.decode, a.vertex_count, a.draw_count, a.index_count,\n'
        '                    unsigned(a.index16), a.base_vertex, unsigned(a.topology), unsigned(a.strip), unsigned(a.pretransformed), a.buffer_count, a.vertices.size());\n'
        '                auto save_geometry = [&](const char* suffix, const void* data, size_t bytes) {\n'
        '                    char name[96];\n'
        '                    snprintf(name, sizeof(name), "draw_%04u_%s", draw_id, suffix);\n'
        '                    bool written = false;\n'
        '                    if (data && bytes) if (FILE* file = _wfopen((cap_dir_ / name).c_str(), L"wb")) {\n'
        '                        written = fwrite(data, 1, bytes, file) == bytes;\n'
        '                        fclose(file);\n'
        '                    }\n'
        '                    fprintf(cap_, "     geometry_file draw=%u name=%s bytes=%zu written=%u\\n", draw_id, name, bytes, unsigned(written));\n'
        '                };\n'
        '                for (unsigned reg = 0; reg < a.elements.size(); ++reg) if (a.regs & (1u << reg)) {\n'
        '                    const auto& element = a.elements[reg];\n'
        '                    fprintf(cap_, "     geometry_element draw=%u reg=%u format=%u buffer=%u offset=%u decode=%u\\n",\n'
        '                        draw_id, reg, unsigned(element.format), unsigned(element.buffer), unsigned(element.offset), unsigned((a.decode >> (reg * 4)) & 15));\n'
        '                }\n'
        '                for (unsigned slot = 0; slot < a.buffer_count && slot < a.buffers.size(); ++slot) {\n'
        '                    const auto& buffer = a.buffers[slot];\n'
        '                    fprintf(cap_, "     geometry_buffer draw=%u slot=%u stride=%u bytes=%u\\n", draw_id, slot, buffer.stride, buffer.bytes);\n'
        '                    char suffix[32];\n'
        '                    snprintf(suffix, sizeof(suffix), "vb_%u.bin", slot);\n'
        '                    save_geometry(suffix, buffer.src, buffer.bytes);\n'
        '                }\n'
        '                if (a.index_count) save_geometry("ib.bin", a.index_src, size_t(a.index_count) * (a.index16 ? 2 : 4));\n'
        '                if (!a.vertices.empty()) save_geometry("converted.bin", a.vertices.data(), a.vertices.size() * sizeof(float));\n'
        '                save_geometry("vs_constants.bin", s.vsc.data(), sizeof(s.vsc));\n'
        '                save_geometry("draw_constants.bin", &c, sizeof(c));\n'
        '                if (Target* target = FindTarget(s.rt, false)) {\n'
        '                    char name[64];\n'
        '                    snprintf(name, sizeof(name), "draw_%04u_%08X", draw_id, target->data);\n'
        '                    CapSave({.res=target->res.Get(), .state=&target->state, .fmt=target->fmt,\n'
        '                        .w=target->w, .h=target->h, .mul=target->range}, name);\n'
        '                }\n'
        '                break;\n'
        '            }\n'
        '            if (!*end) break;\n'
        '            cursor = end + 1;\n'
        '        }\n'
        '    }\n'
        '    if (cap_ && std::getenv("OPENSHAO_CAPTURE_MENU_LAYERS") && a.pretransformed && count <= 6) {\n'
        '        if (Target* target = FindTarget(s.rt, false)) {\n'
        '            char name[48]; snprintf(name, sizeof(name), "%04u_menu_layer", cap_i_);\n'
        '            CapSave({.res=target->res.Get(), .state=&target->state, .fmt=target->fmt, .w=target->w, .h=target->h, .mul=target->range}, name);\n'
        '        }\n'
        '    }\n'
        '    if (cap_ && std::getenv("OPENSHAO_CAPTURE_HUD") && count == 4 && s.textures[0]) {\n'
        '        TextureDesc desc;\n'
        '        if (Describe(s.textures[0], desc) && desc.width == 256 && desc.height == 256)\n'
        '            if (Target* target = FindTarget(s.rt, false)) Capture(*target, "hud_quad", 1.0f);\n'
        '    }\n'
        '    if (cap_ && std::getenv("OPENSHAO_CAPTURE_SMALL_DRAWS") && count <= 4 &&\n'
        '        (s.vs_handle == 0x104 || s.vs_handle == 0x404))\n'
        '        if (Target* target = FindTarget(s.rt, false)) Capture(*target, "postfx_quad", 1.0f);')
    # Depth-only passes have no colour target for CapStep to follow.
    backend = replace_once(backend, '    CapStep(Surface{});',
        '    CapStep(Surface{});\n'
        '    for (auto& [key, target] : targets_) if (target.depth) {\n'
        '        char name[64];\n'
        '        snprintf(name, sizeof(name), "depth_%08X_%ux%u", target.data, target.gw, target.gh);\n'
        '        CapSave(target, name);\n'
        '    }')
    backend = replace_once(backend, '    if (Target* f = FindTarget(front, false)) CapSave(*f, "final");',
        '    if (Target* f = FindTarget(front, false)) {\n'
        '        CapSave(*f, "final");\n'
        '        if (std::getenv("OPENSHAO_CAPTURE_NATIVE_PIXELS"))\n'
        '            CapSave({.res=f->res.Get(), .state=&f->state, .fmt=f->fmt, .w=f->w, .h=f->h, .mul=f->range}, "final_native");\n'
        '    }')
    if args.shadow_diagnostics:
        from prepare_shadow_diagnostics import adapt_backend
        backend = adapt_backend(backend, replace_once)
    from prepare_graphics_cache import adapt_backend as adapt_graphics_cache
    backend = adapt_graphics_cache(backend, replace_once)
    from prepare_pipeline_precompile import adapt_backend as adapt_pipeline_precompile
    backend = adapt_pipeline_precompile(backend, replace_once)
    from prepare_perf_present import adapt_backend as adapt_perf_present
    backend = adapt_perf_present(backend, replace_once)
    (args.out / "d3d12.cpp").write_text(backend, encoding="utf-8")
    # Linux builds the render thread from this copy; Windows still compiles the SDK file.
    threaded = (args.sdk / "runtime/gpu/render_thread.cpp").read_text(encoding="utf-8")
    threaded = replace_once(threaded, '#include <windows.h>\n', '#ifdef _WIN32\n#include <windows.h>\n#else\n#include <pthread.h>\n#endif\n')
    threaded = replace_once(threaded,
        '        using SetDescription = HRESULT(WINAPI*)(HANDLE, PCWSTR);\n'
        '        if (auto set = reinterpret_cast<SetDescription>(\n'
        '                reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetThreadDescription"))))\n'
        '            set(GetCurrentThread(), L"render");\n',
        '#ifdef _WIN32\n'
        '        using SetDescription = HRESULT(WINAPI*)(HANDLE, PCWSTR);\n'
        '        if (auto set = reinterpret_cast<SetDescription>(\n'
        '                reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetThreadDescription"))))\n'
        '            set(GetCurrentThread(), L"render");\n'
        '#else\n'
        '        pthread_setname_np(pthread_self(), "render");\n'
        '#endif\n')
    (args.out / "render_thread.cpp").write_text(threaded, encoding="utf-8")
    vertex = (args.sdk / "runtime/gpu/vertex.cpp").read_text(encoding="utf-8")
    vertex = replace_once(vertex,
        '    return !(s.vs_handle & 1) && (s.vs_handle & 0xE) == 0x4;',
        '    // EndPrimitive tags its synthetic declaration with bit zero but keeps\n'
        '    // the source FVF position bits. Real SDK shader handles are aligned\n'
        '    // allocations OR 1, and programmable shaders keep their own output.\n'
        '    return (s.vs_handle & 0xE) == 0x4 &&\n'
        '           (!(s.vs_handle & 1) || (s.vs && s.vs->program.empty()));')
    from prepare_pipeline_layout import adapt_vertex as adapt_pipeline_layout
    vertex = adapt_pipeline_layout(vertex, replace_once)
    (args.out / "vertex.cpp").write_text(vertex, encoding="utf-8")
    shaders = (args.sdk / "runtime/gpu/shaders.cpp").read_text(encoding="utf-8")
    shaders = '#include "raster_compat.h"\n' + shaders
    if args.shadow_diagnostics:
        from prepare_shadow_diagnostics import adapt_shaders
        shaders = adapt_shaders(shaders, replace_once)
    quad_begin = shaders.index('std::string QuadHlsl(std::string s) {')
    quad_end = shaders.index('// a point covers', quad_begin)
    quad = replace_once(shaders[quad_begin:quad_end], '    return s;',
        '    return openshao::AtlasQuadHlsl(std::move(s));')
    shaders = shaders[:quad_begin] + quad + shaders[quad_end:]
    # Guest screen coordinates place pixel centers at integers. D3D12 places
    # them at n+0.5. Translate raster positions, preserving the title's UVs.
    # This epilogue serves programmable shaders and fixed XYZRHW vertices.
    shaders = replace_once(shaders, '    q.x = p.x * 2 / surf.x - 1;',
        '    q.x = (p.x + 0.5) * 2 / surf.x - 1;')
    shaders = replace_once(shaders, '    q.y = 1 - p.y * 2 / surf.y;',
        '    q.y = 1 - (p.y + 0.5) * 2 / surf.y;')
    # A sprite at x/y zero covers the first console sample. Expand only the
    # physical target border so upscaling does not expose the cleared column.
    shaders = replace_once(shaders,
        '    if (surf.w == 0 && abs(p.x - (surf.x - 0.5)) < 1e-3) p.x = surf.x;\n',
        '    if (surf.w == 0 && abs(p.x) < 1e-4) p.x = -0.5;\n'
        '    if (surf.w == 0 && abs(p.x - (surf.x - 0.5)) < 1e-3) p.x = surf.x;\n')
    shaders = replace_once(shaders,
        '    if (surf.w == 0 && abs(p.y - (surf.y - 0.5)) < 1e-3) p.y = surf.y;\n',
        '    if (surf.w == 0 && abs(p.y) < 1e-4) p.y = -0.5;\n'
        '    if (surf.w == 0 && abs(p.y - (surf.y - 0.5)) < 1e-3) p.y = surf.y;\n')
    shaders = replace_once(shaders,
        'sub("    if (surf.w == 0 && abs(p.x - (surf.x - 0.5)) < 1e-3) p.x = surf.x;\\n",',
        'sub("    if (surf.w == 0 && abs(p.x) < 1e-4) p.x = -0.5;\\n",')
    # Fixed XYZ vertices already have clip coordinates; the same translation
    # is one clip unit / target extent, multiplied by their homogeneous W.
    shaders = replace_once(shaders, '\n    o.pos = cp;\n',
        '\n    o.pos = cp;\n    o.pos.xy += float2(1 / surf.x, -1 / surf.y) * cp.w;\n')
    from prepare_pipeline_pixel_key import adapt_shaders as adapt_pipeline_pixel_key
    shaders = adapt_pipeline_pixel_key(shaders, replace_once)
    (args.out / "shaders.cpp").write_text(shaders, encoding="utf-8")


if __name__ == "__main__":
    main()
