"""Check ISO registration without a game window or changes to the disc image."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
import tomllib
import zipfile
from recomp_game_fixture import game_files, write_game_folder, write_game_image


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--disc', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--xbe', required=True, type=Path)
    args = parser.parse_args()
    exe, disc = args.exe.resolve(), args.disc.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    before = disc.stat()
    with tempfile.TemporaryDirectory(prefix='image-selection-', dir=args.output) as directory:
        root = Path(directory)
        data = root / 'application-data'

        def run(image, register=True, expected=0):
            result = subprocess.run([str(exe), '--data-dir', str(data),
                                     '--register-disc' if register else '--validate-disc', str(image)],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding='utf-8', timeout=30,
                                    creationflags=subprocess.CREATE_NO_WINDOW | subprocess.BELOW_NORMAL_PRIORITY_CLASS)
            # The independent report worker can outlive a normal command briefly.
            # Wait for its owned mappings before TemporaryDirectory removes them.
            support = data / 'cache/support'
            deadline = time.monotonic() + 5
            while list(support.glob('worker-*')) and time.monotonic() < deadline:
                time.sleep(.02)
            assert not list(support.glob('worker-*')), 'The completed command must release its report worker files'
            assert result.returncode == expected, (result.returncode, result.stderr[-4000:])
            if not expected:
                assert json.loads(result.stdout) == {'valid': True}

        run(disc)
        selection = data / 'game-source.toml'
        assert Path(tomllib.loads(selection.read_text())['disc']) == disc
        saved = selection.read_bytes()
        invalid = root / 'invalid.iso'
        invalid.write_bytes(b'invalid image')
        run(invalid, expected=3)
        assert selection.read_bytes() == saved, 'An invalid image must preserve the previous selection'
        xbe = args.xbe.read_bytes()
        files = game_files(xbe)
        xiso = root / 'game-partition.xiso'
        write_game_image(xiso, files)
        run(xiso, register=False)
        alias = root / 'game.xiso.iso'
        os.link(xiso, alias)
        run(alias, register=False)
        assert selection.read_bytes() == saved, 'Validation must not change the remembered path'
        assert selection.read_bytes() == saved
        assert not any(data.rglob('default.xbe')), 'Direct image reading must not extract the executable'
        folder = root / 'extracted game'
        folder.mkdir()
        write_game_folder(folder, files)
        run(folder / 'default.xbe')
        assert Path(tomllib.loads(selection.read_text())['disc']) == folder
        archive_path = root / 'game.zip'
        with zipfile.ZipFile(archive_path, 'w', zipfile.ZIP_DEFLATED) as archive:
            archive.write(xiso, 'disc/game.xiso.iso')
        run(archive_path)
        installed = Path(tomllib.loads(selection.read_text())['disc'])
        assert installed.is_file() and installed.is_relative_to(data / 'game')
        assert installed.read_bytes() == xiso.read_bytes()
        saved = selection.read_bytes()
        with zipfile.ZipFile(root / 'unsafe.zip', 'w') as archive:
            archive.writestr('../outside.txt', 'escape')
            archive.write(xiso, 'game.iso')
        run(root / 'unsafe.zip', expected=3)
        assert not (root / 'outside.txt').exists()
        assert selection.read_bytes() == saved and installed.is_file()
        with zipfile.ZipFile(root / 'files.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
            for name, payload in files.items():
                archive.writestr('game/' + name, payload)
            archive.writestr('game/assets/sample.dat', b'game-data')
        run(root / 'files.zip')
        installed_folder = Path(tomllib.loads(selection.read_text())['disc'])
        assert (installed_folder / 'assets/sample.dat').read_bytes() == b'game-data'
        saved = selection.read_bytes()
        with zipfile.ZipFile(root / 'wrong.zip', 'w') as archive:
            archive.writestr('default.xbe', b'wrong-game')
        run(root / 'wrong.zip', expected=3)
        assert selection.read_bytes() == saved and installed_folder.is_dir()
        for name in ('../escape', 'C:/escape', 'CON.txt', 'nested/NUL.txt', 'file:stream'):
            with zipfile.ZipFile(root / 'invalid-path.zip', 'w') as archive:
                archive.writestr(name, 'unsafe')
                archive.writestr('default.xbe', xbe)
            run(root / 'invalid-path.zip', expected=3)
            assert selection.read_bytes() == saved
        with zipfile.ZipFile(root / 'duplicate.zip', 'w') as archive:
            archive.writestr('default.xbe', xbe)
            archive.writestr('DEFAULT.XBE', xbe)
        run(root / 'duplicate.zip', expected=3)
        with zipfile.ZipFile(root / 'link.zip', 'w') as archive:
            info = zipfile.ZipInfo('link')
            info.create_system = 3
            info.external_attr = 0o120777 << 16
            archive.writestr(info, '../outside')
            archive.writestr('default.xbe', xbe)
        run(root / 'link.zip', expected=3)
        with zipfile.ZipFile(root / 'corrupt.zip', 'w') as archive:
            archive.writestr('default.xbe', xbe)
        corrupt = bytearray((root / 'corrupt.zip').read_bytes())
        corrupt[30 + len('default.xbe')] ^= 0xff
        (root / 'corrupt.zip').write_bytes(corrupt)
        run(root / 'corrupt.zip', expected=3)
        assert selection.read_bytes() == saved
        assert not list((data / 'game').glob('*.partial'))
    after = disc.stat()
    assert (before.st_size, before.st_mtime_ns) == (after.st_size, after.st_mtime_ns)
    print('PASS: ISO, XISO, XBE folder, ZIP imports, unsafe archives, selection preservation, and unchanged original image')


if __name__ == '__main__':
    main()
