"""Check the first-run launcher language choice through the automation channel."""
import argparse
import os
from pathlib import Path
import sys
import tempfile
import tomllib

from recomp_launcher_test import Desktop, Launcher


def settings(data):
    path = data / 'settings.toml'
    return tomllib.loads(path.read_text(encoding='utf-8')) if path.exists() else {}


def start(exe, data, root, name, desktop, locale):
    os.environ['OPENSHAO_LAUNCHER_LOCALE'] = locale
    return Launcher(exe, ['--configure', '--data-dir', data], root, name, desktop)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', required=True, type=Path)
    args = parser.parse_args()
    exe = args.exe.resolve()
    os.environ['OPENSHAO_LAUNCHER_LANGUAGE_PROMPT'] = '1'
    desktop = Desktop()
    with tempfile.TemporaryDirectory(prefix='openshao-launcher-language-', ignore_cleanup_errors=True) as directory:
        try:
            root = Path(directory)
            data = root / 'data'
            data.mkdir()

            # The first start opens the language list with the system language selected.
            launcher = start(exe, data, root, 'first', desktop, 'de_DE')
            state = launcher.wait(lambda s: s['page'] == 'language', 'The first start must open the language list')
            assert state['language_required'] and state['system_language'] == 'de' and state['launcher_language'] == 'de', state
            assert state['play_label'] == 'Spielen', 'The preselected language must translate the launcher'

            # Selecting a row previews the language; only Continue saves it.
            launcher.send('language pt-BR')
            launcher.wait(lambda s: s['launcher_language'] == 'pt-BR' and s['play_label'] == 'Jogar', 'Selection must preview')
            assert 'language' not in settings(data).get('launcher', {}), 'A preview must not save the language'
            launcher.send('language-confirm')
            launcher.wait(lambda s: s['page'] == 'play' and not s['language_required'], 'Continue must open Play')
            assert settings(data)['launcher']['language'] == 'pt-BR', 'Continue must save [launcher].language'
            assert 'language' not in settings(data) or settings(data)['language'] == 'en', 'The game text language must not change'
            launcher.send('close')
            assert launcher.exit_code() == 0

            # The next start uses the saved language and does not ask again.
            launcher = start(exe, data, root, 'second', desktop, 'fr-FR')
            state = launcher.wait(lambda s: s['page'] == 'play', 'A saved language must skip the list')
            assert state['launcher_language'] == 'pt-BR' and state['system_language'] == 'fr', state

            # Files > Language reopens the list. Back discards the preview.
            launcher.send('language-page', 'language it')
            launcher.wait(lambda s: s['page'] == 'language' and s['launcher_language'] == 'it', 'The list must reopen')
            launcher.send('language-back')
            launcher.wait(lambda s: s['page'] == 'play' and s['launcher_language'] == 'pt-BR', 'Back must keep the saved language')
            assert settings(data)['launcher']['language'] == 'pt-BR'
            launcher.send('close')
            assert launcher.exit_code() == 0

            # Unsupported system languages start in English; a regional variant matches its language.
            for name, locale, expected in (('japanese', 'ja-JP', 'en'), ('portugal', 'pt-PT', 'pt-BR'), ('mexico', 'es_MX', 'es')):
                folder = root / ('data-' + name)
                folder.mkdir()
                launcher = start(exe, folder, root, name, desktop, locale)
                state = launcher.wait(lambda s: s['page'] == 'language', 'The first start must open the list')
                assert state['launcher_language'] == expected, (locale, state)
                launcher.send('close')
                assert launcher.exit_code() == 0
                assert 'launcher' not in settings(folder), 'Closing without Continue must not save a language'
        except BaseException:
            for launcher in Launcher.started:
                launcher.report(desktop)
            raise
        finally:
            for launcher in Launcher.started:
                launcher.close()
            desktop.close()
    print('Launcher language checks passed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
