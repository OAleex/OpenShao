"""Check that the launcher stays on its game page until a usable game is chosen."""
import argparse
from pathlib import Path
import sys
import tempfile
import time
import tomllib
from recomp_game_fixture import game_files, write_game_folder
from recomp_launcher_test import Desktop, Launcher


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--xbe', type=Path)
    args = parser.parse_args()
    exe = args.exe.resolve()
    desktop = Desktop()
    with tempfile.TemporaryDirectory(prefix='openshao-setup-', ignore_cleanup_errors=True) as directory:
        try:
            root = Path(directory)
            data = root / 'data'
            selection = data / 'game-source.toml'

            # The first start opens on the game page, and the tabs stay closed until a game is chosen.
            launcher = Launcher(exe, ['--data-dir', data], root, 'first-run', desktop)
            launcher.wait(lambda s: s['page'] == 'game' and not s['game_busy'], 'The first start must open on game selection')
            launcher.send('page files', 'game-back', 'play')
            time.sleep(.5)
            state = launcher.wait(lambda s: True, 'The launcher must keep running')
            assert state['page'] == 'game' and state['result'] == -1, ('Game selection must stay open without a game', state)

            # An unusable choice reports the problem and keeps the page.
            launcher.send('choose-game ' + str(root / 'missing.iso'))
            state = launcher.wait(lambda s: s['game_error'] and not s['game_busy'], 'An unusable game must be reported')
            assert state['page'] == 'game', state
            launcher.send('close')
            assert launcher.exit_code() == 0
            assert not selection.exists(), 'A failed choice must not be remembered'
            assert not (data / 'game').exists()
            assert not (data / 'settings.toml').exists()

            # Settings-only runs never ask for the game.
            launcher = Launcher(exe, ['--configure', '--data-dir', data], root, 'configure', desktop)
            launcher.wait(lambda s: s['page'] == 'play', 'Settings-only runs must open on Play')
            launcher.send('close')
            assert launcher.exit_code() == 0

            if args.xbe:
                game = root / 'game files'
                game.mkdir()
                write_game_folder(game, game_files(args.xbe.read_bytes()))

                # Choosing a complete folder remembers it and opens Play.
                launcher = Launcher(exe, ['--data-dir', data], root, 'select', desktop)
                launcher.wait(lambda s: s['page'] == 'game', 'The first start must open on game selection')
                launcher.send('choose-game ' + str(game))
                state = launcher.wait(lambda s: s['page'] == 'play' and not s['game_busy'], 'A valid game must open Play', timeout=60)
                assert Path(state['source']) == game, state
                assert Path(tomllib.loads(selection.read_text())['disc']) == game
                remembered = selection.read_bytes()
                launcher.send('close')
                assert launcher.exit_code() == 0
                assert not (data / 'settings.toml').exists()

                # A remembered game survives repeated launcher restarts.
                for restart in range(3):
                    launcher = Launcher(exe, ['--data-dir', data], root, 'remembered-' + str(restart), desktop)
                    state = launcher.wait(lambda s: s['page'] == 'play', 'A remembered game must open Play')
                    assert Path(state['source']) == game, state
                    launcher.send('page files', 'choose-game')
                    launcher.wait(lambda s: s['page'] == 'game', 'Change must open game selection')
                    launcher.send('game-back')
                    launcher.wait(lambda s: s['page'] == 'files', 'Back must return to Files')
                    launcher.send('close')
                    assert launcher.exit_code() == 0
                    assert selection.read_bytes() == remembered, 'Restarting must preserve the selected game'

                # An earlier faulty migration could remember the data folder as the game.
                selection.write_text("disc = '" + data.as_posix() + "'\n", encoding='utf-8')
                launcher = Launcher(exe, ['--data-dir', data], root, 'invalid-remembered', desktop)
                state = launcher.wait(lambda s: s['page'] == 'game', 'An incomplete remembered folder must reopen game selection')
                assert 'default.xbe' in state['game_error'], state
                launcher.send('choose-game ' + str(game))
                launcher.wait(lambda s: s['page'] == 'play' and not s['game_busy'], 'A valid choice must recover the saved selection', timeout=60)
                assert Path(tomllib.loads(selection.read_text())['disc']) == game
                remembered = selection.read_bytes()
                launcher.send('close')
                assert launcher.exit_code() == 0

                # A game that moved reopens selection and keeps the previous choice until a new one succeeds.
                game.rename(root / 'moved game files')
                launcher = Launcher(exe, ['--data-dir', data], root, 'unavailable', desktop)
                state = launcher.wait(lambda s: s['page'] == 'game', 'A missing game must reopen selection')
                assert 'no longer' in state['game_error'], state
                launcher.send('close')
                assert launcher.exit_code() == 0
                assert selection.read_bytes() == remembered
                assert not (data / 'settings.toml').exists()
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
    print('PASS: the launcher opens on game selection until a usable game is chosen; failed choices are not remembered')


if __name__ == '__main__':
    main()
