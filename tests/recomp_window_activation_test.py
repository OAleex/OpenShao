"""Exercise the adapted fullscreen handlers without changing a monitor or desktop focus."""
import argparse
from pathlib import Path
import subprocess
import tempfile


FIXTURE = r'''
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <deque>
#include <iostream>
#include <stdexcept>

namespace openshao { struct Options { bool background = false; }; Options options; }
namespace kernel { bool g_relaunching = false; }
namespace app {
enum class DisplayMode { Fullscreen, Windowed, Borderless };
struct Settings {
    DisplayMode display_mode = DisplayMode::Fullscreen;
    unsigned width = 1280, height = 720;
};
Settings settings;
const Settings& Get() { return settings; }
HWND g_hwnd = reinterpret_cast<HWND>(uintptr_t(1));
constexpr UINT kApplyDisplay = WM_APP + 1;
bool g_mode_changed = false, g_apply_posted = false;
bool focused = true, iconic = false, visible = true;
unsigned changes = 0, background_changes = 0, placements = 0;
struct Message { UINT id; WPARAM parameter; };
std::deque<Message> pending;

HMONITOR Monitor(HWND, DWORD) { return reinterpret_cast<HMONITOR>(uintptr_t(1)); }
BOOL MonitorInfo(HMONITOR, MONITORINFOEXW*) { return TRUE; }
BOOL DesktopSettings(LPCWSTR, DWORD, DEVMODEW* mode) {
    mode->dmPelsWidth = 1920;
    mode->dmPelsHeight = 1080;
    return TRUE;
}
LONG ChangeMode(LPCWSTR, DEVMODEW* mode, HWND, DWORD, LPVOID) {
    if (mode) {
        ++changes;
        if (!focused) ++background_changes;
    }
    return DISP_CHANGE_SUCCESSFUL;
}
BOOL Show(HWND, int mode) { iconic = mode == SW_MINIMIZE; return TRUE; }
BOOL Post(HWND, UINT message, WPARAM parameter, LPARAM) {
    pending.push_back({message, parameter});
    return TRUE;
}
BOOL Visible(HWND) { return visible; }
BOOL Iconic(HWND) { return iconic; }
HWND Foreground() { return focused ? g_hwnd : reinterpret_cast<HWND>(uintptr_t(2)); }
}
namespace openshao::window {
bool Place(HWND, bool, unsigned, unsigned) { ++app::placements; return true; }
}
#define LOGE(...) ((void)0)
#define MonitorFromWindow app::Monitor
#define GetMonitorInfoW app::MonitorInfo
#define EnumDisplaySettingsW app::DesktopSettings
#define ChangeDisplaySettingsExW app::ChangeMode
#define ShowWindow app::Show
#define PostMessageW app::Post
#define IsWindowVisible app::Visible
#define IsIconic app::Iconic
#define GetForegroundWindow app::Foreground
namespace app {
'''

ASSERTIONS = r'''
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Focus(bool value) {
    focused = value;
    WindowMessage(WM_ACTIVATE, value ? WA_ACTIVE : WA_INACTIVE);
}
void Drain() {
    unsigned processed = 0;
    while (!pending.empty()) {
        Require(++processed <= 10, "Restoring focus must not create an endless display-apply loop");
        const auto message = pending.front();
        pending.pop_front();
        WindowMessage(message.id, message.parameter);
    }
}
void Reset() {
    settings = {};
    focused = visible = true;
    iconic = g_mode_changed = g_apply_posted = false;
    changes = background_changes = placements = 0;
    pending.clear();
}
}
int main() {
    using namespace app;
    try {
        ApplyNow();
        for (unsigned cycle = 0; cycle < 20; ++cycle) {
            Focus(false);
            Require(!g_mode_changed, "Losing focus must return the changed mode to the desktop");
            Focus(true);
            Drain();
            Require(g_mode_changed && !g_apply_posted, "Returning focus must restore fullscreen once");
        }
        Require(changes == 21 && background_changes == 0, "Ordinary switching must only restore while focused");

        Focus(false);
        Focus(true);
        Focus(false);
        Drain();
        Require(background_changes == 0 && !g_mode_changed && !g_apply_posted,
                "A stale focus message must not restore fullscreen over another active window");
        Focus(true);
        Drain();
        Require(g_mode_changed && changes == 22, "The next real return must restore the cancelled fullscreen request");

        Reset();
        for (unsigned event = 0; event < 10; ++event) Focus(true);
        Require(pending.size() == 1, "Repeated activation must queue a single fullscreen restore");
        Drain();
        Require(changes == 1 && !g_apply_posted, "The queued restoration must complete once");

        Reset();
        focused = false;
        WindowMessage(kApplyDisplay, 0);
        Require(changes == 1, "An explicit settings request must retain its normal display-apply path");

        Reset();
        openshao::options.background = true;
        Focus(false);
        Focus(true);
        ApplyNow();
        Require(changes == 0 && placements == 0 && pending.empty(), "Background diagnostics must retain their hidden window");
        openshao::options.background = false;

        for (const auto mode : {DisplayMode::Windowed, DisplayMode::Borderless}) {
            Reset();
            settings.display_mode = mode;
            Focus(false);
            Focus(true);
            Drain();
            Require(changes == 0 && pending.empty(), "Windowed and borderless activation must not change the monitor mode");
        }
        std::cout << "PASS: ordinary, rapid and duplicate activation, explicit settings, and background isolation\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--main', type=Path, required=True)
    args = parser.parse_args()
    source = args.main.read_text(encoding='utf-8')
    apply_begin = source.index('void ApplyNow() {')
    apply = source[apply_begin:source.index('\nLRESULT CALLBACK WndProc', apply_begin)]
    activation_begin = source.index('        case WM_ACTIVATE:')
    activation = source[activation_begin:source.index('        case kOpenWindow:', activation_begin)]
    program = FIXTURE + apply
    program += '\nLRESULT WindowMessage(UINT msg, WPARAM wp) { const auto hwnd = g_hwnd; switch (msg) {\n'
    program += activation + '\n} return 0; }\n' + ASSERTIONS
    with tempfile.TemporaryDirectory(prefix='openshao-window-activation-') as directory:
        root = Path(directory)
        cpp = root / 'activation.cpp'
        exe = root / 'activation.exe'
        cpp.write_text(program, encoding='utf-8')
        subprocess.run([str(args.compiler), '-std=c++20', str(cpp), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    main()
