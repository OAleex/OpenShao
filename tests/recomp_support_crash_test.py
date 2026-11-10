"""Verify the prestarted helper survives both native exceptions and sudden termination."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import json
import struct
from pathlib import Path
import tempfile
import time
import zipfile
from recomp_error_dialog_test import PrivateProcess
from recomp_launcher_test import Desktop

parser=argparse.ArgumentParser()
parser.add_argument('--exe',required=True,type=Path)
parser.add_argument('--mode',required=True,choices=['native','native-no-dump','abrupt','clean','external','failfast'])
args=parser.parse_args()
desktop=Desktop()
user=desktop.user;kernel=c.WinDLL('kernel32',use_last_error=True)
kernel.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];kernel.OpenProcess.restype=w.HANDLE
kernel.WaitForSingleObject.argtypes=[w.HANDLE,w.DWORD];kernel.CloseHandle.argtypes=[w.HANDLE]
kernel.TerminateProcess.argtypes=[w.HANDLE,w.UINT]
user.EnumChildWindows.argtypes=[w.HWND,desktop.callback,w.LPARAM]
try:
    with tempfile.TemporaryDirectory(prefix='openshao-crash-flow-') as folder:
        root=Path(folder);data=root/'data';output=(root/'process.log').open('wb')
        old_dump=struct.pack('<IIIIIIQ',0x504d444d,0xa793,1,32,0,0,0)+struct.pack('<III',3,4,44)+b'OLD-CRASH-DUMP'
        if args.mode=='native-no-dump':
            data.mkdir();(data/'crashes').write_bytes(b'preserve-blocked-dump-folder')
        else:
            (data/'crashes').mkdir(parents=True)
            (data/'crashes/old.dmp').write_bytes(old_dump)
        process=PrivateProcess([str(args.exe.resolve()),str(data),args.mode],desktop.name,output,root)
        helper=None;dialogs=[];messages=[]
        def text(window):
            value=c.create_unicode_buffer(16384);user.GetWindowTextW(window,value,len(value));return value.value
        @desktop.callback
        def child(window,_):
            messages.append(text(window));return True
        @desktop.callback
        def observe(window,_):
            if text(window)=='OpenShao':
                user.EnumChildWindows(window,child,0)
                if 'Diagnostic ZIP:' in '\n'.join(messages):dialogs.append(window)
            return True
        try:
            deadline=time.monotonic()+10
            while time.monotonic()<deadline and helper is None:
                states=list((data/'cache/support').glob('*.state'))
                if states:
                    import struct
                    try:
                        state=states[0].read_bytes()
                        # The helper creates the file before writing it; retry until its PID is there.
                        pid=struct.unpack_from('<I',state,12)[0] if len(state)>=16 else 0
                        if pid:helper=kernel.OpenProcess(0x100001,False,pid)
                    except OSError:pass
                time.sleep(.01)
            assert helper,'The report helper must start before launcher/game execution'
            if args.mode=='external':
                assert kernel.TerminateProcess(process.handle,42),'The owned fixture must be terminated externally'
            result=process.wait(timeout=10)
            if args.mode=='clean':
                assert result==0 and kernel.WaitForSingleObject(helper,5000)==0
                assert not list((data/'reports').glob('*.zip')),'Clean shutdown must not produce a crash report'
            else:
                assert result!=0
                deadline=time.monotonic()+15
                while not dialogs and time.monotonic()<deadline:
                    user.EnumDesktopWindows(desktop.handle,observe,0);time.sleep(.03)
                assert dialogs,'A sudden crash must show its ZIP without reopening the launcher'
                archives=list((data/'reports').glob('*.zip'))
                assert len(archives)==1 and str(archives[0]) in '\n'.join(messages)
                assert 'OpenShao > Support > Report a bug' in '\n'.join(messages)
                with zipfile.ZipFile(archives[0]) as z:
                    report=json.loads(z.read('report.json'))
                    assert report['kind']=='crash' and 'renderer=d3d12' in z.read('session.log').decode()
                    if args.mode=='native':
                        assert 'C0000005' in report['reason']
                        assert 'crash.dmp' in z.namelist(),'A native crash must include its own minidump in the ZIP'
                        dump=z.read('crash.dmp')
                        assert dump!=old_dump and dump[:4]==b'MDMP'
                        streams,table=struct.unpack_from('<II',dump,8)
                        entries={kind:(size,rva) for kind,size,rva in
                                 (struct.unpack_from('<III',dump,table+i*12) for i in range(streams))}
                        assert 3 in entries and 4 in entries and 6 in entries,'Dump must contain threads, modules and the fault context'
                        size,rva=entries[6]
                        assert struct.unpack_from('<I',dump,rva+8)[0]==0xc0000005
                        address=struct.unpack_from('<Q',dump,rva+24)[0]
                        assert f'{address:016X}' in report['reason'],'Dump and report must describe the same native fault'
                        assert 16 in entries,'Dump must retain memory protection and allocation state for invalid guest pointers'
                        map_size,map_rva=entries[16]
                        header_size,entry_size,count=struct.unpack_from('<IIQ',dump,map_rva)
                        assert header_size>=16 and entry_size>=48 and count>0
                        assert header_size+entry_size*count<=map_size
                        regions=[map_rva+header_size+i*entry_size for i in range(count)]
                        assert any(struct.unpack_from('<Q',dump,p)[0]<=address<
                                   struct.unpack_from('<Q',dump,p)[0]+struct.unpack_from('<Q',dump,p+24)[0]
                                   and struct.unpack_from('<I',dump,p+32)[0]==0x1000 for p in regions),\
                            'Memory map must identify the faulting instruction as committed memory'
                        context_size,context_rva=struct.unpack_from('<II',dump,rva+160)
                        assert context_size>=256 and struct.unpack_from('<Q',dump,context_rva+248)[0]!=0
                        current=[p for p in (data/'crashes').glob('*.dmp') if p.name!='old.dmp']
                        assert len(current)==1 and current[0].read_bytes()==dump,'ZIP must contain the completed local dump'
                        assert report['dump_included'] is True
                    else:
                        assert 'crash.dmp' not in z.namelist(),'Never attach an old dump to an unrelated abrupt exit'
                        if args.mode=='native-no-dump':
                            assert report['dump_status']=='capture_failed'
                            assert (data/'crashes').read_bytes()==b'preserve-blocked-dump-folder'
                    if args.mode=='external':assert '0000002A' in report['reason']
                    if args.mode=='failfast':assert f'{result:08X}' in report['reason'],report['reason']
                assert user.PostMessageW(dialogs[0],0x0010,0,0)
                assert kernel.WaitForSingleObject(helper,5000)==0,'OK must finish the helper on the private desktop'
                assert len(list((data/'reports').glob('*.zip')))==1,'One crash must produce one ZIP'
        finally:
            if process.poll() is None:process.terminate();process.wait(timeout=5)
            process.close();output.close()
            if helper:
                if kernel.WaitForSingleObject(helper,0)!=0:kernel.TerminateProcess(helper,255);kernel.WaitForSingleObject(helper,5000)
                kernel.CloseHandle(helper)
finally:desktop.close()
print('PASS:',args.mode,'exit has the expected immediate report and dialog behavior')
