"""Check that a texture source named by OPENSHAO_HD_PACK_DIR is read when no mod is installed, in a hidden game session."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def run(exe, disc, root, environment):
    flags = subprocess.CREATE_NO_WINDOW | subprocess.BELOW_NORMAL_PRIORITY_CLASS if os.name == 'nt' else 0
    try:
        result = subprocess.run([str(exe.resolve()), '--background', '--timeout', '8', '--data-dir', str(root),
                                 '--save-dir', str(root / 'saves'), '--disc', str(disc.resolve())],
                                cwd=root, capture_output=True, timeout=40, creationflags=flags, env=environment)
    except subprocess.TimeoutExpired as error:
        result = subprocess.CompletedProcess(error.cmd, -1, error.stdout or b'', error.stderr or b'')
    text = '\n'.join(file.read_text(encoding='utf-8', errors='replace') for file in sorted((root / 'logs').glob('*.log')))
    return text + result.stdout.decode('utf-8', errors='replace') + result.stderr.decode('utf-8', errors='replace')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--disc', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='openshao-texture-source-') as temp:
        base = Path(temp)
        # A folder with no index is read as a folder of pictures, which always leaves a line in the log.
        pictures = base / 'pictures'
        pictures.mkdir()

        # No mod installed, the variable set: the folder must be read.
        with_variable = base / 'with-variable'
        with_variable.mkdir()
        environment = dict(os.environ, OPENSHAO_HD_PACK_DIR=str(pictures))
        text = run(args.exe, args.disc, with_variable, environment)
        assert not (with_variable / 'mods/installed.toml').exists() or 'enabled = true' not in (with_variable / 'mods/installed.toml').read_text(), \
            'The case needs a data folder with no active mod'
        assert 'Texture pack: developer folder:' in text, 'A texture source named by the variable was ignored: ' + text[-4000:]

        # The variable unset: nothing is read.
        without_variable = base / 'without-variable'
        without_variable.mkdir()
        environment = {key: value for key, value in os.environ.items() if key != 'OPENSHAO_HD_PACK_DIR'}
        text = run(args.exe, args.disc, without_variable, environment)
        assert 'Texture pack: developer folder:' not in text, 'A texture source was read without the variable: ' + text[-4000:]
    print('PASS: a texture source named by OPENSHAO_HD_PACK_DIR is read with no mod installed, and only then')


if __name__ == '__main__':
    main()
