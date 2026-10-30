"""Reproduce the real startup Reset without showing a window or touching user saves."""
import argparse
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import shutil
import subprocess
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--disc', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--start', action='store_true', help='Also press START and capture the following frames')
    parser.add_argument('--accept', action='store_true', help='With --start, accept brightness using A and capture again')
    args = parser.parse_args()
    if args.accept and not args.start:
        parser.error('--accept requires --start')
    folder = args.output.resolve() / str(time.time_ns())
    folder.mkdir(parents=True)
    exe = folder / args.exe.name
    shutil.copy2(args.exe.resolve(), exe)
    user = ctypes.WinDLL('user32', use_last_error=True)
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
    user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user.IsWindowVisible.argtypes = [wintypes.HWND]
    user.GetClassNameW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
    user.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
    visible = []
    events = []
    actions = [(23, 0x72, True), (23.2, 0x72, False), (25, 13, True), (25.4, 13, False),
               (28, 0x72, True), (28.2, 0x72, False), (35, 0x72, True), (35.2, 0x72, False)] if args.start else []
    if args.accept:
        actions.extend([(31, 32, True), (31.4, 32, False)])
        actions.sort()
    # Cold pipeline preparation and original-disc I/O can outlast the old
    # startup watchdog on slower CPUs. Retain the Reset and controller checks.
    seconds = 40 if args.start else 45
    environment = os.environ.copy()
    if args.start:
        environment['XBE_CAPTURE_DIR'] = 'captures'
    with (folder / 'native.log').open('wb') as log:
        process = subprocess.Popen([str(exe), '--background', '--timeout', str(seconds), '--disc', str(args.disc.resolve())],
                                   stdout=log, stderr=subprocess.STDOUT, cwd=folder, env=environment,
                                   creationflags=subprocess.CREATE_NO_WINDOW | subprocess.BELOW_NORMAL_PRIORITY_CLASS)
        def observe(hwnd, _):
            owner = wintypes.DWORD()
            user.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
            if owner.value == process.pid and user.IsWindowVisible(hwnd):
                visible.append(int(hwnd))
            if owner.value == process.pid and not user.IsWindowVisible(hwnd) and actions:
                name = ctypes.create_unicode_buffer(256)
                user.GetClassNameW(hwnd, name, len(name))
                if name.value == 'OpenShao' and time.monotonic() - began >= actions[0][0]:
                    _, key, down = actions.pop(0)
                    if not user.PostMessageW(hwnd, 0x100 if down else 0x101, key, 0):
                        visible.append(int(hwnd))  # fail the isolated run on a rejected message
                    events.append({'key': key, 'down': down, 'elapsed': time.monotonic() - began})
            return True
        observer = callback_type(observe)
        began = time.monotonic()
        deadline = began + seconds + 10
        while process.poll() is None:
            user.EnumWindows(observer, 0)
            if visible or time.monotonic() > deadline:
                process.terminate()  # only the child created above
                break
            time.sleep(0.05)
        code = process.wait(timeout=5)
    text = (folder / 'native.log').read_text(encoding='utf-8', errors='replace')
    result = {'code': code, 'pid': process.pid, 'visible_windows': visible, 'events': events, 'log': str(folder / 'native.log')}
    (folder / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result), flush=True)
    assert not visible, 'The diagnostic window became visible'
    assert 'd3d: Reset complete' in text, 'Startup Reset did not complete: ' + text[-5000:]
    assert 'indirect call to 000F1A07 has no recompiled function' not in text, 'The registered thread callback is missing from the dispatch table'
    assert code == 7, f'The startup exited before the controlled watchdog: {code:#x}; see {folder / "native.log"}'
    assert 'openshao input: open port=0' in text, 'The game never opened the connected virtual controller'
    assert 'openshao input: state port=0' in text, 'The game never read its virtual controller'
    if args.start:
        assert 'buttons=0010' in text, 'START did not reach the game controller API'
        assert 'openshao audio: amplitude envelope OFF' in text and 'openshao audio: pitch=' in text
        assert not actions, 'The scheduled test inputs did not all run'
    if args.accept:
        assert 'A=255' in text, 'Accept did not reach the game controller API'
    print('PASS: isolated real startup' + (' and START transition' if args.start else '') +
          (' and brightness acceptance (visual result requires inspection)' if args.accept else ''))


if __name__ == '__main__':
    main()
