"""Validate required game files without starting the game or using personal data."""
import argparse
import ctypes
import json
from pathlib import Path
import subprocess
import tempfile
import zipfile
from recomp_game_fixture import game_files, write_game_folder, write_game_image, write_zip64_offsets


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--xbe', required=True, type=Path)
    args = parser.parse_args()
    exe = args.exe.resolve()
    files = game_files(args.xbe.read_bytes())
    with tempfile.TemporaryDirectory(prefix='openshao-game-validation-') as temporary:
        root = Path(temporary)
        data = root / 'data'
        game = root / 'complete game'
        write_game_folder(game, files)

        def run(source, expected=0, message='', register=False):
            result = subprocess.run([str(exe), '--background', '--data-dir', str(data),
                                     '--register-disc' if register else '--validate-disc', str(source)],
                                    capture_output=True, text=True, encoding='utf-8', timeout=20,
                                    creationflags=subprocess.CREATE_NO_WINDOW | subprocess.BELOW_NORMAL_PRIORITY_CLASS)
            assert result.returncode == expected, (source, expected, result.returncode, result.stderr)
            assert not list((data / 'cache/support').glob('worker-*')), (
                'File-validation fixtures must not start an interactive crash-report worker')
            if expected:
                assert message.lower() in result.stderr.lower(), result.stderr
                assert (data / 'game-source.toml').read_bytes() == saved, 'Rejected input changed the remembered game'
            else:
                assert json.loads(result.stdout) == {'valid': True}

        run(game, register=True)
        saved = (data / 'game-source.toml').read_bytes()
        zip64 = root / 'zip64-offsets.zip'
        write_zip64_offsets(zip64, files)
        run(zip64)
        write_zip64_offsets(zip64, files, invalid_offset=True)
        run(zip64, 3, 'ZIP')
        for name, payload in files.items():
            if name == 'default.xbe':
                continue
            path = game / name
            path.unlink()
            run(game, 3, name.replace('/', '\\'))
            path.write_bytes(b'')
            run(game, 3, name.replace('/', '\\'))
            path.write_bytes(payload)
        path = game / 'GAMEDATA.WAD'
        path.write_bytes(b'wrong archive header' + bytes(32))
        run(game, 3, 'GAMEDATA.WAD')
        path.write_bytes(files['GAMEDATA.WAD'])
        path = game / 'front/movies/opening.sfd'
        path.write_bytes(b'not a movie')
        run(game, 3, 'OPENING.SFD')
        path.write_bytes(files['front/movies/opening.sfd'])
        path = game / 'GAMEDATA.WAE'
        path.unlink()
        path.mkdir()
        run(game, 3, 'GAMEDATA.WAE')
        path.rmdir()
        path.write_bytes(files['GAMEDATA.WAE'])

        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32,
                                      ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
        kernel.CreateFileW.restype = ctypes.c_void_p
        kernel.CloseHandle.argtypes = [ctypes.c_void_p]
        handle = kernel.CreateFileW(str(path), 0x80000000, 0, None, 3, 0, None)
        assert handle not in (None, ctypes.c_void_p(-1).value), ctypes.get_last_error()
        try:
            run(game, 3, 'GAMEDATA.WAE')
        finally:
            kernel.CloseHandle(handle)

        run(game / 'default.xbe')
        image = root / 'complete.xiso.iso'
        write_game_image(image, files)
        run(image)
        altered = dict(files)
        altered['GAMEDATA.WAE'] = b'changed asset contents'
        write_game_image(image, altered)
        run(image)
        for missing in ('GAMEDATA.WAK', 'front/movies/final.sfd'):
            incomplete = dict(files)
            del incomplete[missing]
            write_game_image(image, incomplete)
            run(image, 3, missing.replace('/', '\\'))
        write_game_image(image, files)
        with image.open('r+b') as stream:
            stream.truncate(image.stat().st_size - 2048)
        run(image, 3, 'OPENING.SFD')

        incomplete = dict(files)
        del incomplete['GAMEDATA.WAG']
        archive = root / 'incomplete.zip'
        with zipfile.ZipFile(archive, 'w') as zip_file:
            for name, payload in incomplete.items():
                zip_file.writestr('game/' + name, payload)
        run(archive, 3, 'GAMEDATA.WAG', register=True)
        assert not list((data / 'game').glob('*.partial'))

        wrong = bytearray(files['default.xbe'])
        wrong[-1] ^= 0xff
        (game / 'default.xbe').write_bytes(wrong)
        run(game, 3, 'not supported')
        run(image, 3, 'OPENING.SFD')
    print('PASS: required files, invalid headers, unreadable files, truncated images, ZIP rollback, and XBE version checks')


if __name__ == '__main__':
    main()
