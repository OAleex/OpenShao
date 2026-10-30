"""Exercise Support without opening a window or requiring game files."""
import argparse
import json
import struct
from pathlib import Path
import tempfile
import zipfile
from recomp_launcher_test import Desktop, Launcher

parser = argparse.ArgumentParser()
parser.add_argument('--exe', required=True, type=Path)
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='openshao-support-launcher-') as folder:
    root = Path(folder)
    desktop = Desktop()
    try:
        for name, arguments in [('missing-game', []), ('configured', ['--configure'])]:
            data = root / (name + '-data')
            data.mkdir()
            (data/'crashes').mkdir()
            dump=struct.pack('<IIIIIIQ',0x504d444d,0xa793,1,32,0,0,0)+struct.pack('<III',3,4,44)+b'LAUNCHER-DUMP'
            (data/'crashes/previous.dmp').write_bytes(dump)
            (data / 'settings.toml').write_text('[launcher]\ncheck_updates=false\n')
            launcher = Launcher(args.exe.resolve(), ['--data-dir', data, *arguments], root, name, desktop)
            launcher.wait(lambda s: s['page'] in ('game', 'play'), 'The launcher must open')
            launcher.send('page support', 'generate-report', 'generate-report')
            state = launcher.wait(lambda s: s['page']=='support' and bool(s.get('report_path')) and not s.get('report_busy'),
                                  'Support must export a ZIP, including when no game is available', timeout=12)
            archive = Path(state['report_path'])
            assert archive.is_file() and not state['report_error']
            assert len(list((data/'reports').glob('*.zip')))==1,'Repeated input while generating must produce one ZIP'
            with zipfile.ZipFile(archive) as z:
                assert json.loads(z.read('report.json'))['kind']=='manual'
                assert z.read('crash.dmp')==dump,'The Support page must include the latest available dump automatically'
            launcher.send('generate-report')
            launcher.wait(lambda s: s['report_path']!=str(archive) and not s['report_busy'],
                          'Support must allow another export after completion')
            assert len(list((data/'reports').glob('*.zip')))==2
            assert (data/'settings.toml').read_text()=='[launcher]\ncheck_updates=false\n','Report export must preserve settings'
            launcher.send('close')
            assert launcher.exit_code()==0
    finally:
        for launcher in Launcher.started:
            launcher.close()
        desktop.close()
print('PASS: Support exports diagnostics with and without a usable game')
