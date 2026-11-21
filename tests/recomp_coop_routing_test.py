"""Isolated real-XBE regression: a gamepad enters Ko-op, then keyboard is P1."""
import argparse
import ctypes
from ctypes import wintypes
from pathlib import Path
import shutil
import subprocess
import time


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--exe", required=True, type=Path)
    p.add_argument("--disc", required=True, type=Path)
    p.add_argument("--output", required=True, type=Path)
    args = p.parse_args()

    folder = args.output.resolve() / str(time.time_ns())
    folder.mkdir(parents=True)
    exe = folder / args.exe.name
    shutil.copy2(args.exe.resolve(), exe)
    user = ctypes.WinDLL("user32", use_last_error=True)
    callback = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user.EnumWindows.argtypes = [callback, wintypes.LPARAM]
    user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user.GetClassNameW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
    user.IsWindowVisible.argtypes = [wintypes.HWND]
    user.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
    WM_KEYDOWN, WM_KEYUP, WM_APP = 0x100, 0x101, 0x8000
    log_path = folder / "native.log"
    duration = 105
    stages = []
    actions = []
    hw = [None]

    with log_path.open("wb") as logfile:
        process = subprocess.Popen(
            [str(exe), "--background", "--virtual-gamepads", "1",
             "--timeout", str(duration), "--disc", str(args.disc.resolve())],
            cwd=folder, stdout=logfile, stderr=subprocess.STDOUT,
            creationflags=subprocess.CREATE_NO_WINDOW | subprocess.BELOW_NORMAL_PRIORITY_CLASS)

        def discover(hwnd, _):
            owner = wintypes.DWORD()
            user.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
            if owner.value == process.pid:
                name = ctypes.create_unicode_buffer(256)
                user.GetClassNameW(hwnd, name, len(name))
                if name.value == "OpenShao":
                    assert not user.IsWindowVisible(hwnd), "Diagnostic window became visible"
                    hw[0] = hwnd
            return True

        cb = callback(discover)
        started = time.monotonic()

        def send(msg, key, value):
            assert hw[0], "No owned hidden window"
            assert user.PostMessageW(hw[0], msg, key, value), "Cannot post game-owned input"
            actions.append((round(time.monotonic() - started, 2), msg, key, value))

        def key(code):
            send(WM_KEYDOWN, code, 0)
            time.sleep(.18)
            send(WM_KEYUP, code, 0)

        def pad(button):
            send(WM_APP + 0x51, button, 1)
            time.sleep(.18)
            send(WM_APP + 0x51, button, 0)

        try:
            while process.poll() is None and time.monotonic() - started < duration + 6:
                user.EnumWindows(cb, 0)
                elapsed = time.monotonic() - started
                log = log_path.read_text(encoding="utf-8", errors="replace")
                if elapsed >= 23 and "start" not in stages and hw[0]:
                    key(13)  # START, required to pass title screen
                    stages.append("start")
                if elapsed >= 31 and "brightness" not in stages and hw[0]:
                    key(32)  # A, brightness prompt
                    stages.append("brightness")
                if "input: retail menu top=00598430 selected=0" in log and "navigate" not in stages:
                    pad(12)  # SDL DPAD_DOWN: highlight Ko-op via controller
                    stages.append("navigate")
                if "navigate" in stages and "input: retail menu top=00598430 selected=1" in log and "enter" not in stages:
                    pad(0)  # SDL south/A: confirm Ko-op via controller
                    stages.append("enter")
                if "enter" in stages and "input: multiplayer=1 keyboard=1 reserved_port=0" in log and "separate" not in stages:
                    pad(0)  # PS4 P2 confirms character
                    time.sleep(.4)
                    key(32)  # keyboard P1 confirms character
                    stages.append("separate")
                if "separate" in stages:
                    break
                time.sleep(.15)
        finally:
            if process.poll() is None:
                process.terminate()  # strictly the child owned by this test
            process.wait(timeout=8)

    log = log_path.read_text(encoding="utf-8", errors="replace")
    print("Test:", folder, "stages:", stages, "actions:", actions, flush=True)
    assert "navigate" in stages and "enter" in stages, "Ko-op menu not entered"
    assert "separate" in stages, "Guest character selector did not trigger Ko-op P1/P2 split"
    split = log.index("input: multiplayer=1 keyboard=1 reserved_port=0")
    after = log[split:]
    assert "openshao input: state port=1" in after, "Gamepad input did not reach player two"
    assert "openshao input: state port=0" in after, "Keyboard input did not reach player one"
    print("PASS: original character selector split keyboard P1 + gamepad P2")


if __name__ == "__main__":
    main()
