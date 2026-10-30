"""Drive the launcher through its automation channel and check the files it changes.

The executable runs with --launcher-test, which keeps the real launcher logic and drawing but opens no window.
On Windows the processes run on a private desktop, so error dialogs never reach the user's desktop.
Other systems run without display access or external desktop commands.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import time
import tomllib

WINDOWS = sys.platform == 'win32'
if WINDOWS:
    import ctypes
    from ctypes import wintypes
    from recomp_error_dialog_test import PrivateProcess
from recomp_game_fixture import game_files, write_game_folder

XBE_SHA256 = '48240902bf4a04eb89376217c529531032d3370749bcce91a194878cc49db74a'


class Desktop:
    """A private Windows desktop, or nothing on other systems."""

    def __init__(self):
        self.name = None
        if not WINDOWS:
            return
        self.user = ctypes.WinDLL('user32', use_last_error=True)
        self.kernel = ctypes.WinDLL('kernel32')
        self.callback = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
        user = self.user
        user.CreateDesktopW.argtypes = [wintypes.LPCWSTR, wintypes.LPCWSTR, ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p]
        user.CreateDesktopW.restype = wintypes.HANDLE
        user.CloseDesktop.argtypes = [wintypes.HANDLE]
        user.GetThreadDesktop.argtypes = [wintypes.DWORD]
        user.GetThreadDesktop.restype = wintypes.HANDLE
        user.SetThreadDesktop.argtypes = [wintypes.HANDLE]
        user.EnumDesktopWindows.argtypes = [wintypes.HANDLE, self.callback, wintypes.LPARAM]
        user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
        user.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
        user.GetClassNameW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
        user.IsWindowVisible.argtypes = [wintypes.HWND]
        user.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
        self.name = 'OpenShaoLauncher' + str(time.time_ns())
        self.handle = user.CreateDesktopW(self.name, None, None, 0, 0x01ff, None)
        assert self.handle
        self.previous = user.GetThreadDesktop(self.kernel.GetCurrentThreadId())
        assert user.SetThreadDesktop(self.handle)

    def windows(self, process):
        """Titles and texts of the visible windows a process opened on the private desktop."""
        if not WINDOWS:
            return []
        self.user.EnumChildWindows.argtypes = [wintypes.HWND, self.callback, wintypes.LPARAM]
        found = []

        def text(window):
            buffer = ctypes.create_unicode_buffer(1024)
            self.user.GetWindowTextW(window, buffer, len(buffer))
            return buffer.value

        def observe(window, _):
            pid = wintypes.DWORD()
            self.user.GetWindowThreadProcessId(window, ctypes.byref(pid))
            if pid.value == process.pid and self.user.IsWindowVisible(window):
                kind = ctypes.create_unicode_buffer(64)
                self.user.GetClassNameW(window, kind, len(kind))
                parts = []
                children = self.callback(lambda child, _: parts.append(text(child)) or True)
                self.user.EnumChildWindows(window, children, 0)
                found.append(f'{kind.value} "{text(window)}": ' + ' | '.join(p for p in parts if p))
            return True

        observer = self.callback(observe)
        self.user.EnumDesktopWindows(self.handle, observer, 0)
        return found

    def close(self):
        if WINDOWS:
            self.user.SetThreadDesktop(self.previous)
            self.user.CloseDesktop(self.handle)

    def close_message(self, process, title, timeout=15):
        """Closes a message box with the given title. Other systems report errors without a dialog."""
        if not WINDOWS:
            return
        found = []

        def observe(window, _):
            pid = wintypes.DWORD()
            self.user.GetWindowThreadProcessId(window, ctypes.byref(pid))
            text = ctypes.create_unicode_buffer(256)
            self.user.GetWindowTextW(window, text, len(text))
            kind = ctypes.create_unicode_buffer(64)
            self.user.GetClassNameW(window, kind, len(kind))
            if pid.value == process.pid and text.value == title and kind.value == '#32770' and self.user.IsWindowVisible(window):
                found.append(window)
            return True

        observer = self.callback(observe)
        deadline = time.monotonic() + timeout
        while process.poll() is None and not found and time.monotonic() < deadline:
            self.user.EnumDesktopWindows(self.handle, observer, 0)
            time.sleep(.05)
        assert found, 'The expected message did not open: ' + title
        time.sleep(.3)
        self.user.PostMessageW(found[0], 0x0010, 0, 0)


class Launcher:
    # Every started launcher, so a failed check still ends them. A process left on the private desktop
    # keeps the test's output handles open and CTest would wait for it indefinitely.
    started = []

    def __init__(self, exe, arguments, root, name, desktop):
        Launcher.started.append(self)
        self.closed = False
        self.folder = root / (name + '-automation')
        self.folder.mkdir()
        (self.folder / 'commands.txt').write_text('')
        self.output = root / (name + '.log')
        self.log = self.output.open('wb')
        command = [str(exe), '--launcher-test', str(self.folder), *map(str, arguments)]
        if WINDOWS:
            self.process = PrivateProcess(command, desktop.name, self.log, root)
        else:
            environment = os.environ.copy()
            for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'DBUS_SESSION_BUS_ADDRESS'):
                environment.pop(key, None)
            environment.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', PATH='')
            self.process = subprocess.Popen(command, stdout=self.log, stderr=subprocess.STDOUT, cwd=root, env=environment)

    def send(self, *commands):
        with (self.folder / 'commands.txt').open('a', encoding='utf-8', newline='\n') as stream:
            for command in commands:
                stream.write(command + '\n')

    def state(self):
        try:
            return json.loads((self.folder / 'state.json').read_text(encoding='utf-8'))
        except (FileNotFoundError, PermissionError, json.JSONDecodeError):
            return None

    def wait(self, predicate, message, timeout=30):
        deadline = time.monotonic() + timeout
        state = None
        while time.monotonic() < deadline:
            state = self.state()
            if state is not None and predicate(state):
                return state
            assert self.process.poll() is None, (message, 'the launcher exited', self.process.poll(), state)
            time.sleep(.03)
        raise AssertionError((message, state))

    def exit_code(self, timeout=15):
        try:
            return self.process.wait(timeout=timeout)
        finally:
            self.close()

    def close(self):
        if self.closed:
            return
        self.closed = True
        try:
            if self.process.poll() is None:
                self.process.terminate()
                self.process.wait(timeout=5)
        finally:
            if WINDOWS:
                self.process.close()
            self.log.close()

    def report(self, desktop):
        """Prints the end of this launcher's output after a failed check."""
        if self.process.poll() is None:
            print('Launcher still running. Its windows: ' + repr(desktop.windows(self.process)), file=sys.stderr)
        else:
            print('Launcher exit code: ' + str(self.process.poll()), file=sys.stderr)
        for dump in self.output.parent.glob('**/crashes/*'):
            print('Crash report: ' + str(dump), file=sys.stderr)
        try:
            text = self.output.read_text(encoding='utf-8', errors='replace')
        except OSError:
            return
        print('--- ' + self.output.name + ' ---', file=sys.stderr)
        print(text[-4000:], file=sys.stderr)
        for log in sorted(self.output.parent.glob('**/logs/*.log'))[-2:]:
            print('--- ' + str(log) + ' ---', file=sys.stderr)
            print(log.read_text(encoding='utf-8', errors='replace')[-4000:], file=sys.stderr)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--game', type=Path)
    args = parser.parse_args()
    exe = args.exe.resolve()
    desktop = Desktop()
    with tempfile.TemporaryDirectory(prefix='openshao-launcher-', ignore_cleanup_errors=True) as directory:
        try:
            root = Path(directory)
            data = root / 'data'
            data.mkdir()
            settings = data / 'settings.toml'
            settings.write_text('language="fr"\nfps_cap=30\nblur=false\naspect_ratio="16:10"\ndisplay_mode="windowed"\nwidth=800\nheight=600\n'
                                'custom_key="preserve"\n[controller]\ndeadzone=0.23\n[controller.bindings]\na="lefttrigger"\n'
                                '[mouse]\nsensitivity=145\ninvert_x=true\nmotion="left_stick"\n[mouse.bindings]\nside1="left_trigger"\n'
                                'wheel_up="dpad_up"\n[keyboard.bindings]\na=[90,86]\n')
            before = settings.read_bytes()

            # Opening and closing the launcher keeps the settings file byte for byte.
            launcher = Launcher(exe, ['--configure', '--data-dir', data], root, 'close', desktop)
            launcher.wait(lambda s: s['page'] == 'play' and s['popup'] is None, 'The launcher must open on Play')
            for page in ('mods', 'files', 'about', 'play'):
                launcher.send('page ' + page)
                launcher.wait(lambda s, page=page: s['page'] == page, 'The launcher must show ' + page)
            launcher.send('close')
            assert launcher.exit_code() == 0
            assert settings.read_bytes() == before, 'Closing the launcher must preserve settings'

            # A second start with the same data folder ends quietly and leaves the open launcher working.
            first = Launcher(exe, ['--configure', '--data-dir', data], root, 'first-copy', desktop)
            first.wait(lambda s: s['page'] == 'play', 'The first copy must open')
            second = Launcher(exe, ['--configure', '--data-dir', data], root, 'second-copy', desktop)
            assert second.exit_code() == 9, 'A second copy must report that OpenShao is already open'
            assert not (second.folder / 'state.json').exists(), 'A second copy must not open a launcher'
            assert 'already open' in second.output.read_text(encoding='utf-8', errors='replace')
            if WINDOWS:
                # Without a window to bring forward, a normal start says that OpenShao is open.
                with (root / 'third-copy.log').open('wb') as output:
                    third = PrivateProcess([str(exe), '--configure', '--data-dir', str(data)], desktop.name, output, root)
                    try:
                        desktop.close_message(third, 'OpenShao')
                        assert third.wait(15) == 9, 'Closing the notice must end the second copy'
                    finally:
                        if third.poll() is None:
                            third.terminate()
                        third.close()
            first.send('page mods')
            first.wait(lambda s: s['page'] == 'mods', 'The first copy must keep working')
            first.send('close')
            assert first.exit_code() == 0
            assert settings.read_bytes() == before, 'A second copy must not change settings'

            # Restoring video changes only the display settings and keeps every other preference.
            launcher = Launcher(exe, ['--configure', '--data-dir', data], root, 'restore', desktop)
            launcher.wait(lambda s: s['page'] == 'play', 'The launcher must open')
            launcher.send('page files', 'restore-video')
            launcher.wait(lambda s: s['popup'] and s['popup']['title'] == 'Video settings restored', 'Restore must confirm the change')
            launcher.send('dismiss', 'check-updates off')
            launcher.wait(lambda s: s['popup'] is None and not s['check_updates'], 'Turning off update checks must be visible')
            launcher.send('close')
            assert launcher.exit_code() == 0
            config = tomllib.loads(settings.read_text())
            assert config['display_mode'] == 'borderless' and config['width'] == 0 and config['height'] == 0
            assert config['aspect_ratio'] == 'original' and config['render_percent'] == 100 and config['vsync'] is True
            assert config['language'] == 'fr'
            assert config['fps_cap'] == 60, 'Saving must replace the old frame-rate preference with 60 FPS'
            assert config['bloom_effect'] is False and 'bloom' not in config and 'blur' not in config, 'Bloom must persist and migrate legacy keys'
            assert config['custom_key'] == 'preserve'
            assert abs(config['controller']['deadzone'] - .23) < .0001
            assert config['controller']['bindings']['a'] == 'lefttrigger'
            assert config['mouse']['sensitivity'] == 145 and config['mouse']['bindings']['side1'] == 'left_trigger'
            assert config['mouse']['bindings']['wheel_up'] == 'dpad_up'
            assert config['mouse']['invert_x'] and config['mouse']['motion'] == 'left_stick'
            assert config['keyboard']['bindings']['a'] == [ord('Z'), ord('V')]
            assert config['launcher']['check_updates'] is False, 'The update preference must be saved'
            saved = settings.read_bytes()

            # A local package installs directly from its file and asks before removal.
            if args.game:
                game = root / 'original-game'
                write_game_folder(game, game_files(args.game.read_bytes()))
                payload = b'Local mod installation regression'
                manifest = dict(schema=2, id='LocalMod', name='Local mod', summary='Synthetic package', authors=['Tester'],
                                type='data', install_dir='packages/LocalMod', version='1.0.0', recomp='0.2.0',
                                game={'xbe_sha256': hashlib.sha256(args.game.read_bytes()).hexdigest()}, patches=[],
                                files=[dict(path='files/local.bin', size=len(payload), sha256=hashlib.sha256(payload).hexdigest())])
                archive = root / 'local.tar.xz'
                with tarfile.open(archive, 'w:xz', format=tarfile.USTAR_FORMAT) as tar:
                    for name, content in {'mod.json': json.dumps(manifest).encode(), 'files/local.bin': payload}.items():
                        entry = tarfile.TarInfo(name)
                        entry.size = len(content)
                        tar.addfile(entry, io.BytesIO(content))
                local_data = root / 'local-install'
                launcher = Launcher(exe, ['--configure', '--data-dir', local_data, '--disc', game], root, 'local-install', desktop)
                launcher.wait(lambda s: not s['mods_busy'], 'The catalog check must finish')
                launcher.send('page mods', 'mod-install-file ' + str(archive))
                launcher.wait(lambda s: any(m['id'] == 'LocalMod' and m['installed'] and m['enabled'] for m in s['mods']) and not s['mods_busy'],
                              'Choosing a local mod must install and enable it', timeout=60)
                rows = tomllib.loads((local_data / 'mods/installed.toml').read_text())['mods']
                assert len(rows) == 1 and rows[0]['id'] == 'LocalMod' and rows[0]['enabled']
                assert (local_data / 'mods/packages/LocalMod/files/local.bin').read_bytes() == payload
                launcher.send('mod-remove LocalMod')
                launcher.wait(lambda s: s['popup'] and s['popup']['title'] == 'Remove Local mod?', 'Removing must ask first')
                launcher.send('dismiss')
                launcher.wait(lambda s: s['popup'] is None, 'Cancel must keep the mod')
                assert (local_data / 'mods/packages/LocalMod/files/local.bin').exists()
                launcher.send('mod-remove LocalMod', 'confirm')
                launcher.wait(lambda s: not any(m['id'] == 'LocalMod' and m['installed'] for m in s['mods']) and not s['mods_busy'],
                              'Confirming must remove the mod')
                assert not (local_data / 'mods/packages/LocalMod/files/local.bin').exists()
                launcher.send('close')
                assert launcher.exit_code() == 0

            # Installed language packs are enabled and disabled in Mods without changing saved languages.
            catalog_data = root / 'new-language'
            (catalog_data / 'cache').mkdir(parents=True)
            catalog_mod = dict(id='LanguagePack_JA', name='Japanese translation', summary='Synthetic catalog entry',
                               authors=['Tester'], type='language', language='ja', language_name='Japanese',
                               install_dir='packages/LanguagePack_JA', versions=[dict(version='1.0.0', recomp='0.1.0', size=1,
                               sha256='0' * 64, url='https://github.com/OAleex/OpenShao-Mods/releases/download/test/test.tar.xz')])
            (catalog_data / 'cache/mods.json').write_text(json.dumps(dict(schema=1, game={'xbe_sha256': XBE_SHA256}, mods=[catalog_mod])))
            local_pack = catalog_data / 'mods/packages/LanguagePack_JA'
            local_pack.mkdir(parents=True)
            (local_pack / 'README.txt').write_bytes(b'test')
            local_metadata = dict(catalog_mod, schema=2, version='1.0.0', recomp='0.2.0', patches=[], game={'xbe_sha256': XBE_SHA256},
                                  files=[dict(path='README.txt', size=4, sha256=hashlib.sha256(b'test').hexdigest())])
            catalog_registry = catalog_data / 'mods/installed.toml'
            catalog_registry.write_text('[[mods]]\nid = "LanguagePack_JA"\nversion = "1.0.0"\nenabled = false\nmanifest = '
                                        + json.dumps(json.dumps(local_metadata)) + '\n')
            catalog_settings = catalog_data / 'settings.toml'
            catalog_settings.write_text('language = "fr"\nvoice_language = "en"\n')
            launcher = Launcher(exe, ['--configure', '--data-dir', catalog_data], root, 'catalog-language', desktop)
            launcher.wait(lambda s: not s['mods_busy'] and any(m['id'] == 'LanguagePack_JA' and m['installed'] for m in s['mods']),
                          'An installed language mod must appear in Mods')
            launcher.send('page mods', 'mod-toggle LanguagePack_JA')
            launcher.wait(lambda s: any(m['id'] == 'LanguagePack_JA' and m['enabled'] for m in s['mods']) and not s['mods_busy'],
                          'The switch must enable the installed pack')
            assert tomllib.loads(catalog_registry.read_text())['mods'][0]['enabled']
            assert (local_pack / 'README.txt').read_bytes() == b'test', 'Enabling must preserve package files'
            launcher.send('mod-toggle LanguagePack_JA')
            launcher.wait(lambda s: any(m['id'] == 'LanguagePack_JA' and not m['enabled'] for m in s['mods']) and not s['mods_busy'],
                          'The switch must disable the pack')
            assert not tomllib.loads(catalog_registry.read_text())['mods'][0]['enabled']
            assert catalog_settings.read_text() == 'language = "fr"\nvoice_language = "en"\n', 'Mod switches must not change saved languages'
            launcher.send('close')
            assert launcher.exit_code() == 0

            # Play with a missing game reaches the game's error report.
            launcher = Launcher(exe, ['--data-dir', data, '--disc', root / 'missing.iso'], root, 'temporary-session', desktop)
            launcher.wait(lambda s: s['page'] == 'play', 'The launcher must open with a temporary game')
            launcher.send('play')
            desktop.close_message(launcher.process, 'OpenShao')
            assert launcher.exit_code() == 3
            assert settings.read_bytes() == saved, 'Play must preserve the settings file'
            if WINDOWS:
                closed = 'Error dialog closed.'
                logs = [launcher.output, *(data / 'logs').glob('*.log')]
                assert any(closed in log.read_text(encoding='utf-8', errors='replace') for log in logs), 'The error report must log its dismissal'

            # A saved language without its pack keeps the player in the launcher and opens Mods.
            for kind, text, voice in (('text', 'pt-BR', 'en'), ('voice', 'fr', 'pt-BR')):
                missing_data = root / ('missing-' + kind)
                missing_data.mkdir()
                languages = f'language = "{text}"\nvoice_language = "{voice}"\n'
                (missing_data / 'settings.toml').write_text(languages)
                launcher = Launcher(exe, ['--data-dir', missing_data, '--disc', root / 'missing.iso'], root, 'missing-' + kind, desktop)
                launcher.wait(lambda s: s['page'] == 'play' and not s['mods_busy'], 'The launcher must open')
                launcher.send('play')
                state = launcher.wait(lambda s: s['popup'] and s['popup']['title'] == 'Language pack required',
                                      'A missing ' + kind + ' pack must be reported')
                assert state['page'] == 'mods', 'A missing ' + kind + ' pack must direct the player to Mods'
                assert state['result'] == -1, 'Play must remain in the launcher without an enabled language pack'
                launcher.send('dismiss')
                launcher.wait(lambda s: s['popup'] is None, 'The message must close')
                assert (missing_data / 'settings.toml').read_text() == languages, 'Missing packs must not reset saved languages'
                launcher.send('close')
                assert launcher.exit_code() == 0

            # A development language pack does not bring back a language selector or change settings.
            pack = root / 'LanguagePack_PT-BR'
            pack.mkdir()
            launcher = Launcher(exe, ['--configure', '--data-dir', data, '--language-pack', pack], root, 'language-pack', desktop)
            launcher.wait(lambda s: s['page'] == 'play', 'The launcher must open with a development pack')
            launcher.send('close')
            assert launcher.exit_code() == 0
            config = tomllib.loads(settings.read_text())
            assert config['language'] == 'fr' and config['custom_key'] == 'preserve'
        except BaseException:
            if Launcher.started:
                Launcher.started[-1].report(desktop)
            raise
        finally:
            for launcher in Launcher.started:
                try:
                    launcher.close()
                except Exception as error:
                    print('Cannot end a launcher:', error, file=sys.stderr)
            desktop.close()
    print('PASS: launcher pages, settings preservation, video recovery, update preference, local mods, language packs and play errors')


if __name__ == '__main__':
    main()
