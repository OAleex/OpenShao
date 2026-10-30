"""Exercise incomplete data folders, oversized logs and report fallback without desktop access."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import zipfile

parser=argparse.ArgumentParser()
parser.add_argument('--exe',required=True,type=Path)
parser.add_argument('--case',required=True,choices=['empty','corrupt','unicode','fallback','truncated','long-line'])
args=parser.parse_args()
mode=0
if os.name=='nt':
    import ctypes
    api=ctypes.WinDLL('kernel32')
    mode=api.SetErrorMode(0x0001|0x0002|0x8000)
try:
    with tempfile.TemporaryDirectory(prefix='openshao-report-edge-') as folder:
        root=Path(folder)
        data=root/('private-\u00e7-\u6771' if args.case=='unicode' else 'data')
        if args.case!='empty':
            data.mkdir();(data/'logs').mkdir()
            (data/'logs/openshao.log').write_text('C0000005 AMD Radeon technical-start\ntechnical-end\n',encoding='utf-8')
        if args.case in ('corrupt','unicode'):
            (data/'settings.toml').write_text('renderer = [\n' if args.case=='corrupt' else 'renderer="vulkan"\n',encoding='utf-8')
            (data/'mods').mkdir();(data/'mods/installed.toml').write_text('[[mods\n')
        if args.case=='fallback':
            (data/'reports').write_bytes(b'preserve-this-file')
        if args.case=='truncated':
            size=2*1024*1024
            offset=size-512*1024
            prefix=b'technical-start C0000005 AMD Radeon\n'
            marker=b'Path C:\\Users\\'
            private=b'HistoricalAccount\\PRIVATE-TAIL-LOCATION\\file.iso\n'
            start=offset-len(marker)
            filler=(b'diagnostic line\n'*(size//16+2))
            content=prefix+filler[:start-len(prefix)-1]+b'\n'+marker+private
            end=b'\ntechnical-end\n'
            content+=filler[:size-len(content)-len(end)]+end
            assert len(content)==size
            (data/'logs/openshao.log').write_bytes(content)
        if args.case=='long-line':
            (data/'logs/openshao.log').write_text('technical-start C0000005\nPath C:\\Users\\'+('x'*16000)+'\\private.iso\ntechnical-end\n')
        archive=None
        try:
            command=[str(args.exe.resolve()),'--support-report','--data-dir',str(data)]
            run=subprocess.run(command,capture_output=True,timeout=30,
                               creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
            assert run.returncode==0,('Report export must survive '+args.case,run.returncode,run.stderr[-1000:])
            archive=Path(run.stdout.decode('utf-8').strip())
            assert archive.is_file(),('Printed ZIP must exist',archive)
            with zipfile.ZipFile(archive) as z:
                assert z.testzip() is None
                content='\n'.join(z.read(n).decode('utf-8') for n in z.namelist())
                assert 'HistoricalAccount' not in content and 'PRIVATE-TAIL-LOCATION' not in content, 'Truncation must discard partial path-bearing lines'
                if args.case in ('truncated','long-line'):
                    assert 'technical-start' in content and 'technical-end' in content, 'Keep the beginning and end of large logs'
                if args.case=='corrupt':
                    assert json.loads(z.read('settings.json'))=={}
                if args.case=='unicode':assert json.loads(z.read('settings.json'))['renderer']=='vulkan'
            if args.case=='fallback':
                assert archive.parent==Path(tempfile.gettempdir())/'OpenShao/reports'
                assert (data/'reports').read_bytes()==b'preserve-this-file'
        finally:
            if archive and archive.is_relative_to(Path(tempfile.gettempdir())/'OpenShao/reports'):
                archive.unlink(missing_ok=True)
finally:
    if os.name=='nt':api.SetErrorMode(mode)
print('PASS: report edge case',args.case)
