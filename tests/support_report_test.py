"""Check report allowlisting, legacy-log scrubbing and immediate crash-worker export."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import time
import zipfile

parser=argparse.ArgumentParser()
parser.add_argument('--exe',required=True,type=Path)
parser.add_argument('--worker',action='store_true')
args=parser.parse_args()
exe=args.exe.resolve()
def started(process):
    if os.name=='nt':
        api=c.WinDLL('kernel32',use_last_error=True)
        api.GetProcessTimes.argtypes=[w.HANDLE,*([c.POINTER(w.FILETIME)]*4)]
        values=[w.FILETIME() for _ in range(4)]
        assert api.GetProcessTimes(int(process._handle),*[c.byref(v) for v in values])
        return (values[0].dwHighDateTime<<32)|values[0].dwLowDateTime
    return int(Path(f'/proc/{process.pid}/stat').read_text().rsplit(')',1)[1].split()[19])
with tempfile.TemporaryDirectory(prefix='openshao-support-') as folder:
    root=Path(folder);data=root/'PrivateAccount data';data.mkdir()
    (data/'logs').mkdir()
    log=data/'logs/openshao_001.log'
    log.write_text('OpenShao version: 0.4.0\nOS: Windows 10\nCPU: AMD Ryzen\nGPU 1: AMD Radeon\n'
                   'Native exception C0000005 at 0000000012340000\n'
                   'File C:\\Users\\PrivateAccount\\Downloads\\private-game.iso\n'
                   'Computer PRIVATE-PC; account PrivateAccount\n',encoding='utf-8')
    (data/'settings.toml').write_text('renderer="vulkan"\nfps_cap=120\nvoice_language="pt-BR"\nsecret="PRIVATE-SECRET"\n'
                                      'join_ip="192.168.1.2"\n[enhancements]\nrestore_dialogue=true\n',encoding='utf-8')
    (data/'mods').mkdir()
    (data/'mods/installed.toml').write_text('[[mods]]\nid="LanguagePack_PT-BR"\nversion="2.0.0"\nenabled=true\nmanifest="PRIVATE-MANIFEST"\n')
    for sub in ['crashes','saves','game','cache']:(data/sub).mkdir()
    dump_bytes=struct.pack('<IIIIIIQ',0x504d444d,0xa793,1,32,0,0,0)+struct.pack('<III',3,4,44)+b'PRIVATE-MEMORY-DUMP'
    (data/'crashes/private.dmp').write_bytes(dump_bytes)
    (data/'crashes/incomplete.dmp').write_bytes(b'MDMP')
    (data/'saves/save.dat').write_bytes(b'PRIVATE-SAVE')
    (data/'game/default.xbe').write_bytes(b'PRIVATE-GAME')
    (data/'cache/credentials.txt').write_bytes(b'PRIVATE-CREDENTIAL')
    before={str(p.relative_to(data)):hashlib.sha256(p.read_bytes()).hexdigest() for p in data.rglob('*') if p.is_file()}
    env=os.environ.copy();env.update(USERNAME='PrivateAccount',USER='PrivateAccount',COMPUTERNAME='PRIVATE-PC')
    flags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0
    if args.worker:
        parent=subprocess.Popen([os.sys.executable,'-c','import time,os;time.sleep(1.0);os._exit(42)'],creationflags=flags)
        state=root/'worker.state';state.write_bytes(bytes(4096))
        task=root/'worker.toml'
        fields={'data':str(data),'session_log':str(log),'version':'0.4.0','kind':'crash',
                'parent_pid':parent.pid,'parent_started':started(parent),'state':str(state),'show_dialog':False}
        task.write_text('\n'.join(k+' = '+(str(v).lower() if isinstance(v,bool) else str(v) if isinstance(v,int) else json.dumps(v))
                                  for k,v in fields.items()),encoding='utf-8')
        worker=subprocess.Popen([str(exe),'--support-worker',str(task)],env=env,creationflags=flags,
                                stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        try:
            parent.wait(timeout=5)
            worker.communicate(timeout=15)
            assert worker.returncode==0,'The independent report worker failed'
        finally:
            for process in [parent,worker]:
                if process.poll() is None:process.kill();process.wait()
        assert not task.exists() and not state.exists(),'Worker control files must be removed after reporting'
    else:
        run=subprocess.run([str(exe),'--support-report','--data-dir',str(data)],env=env,capture_output=True,text=True,
                           creationflags=flags,timeout=20)
        assert run.returncode==0,('The report export command must work without opening the launcher',run.returncode,run.stderr)
    archives=list((data/'reports').glob('*.zip'))
    assert len(archives)==1,'Exactly one report ZIP must be generated for this request'
    with zipfile.ZipFile(archives[0]) as archive:
        names=set(archive.namelist())
        assert {'report.json','session.log','settings.json','mods.json'}<=names,names
        assert all('/' not in name and '\\' not in name for name in names),names
        content='\n'.join(archive.read(name).decode('utf-8') for name in names if not name.endswith('.dmp'))
        for secret in ['PrivateAccount','PRIVATE-PC','PRIVATE-SECRET','PRIVATE-MANIFEST','192.168.1.2',
                       'PRIVATE-MEMORY-DUMP','PRIVATE-SAVE','PRIVATE-GAME','PRIVATE-CREDENTIAL','private-game.iso']:
            assert secret not in content,('Private data survived report export',secret)
        assert 'C0000005' in content and 'AMD Radeon' in content and 'LanguagePack_PT-BR' in content
        settings=json.loads(archive.read('settings.json'))
        assert settings['renderer']=='vulkan' and settings['fps_cap']==120
        metadata=json.loads(archive.read('report.json'))
        assert metadata['kind']==('crash' if args.worker else 'manual')
        if not args.worker:
            assert 'crash.dmp' in names,'Manual reports must include the latest completed crash dump'
            assert archive.read('crash.dmp')==dump_bytes,'Dumps must be preserved byte for byte'
            assert metadata['dump_included'] is True
        else:
            assert 'crash.dmp' not in names,'An abrupt exit must not be paired with a dump from a different crash'
        if args.worker and os.name=='nt':
            assert '0000002A' in metadata['reason'],'Abrupt exits must retain the operating system exit code'
    after={str(p.relative_to(data)):hashlib.sha256(p.read_bytes()).hexdigest() for p in data.rglob('*')
           if p.is_file() and 'reports' not in p.relative_to(data).parts}
    assert before==after,'Export must preserve source logs, settings, saves and game files'
print('PASS:','immediate crash ZIP' if args.worker else 'manual ZIP','contains the correct diagnostics and preserves source files')
