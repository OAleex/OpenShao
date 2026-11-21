"""Capture original menu frames from an owned hidden window (manual real-disc regression)."""
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
    parser.add_argument('--frames', type=int, default=24, choices=range(1, 25))
    parser.add_argument('--scale', type=int, default=210, choices=range(100, 301))
    parser.add_argument('--language', choices=('en', 'fr', 'de', 'es', 'it'), default='en')
    parser.add_argument('--options-page', choices=('main', 'brightness', 'audio', 'controls'))
    parser.add_argument('--original-options', action='store_true')
    args = parser.parse_args()
    folder = args.output.resolve() / str(time.time_ns())
    folder.mkdir(parents=True)
    exe = folder / 'OpenShao.exe'
    shutil.copy2(args.exe.resolve(), exe)
    (folder / 'runtime-data').mkdir()
    config = 'language = "' + args.language + '"\n'
    if args.original_options:
        config += '[game]\noverride_original = true\nmusic_volume = 20\neffects_volume = 40\nspeech_volume = 60\nbrightness = 25\nversus_rounds = 1\nversus_time = 180\ncamera_reverse = true\n'
    (folder / 'runtime-data/settings.toml').write_text(config)
    disc = args.disc.resolve()
    media_stat = disc.stat()
    environment = os.environ.copy()
    environment.update(XBE_CAPTURE_DIR='captures', OPENSHAO_CAPTURE_HUD='1',
                       OPENSHAO_CAPTURE_NATIVE_PIXELS='1',
                       OPENSHAO_CAPTURE_RENDER_PERCENT=str(args.scale))
    environment.pop('OPENSHAO_CAPTURE_MENU_LAYERS', None)
    user = ctypes.WinDLL('user32', use_last_error=True)
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
    user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user.GetClassNameW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
    user.IsWindowVisible.argtypes = [wintypes.HWND]
    user.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
    seconds = min(180, 48 + args.frames * 3)
    events, releases, visible = [], [], []
    hwnd = None
    log_path = folder / 'native.log'
    with log_path.open('wb') as log:
        process = subprocess.Popen([str(exe), '--background', '--timeout', str(seconds), '--disc', str(disc)],
                                   cwd=folder, env=environment, stdout=log, stderr=subprocess.STDOUT,
                                   creationflags=subprocess.CREATE_NO_WINDOW | subprocess.BELOW_NORMAL_PRIORITY_CLASS)

        def observe(window, _):
            nonlocal hwnd
            owner = wintypes.DWORD()
            user.GetWindowThreadProcessId(window, ctypes.byref(owner))
            if owner.value == process.pid:
                if user.IsWindowVisible(window):
                    visible.append(int(window))
                name = ctypes.create_unicode_buffer(256)
                user.GetClassNameW(window, name, len(name))
                if name.value == 'OpenShao':
                    hwnd = window
            return True

        observer = callback_type(observe)
        began = time.monotonic()
        menu_seen = None
        next_capture = 0
        sent = 0
        started = accepted = closed = False
        navigation = ([40] * 4 + [None] * 2 + [32]) if args.options_page else []
        if args.options_page and args.options_page != 'main':
            navigation += [None] * 3 + [40] * {'brightness': 0, 'audio': 1, 'controls': 2}[args.options_page] + [None] * 2 + [32]
        navigation_sent = 0

        def key(code, elapsed):
            assert hwnd and user.PostMessageW(hwnd, 0x100, code, 0)
            releases.append((elapsed + .2, code))
            events.append({'elapsed': elapsed, 'key': code})

        try:
            while process.poll() is None:
                user.EnumWindows(observer, 0)
                assert not visible, 'The diagnostic created a visible window'
                elapsed = time.monotonic() - began
                text = log_path.read_text(encoding='utf-8', errors='replace')
                if elapsed >= 24 and hwnd and not started:
                    key(13, elapsed)
                    started = True
                if elapsed >= 31 and hwnd and not accepted:
                    key(32, elapsed)
                    accepted = True
                if 'retail menu top=00598430' in text and menu_seen is None:
                    menu_seen = elapsed
                if menu_seen is not None and navigation_sent < len(navigation) and elapsed >= menu_seen + .5 + navigation_sent * .5:
                    if navigation[navigation_sent] is not None:
                        key(navigation[navigation_sent], elapsed)
                    navigation_sent += 1
                captures = sorted((folder / 'captures').glob('*/final_native.bmp'))
                if (menu_seen is not None and elapsed >= menu_seen + 3 + len(navigation) * .5 and elapsed >= next_capture
                        and sent < args.frames and len(captures) == sent):
                    key(0x72, elapsed)
                    sent += 1
                    next_capture = elapsed + 1.7
                for due, code in list(releases):
                    if elapsed >= due:
                        assert user.PostMessageW(hwnd, 0x101, code, 0)
                        releases.remove((due, code))
                if len(captures) == args.frames and not releases and elapsed >= next_capture and not closed:
                    assert user.PostMessageW(hwnd, 0x10, 0, 0)  # WM_CLOSE to this process's own hidden window
                    closed = True
                if elapsed > seconds + 8:
                    raise AssertionError('The owned diagnostic exceeded its watchdog deadline')
                time.sleep(.08)
        finally:
            if process.poll() is None:
                process.terminate()
            code = process.wait(timeout=8)
    captures = sorted((folder / 'captures').glob('*/final_native.bmp'))
    result = {'code': code, 'pid': process.pid, 'frames': [str(p.relative_to(folder)) for p in captures],
              'events': events, 'scale_percent': args.scale, 'menu_seen': menu_seen}
    (folder / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    assert code == 0 and closed, f'Unexpected exit before a completed capture sequence: {code}; see {log_path}'
    assert len(captures) == args.frames, f'Expected {args.frames} completed captures, got {len(captures)}'
    if args.original_options:
        assert 'Original options applied: music=20 effects=40 speech=60 brightness=25 rounds=1 time=180 camera_reverse=1' in log_path.read_text(encoding='utf-8', errors='replace')
    assert (disc.stat().st_size, disc.stat().st_mtime_ns) == (media_stat.st_size, media_stat.st_mtime_ns)
    print(f'PASS: {len(captures)} isolated original-menu captures: {folder}', flush=True)


if __name__ == '__main__':
    main()
