"""Opt-in first-map audio probe with isolated data and silent virtual input.

The original game is read-only. Logs and summaries stay in local output.
This checks runtime activity; it does not assess audible quality.
"""
from pathlib import Path
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
import os
import re
import shutil
import struct
import time
import traceback


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--save-fixture', type=Path)
    parser.add_argument('--cache-fixture', type=Path)
    parser.add_argument('--seconds', type=int, default=25)
    parser.add_argument('--require-effects', action='store_true', help='Require active C++ effect processing without failures')
    parser.add_argument('--channels',type=int,choices=(2,6),default=2,help='Silent diagnostic carrier; no physical playback device is opened')
    parser.add_argument('--audio-mode',choices=('mono','stereo','surround'),default='stereo')
    parser.add_argument('--check-menu',action='store_true',help='Exercise Konfigure Recompilation Audio Output before entering the first map')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This probe uses the owned hidden Win32 game window')
    from recomp_error_dialog_test import PrivateProcess
    if not 5 <= args.seconds <= 180:
        parser.error('--seconds must be between 5 and 180')
    root = Path(__file__).resolve().parents[1]
    run = (args.output or root / 'work/audio-runtime').resolve() / str(time.time_ns())
    run.mkdir(parents=True)
    exe = run / 'OpenShao.exe'
    shutil.copy2(args.exe.resolve(), exe)
    game = args.game.resolve()
    disc = game.suffix.lower() in ('.iso', '.xiso')
    if not disc and game.is_file():
        game = game.parent
    source = game if disc else game / 'default.xbe'
    source_signature = (source.stat().st_size, source.stat().st_mtime_ns)
    data = run / 'data'
    data.mkdir()
    (data / 'settings.toml').write_text(
        'language="en"\nvoice_language="en"\nmaster_volume=0\n'
        'vsync=false\nblur=false\ndiscord_enabled=false\n', encoding='utf-8')
    with (data/'settings.toml').open('a',encoding='utf-8') as config:
        config.write('[game]\naudio_output='+str(1 if args.check_menu else ('mono','stereo','surround').index(args.audio_mode))+'\n')
    if args.cache_fixture:
        fixture = args.cache_fixture.resolve()
        shutil.copytree(fixture, data / ('cache' if (fixture / 'd3d12').is_dir() else 'cache/d3d12'))
    saves = run / 'saves'
    saves.mkdir()
    hashes = {}
    if args.save_fixture:
        fixture = args.save_fixture.resolve()
        hashes = {str(f.relative_to(fixture)): hashlib.sha256(f.read_bytes()).hexdigest()
                  for f in fixture.rglob('*') if f.is_file()}
        shutil.copytree(fixture, saves / 'E/UDATA/4d570029')

    user = c.WinDLL('user32', use_last_error=True)
    kernel = c.WinDLL('kernel32', use_last_error=True)
    callback = c.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
    user.CreateDesktopW.argtypes = [w.LPCWSTR, w.LPCWSTR, c.c_void_p, w.DWORD, w.DWORD, c.c_void_p]
    user.CreateDesktopW.restype = w.HANDLE
    user.GetThreadDesktop.argtypes = [w.DWORD]
    user.GetThreadDesktop.restype = w.HANDLE
    user.SetThreadDesktop.argtypes = [w.HANDLE]
    user.CloseDesktop.argtypes = [w.HANDLE]
    user.EnumDesktopWindows.argtypes = [w.HANDLE, callback, w.LPARAM]
    user.GetWindowThreadProcessId.argtypes = [w.HWND, c.POINTER(w.DWORD)]
    user.GetClassNameW.argtypes = [w.HWND, w.LPWSTR, c.c_int]
    user.IsWindowVisible.argtypes = [w.HWND]
    user.PostMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
    kernel.ReadProcessMemory.argtypes = [w.HANDLE, c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t)]
    desktop_name = 'OpenShaoAudio' + str(time.time_ns())
    desktop = user.CreateDesktopW(desktop_name, None, None, 0, 0x01ff, None)
    assert desktop, 'Cannot create isolated test desktop'
    previous = user.GetThreadDesktop(kernel.GetCurrentThreadId())
    assert user.SetThreadDesktop(desktop)
    process = None
    logfile = (run / 'native.log').open('wb')
    began = time.monotonic()
    state = {'base': None, 'window': None, 'visible_windows': []}
    trace = []
    result = {'run': str(run), 'exe_sha256': hashlib.sha256(exe.read_bytes()).hexdigest()}

    def text():
        return (run / 'native.log').read_text(errors='replace')

    def u32(address):
        value = c.c_uint32()
        size = c.c_size_t()
        assert state['base'] and kernel.ReadProcessMemory(
            process.handle, state['base'] + address, c.byref(value), 4, c.byref(size)) and size.value == 4
        return value.value

    def top():
        depth = u32(0x6C9308)
        return u32(0x6C9308 + depth * 4) if depth <= 16 else 0

    def observe(window, _):
        pid = w.DWORD()
        user.GetWindowThreadProcessId(window, c.byref(pid))
        if pid.value == process.pid:
            if user.IsWindowVisible(window):
                state['visible_windows'].append(int(window))
            name = c.create_unicode_buffer(256)
            user.GetClassNameW(window, name, len(name))
            if name.value == 'OpenShao':
                state['window'] = window
        return True

    observer = callback(observe)

    def tick():
        assert process.poll() is None, 'Game exited before the probe finished'
        assert time.monotonic() - began < 300, 'Probe deadline exceeded'
        # EnumDesktopWindows can return zero while the new process has no windows.
        c.set_last_error(0)
        enumerated = user.EnumDesktopWindows(desktop, observer, 0)
        assert enumerated or not c.get_last_error(), c.get_last_error()
        assert not state['visible_windows'], 'Test created a visible window'
        if not state['base']:
            match = re.search(r'memory: guest space at ([0-9a-fA-Fx]+)', text())
            if match:
                candidate=int(match[1],16); probe=c.c_uint32(); count=c.c_size_t()
                if kernel.ReadProcessMemory(process.handle,candidate+0x6C9308,c.byref(probe),4,c.byref(count)) and count.value==4:
                    state['base']=candidate
        if state['base']:
            sample = dict(top=top(), room=u32(0x505970), world=u32(0x639224),
                          scene=u32(0x6C41B0), cinematic=u32(0x6C4198) & 255)
            if not trace or any(trace[-1].get(key) != value for key, value in sample.items()):
                sample['seconds'] = round(time.monotonic() - began, 3)
                trace.append(sample)
                print('STATE', json.dumps(sample), flush=True)
        time.sleep(.02)

    def wait(seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            tick()

    def pulse(button, gap=1.2):
        assert state['window']
        assert user.PostMessageW(state['window'], 0x8051, button, 1)
        wait(.3)
        assert user.PostMessageW(state['window'], 0x8051, button, 0)
        wait(gap)

    try:
        diagnostic_env = {'OPENSHAO_DISABLE_DISCORD': '1', 'OPENSHAO_AUDIO_DIAGNOSTICS': '1',
                          'OPENSHAO_AUDIO_AUDIT': '1', 'OPENSHAO_AUDIO_TEST_CHANNELS':str(args.channels),
                          'XBE_CAPTURE_DIR':str(run/'captures'),'OPENSHAO_CAPTURE_NATIVE_PIXELS':'1'}
        previous_env = {key: os.environ.get(key) for key in diagnostic_env}
        os.environ.update(diagnostic_env)
        try:
            process = PrivateProcess(
                [str(exe), '--background', '--timeout', '300', '--virtual-gamepads', '1',
                 '--disc' if disc else '--game', str(game), '--data-dir', str(data), '--save-dir', str(saves)],
                desktop_name, logfile, run)
        finally:
            for key, value in previous_env.items():
                if value is None:
                    os.environ.pop(key, None)
                else:
                    os.environ[key] = value
        print('RUN', run, flush=True)
        until = time.monotonic() + 100
        next_boot = began + 24
        boot_key = 13
        while time.monotonic() < until:
            tick()
            if state['window'] and state['base']:
                if top() == 0x598430:
                    break
                if time.monotonic() >= next_boot:
                    assert user.PostMessageW(state['window'], 0x100, boot_key, 0)
                    wait(.2)
                    assert user.PostMessageW(state['window'], 0x101, boot_key, 0)
                    boot_key = 32 if boot_key == 13 else 13
                    next_boot = time.monotonic() + 6
        assert top() == 0x598430, 'Main menu was not reached'
        assert u32(0x598468) == 0, 'New game must be the initial selection'
        if args.check_menu:
            def navigate(row):
                for _ in range(12):
                    if u32(0x598468)==row: return
                    pulse(12,gap=.6)
                raise AssertionError('Main-menu navigation failed')
            navigate(6)
            pulse(0,gap=4); assert top()==0x5985E8
            for _ in range(3): pulse(12,gap=.8)
            pulse(0,gap=4)
            # Capture before navigation so a missing row on initial entry is visible.
            assert user.PostMessageW(state['window'],0x100,0x72,0)
            wait(.1); assert user.PostMessageW(state['window'],0x101,0x72,0)
            wait(2)
            pulse(12,gap=.8)
            mode=1
            for _ in range(3):
                pulse(14,gap=.8); mode=(mode+1)%3
                assert u32(0x64CFCC)==1,'Editing Audio Output must not apply it before Save'
            desired=('mono','stereo','surround').index(args.audio_mode)
            for _ in range((desired-mode)%3): pulse(14,gap=.8)
            pulse(1,gap=3)
            pulse(12,gap=.8); pulse(12,gap=.8); pulse(0,gap=4)
            assert top()==0x598430
            assert u32(0x64CFCC)==(3 if desired==2 else desired),'Saving Audio Output must update the movie route'
            import tomllib
            saved=tomllib.loads((data/'settings.toml').read_text(encoding='utf-8'))
            assert saved['game']['audio_output']==desired,'Konfigure Recompilation must persist Audio Output'
            navigate(0)
            result['audio_menu_checked']=True
        pulse(0, gap=4)
        # Confirm character, difficulty and any new-profile prompt through the real UI.
        for _ in range(7):
            if top() == 0 and u32(0x639224):
                break
            pulse(0, gap=4)
        until = time.monotonic() + 70
        while time.monotonic() < until:
            tick()
            if top() == 0 and u32(0x505970) == 96 and u32(0x639224):
                break
            pulse(6, gap=3)
        assert top() == 0 and u32(0x505970) == 96 and u32(0x639224), 'First map was not reached'
        pulse(6, gap=12)
        pulse(6, gap=5)
        # The room number is assigned before streaming and pipeline preparation finish.
        # Require real gameplay logic to advance before measuring the audio window.
        initial_logic = u32(0x5AFC60)
        until = time.monotonic() + 120
        next_scene_skip = time.monotonic() + 5
        while time.monotonic() < until:
            tick()
            cinematic = u32(0x6C4198) & 255
            if top() == 0 and u32(0x505970) == 96 and not cinematic and ((u32(0x5AFC60) - initial_logic) & 0xFFFFFFFF) >= 60:
                break
            if cinematic and time.monotonic() >= next_scene_skip:
                pulse(6, gap=2)
                next_scene_skip = time.monotonic() + 5
        assert ((u32(0x5AFC60) - initial_logic) & 0xFFFFFFFF) >= 60, 'First-map logic did not advance'
        assert not (u32(0x6C4198) & 255), 'Introductory cutscene did not finish'
        start_log = len(text())
        sample_start = time.monotonic()
        sample_logic = u32(0x5AFC60)
        # Exercise attacks and movement using only the diagnostic virtual gamepad.
        for button in (2, 3, 2, 0, 2, 3):
            pulse(button, gap=.9)
        assert user.PostMessageW(state['window'], 0x8052, 0, 16000)
        wait(1.2)
        assert user.PostMessageW(state['window'], 0x8052, 0, 0)
        wait(max(0, args.seconds - (time.monotonic() - sample_start)))
        native = text()
        logic_frames = (u32(0x5AFC60) - sample_logic) & 0xFFFFFFFF
        assert logic_frames >= 60, 'Gameplay logic stalled during audio measurement'
        result.update(room=u32(0x505970), world=u32(0x639224), top=top(), logic_frames=logic_frames,
                      sample_seconds=round(time.monotonic() - sample_start, 3),
                      effects_images=re.findall(r'(?:dsound: effects image|audio effects: loaded)[^\r\n]*', native),
                      audio_diagnostics=[line for line in native.splitlines()
                                         if re.search(r'audio (?:effects|mixer|audit|output):|dsound: effect', line)],
                      gameplay_audio_lines=[line for line in native[start_log:].splitlines()
                                            if re.search(r'audio|dsound', line, re.IGNORECASE)])
        assert user.PostMessageW(state['window'], 0x10, 0, 0)
        result['exit_code'] = process.wait(timeout=12)
        assert result['exit_code'] == 0
        assert f'silent diagnostic sink, {args.channels} channels' in native,'The probe must bypass physical output'
        effective='Stereo' if args.audio_mode=='surround' and args.channels==2 else args.audio_mode.title()
        output_lines=re.findall(r'audio output: mode=(\w+) channels=(\d+) peaks FL=([\d.]+) FR=([\d.]+) C=([\d.]+) LFE=([\d.]+) BL=([\d.]+) BR=([\d.]+)',native[start_log:])
        assert output_lines and all(m[0]==effective and int(m[1])==args.channels for m in output_lines),'The rendered layout must follow the menu and device fallback'
        if effective=='Mono': assert all(m[2]==m[3] for m in output_lines),'Mono must have identical left and right output'
        if effective!='Surround': assert all(float(p)==0 for m in output_lines for p in m[4:]),'Mono and Stereo must silence unused carrier channels'
        if effective=='Surround': assert any(float(m[6])>0 or float(m[7])>0 for m in output_lines),'Gameplay surround must contain rear-channel activity'
        result['output_metrics']=output_lines
        assert result['effects_images'], 'No effect image was loaded'
        if args.require_effects:
            assert 'C++ filter chain active' in native, 'The C++ effect chain was not active'
            assert 'original GP firmware active' not in native, 'The obsolete DSP execution path was used'
            assert not re.search(r'audio effects:.*(?:cannot load|processing stopped)', native, re.IGNORECASE), 'The effect chain reported a failure'
            metric_pattern = (r'audio effects: blocks=(\d+) failures=(\d+) active=([01]) '
                              r'bins=([0-9a-fA-F]+) input_peak=([0-9.eE+-]+) output_peak=([0-9.eE+-]+)')
            metrics = [dict(blocks=int(m[0]), failures=int(m[1]), active=int(m[2]),
                            bins=int(m[3], 16), input_peak=float(m[4]), output_peak=float(m[5]))
                       for m in re.findall(metric_pattern, native)]
            gameplay_metrics = re.findall(metric_pattern, native[start_log:])
            assert metrics and any(m['active'] and m['blocks'] for m in metrics), 'No active effect blocks were measured'
            assert all(m['failures'] == 0 for m in metrics), 'The effect chain reported failures'
            assert gameplay_metrics and all(int(m[2]) == 1 for m in gameplay_metrics), 'The effect chain stopped during gameplay'
            assert any(float(m[4]) > 0 and float(m[5]) > 0 for m in gameplay_metrics), 'The effect chain produced no gameplay audio'
            result['effect_metrics'] = metrics
            result['effect_summary'] = dict(gameplay_samples=len(gameplay_metrics), blocks=metrics[-1]['blocks'],
                                            failures=max(m['failures'] for m in metrics),
                                            input_peak=max(float(m[4]) for m in gameplay_metrics),
                                            output_peak=max(float(m[5]) for m in gameplay_metrics))
        result['passed'] = True
    except Exception as error:
        result.update(passed=False, failure=repr(error), traceback=traceback.format_exc())
    finally:
        if process and process.poll() is None:
            process.terminate()
            process.wait(timeout=8)
        if process:
            process.close()
        logfile.close()
        user.SetThreadDesktop(previous)
        user.CloseDesktop(desktop)
        result['source_unchanged'] = source_signature == (source.stat().st_size, source.stat().st_mtime_ns)
        if args.save_fixture:
            result['source_saves_unchanged'] = hashes == {
                str(f.relative_to(fixture)): hashlib.sha256(f.read_bytes()).hexdigest()
                for f in fixture.rglob('*') if f.is_file()}
        result['trace'] = trace
        result['visible_windows'] = state['visible_windows']
        (run / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
        print('RESULT', json.dumps({key: value for key, value in result.items()
                                    if key not in ('trace', 'audio_diagnostics', 'gameplay_audio_lines', 'effect_metrics')}), flush=True)
    assert result['passed'] and result['source_unchanged'], result.get('failure')
    assert result.get('source_saves_unchanged', True)


if __name__ == '__main__':
    main()
