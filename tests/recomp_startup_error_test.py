"""Check early folder/configuration failures on a private desktop."""
import argparse
import ctypes as c
from ctypes import wintypes as w
from pathlib import Path
import re
import tempfile
import time
import zipfile
from recomp_error_dialog_test import PrivateProcess

parser=argparse.ArgumentParser()
parser.add_argument('--exe',required=True,type=Path)
args=parser.parse_args()
user=c.WinDLL('user32',use_last_error=True)
kernel=c.WinDLL('kernel32',use_last_error=True)
callback=c.WINFUNCTYPE(w.BOOL,w.HWND,w.LPARAM)
user.CreateDesktopW.argtypes=[w.LPCWSTR,w.LPCWSTR,c.c_void_p,w.DWORD,w.DWORD,c.c_void_p]
user.CreateDesktopW.restype=w.HANDLE
user.CloseDesktop.argtypes=[w.HANDLE]
user.EnumDesktopWindows.argtypes=[w.HANDLE,callback,w.LPARAM]
user.EnumChildWindows.argtypes=[w.HWND,callback,w.LPARAM]
user.GetWindowThreadProcessId.argtypes=[w.HWND,c.POINTER(w.DWORD)]
user.GetWindowTextW.argtypes=[w.HWND,w.LPWSTR,c.c_int]
user.PostMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM]
name='OpenShaoStartupError'+str(time.time_ns())
desktop=user.CreateDesktopW(name,None,None,0,0x01FF,None)
assert desktop,'Private test desktop unavailable'
def text(window):
    value=c.create_unicode_buffer(16384)
    user.GetWindowTextW(window,value,len(value))
    return value.value
try:
    with tempfile.TemporaryDirectory(prefix='openshao-early-startup-') as folder:
        root=Path(folder)
        for scenario in ['folder','settings']:
            data=root/('blocked-data' if scenario=='folder' else 'settings-data')
            if scenario=='folder':data.write_text('synthetic file blocks the requested directory')
            else:
                data.mkdir();(data/'settings.toml').write_text('language = [\n')
            with (root/(scenario+'.log')).open('wb') as output:
                process=PrivateProcess([str(args.exe.resolve()),'--configure','--data-dir',str(data)],name,output,root)
                windows=[];messages=[]
                @callback
                def child(window,_):
                    value=text(window)
                    if value:messages.append(value)
                    return True
                @callback
                def observe(window,_):
                    owner=w.DWORD();user.GetWindowThreadProcessId(window,c.byref(owner))
                    if owner.value==process.pid and text(window)=='OpenShao':
                        before=len(messages);user.EnumChildWindows(window,child,0)
                        # The button can exist before the message control is initialized.
                        if any('Diagnostic ZIP:' in value or 'The diagnostic ZIP could not be created:' in value
                               for value in messages[before:]):windows.append(window)
                    return True
                fallback=None
                try:
                    deadline=time.monotonic()+15
                    while not windows and process.poll() is None and time.monotonic()<deadline:
                        user.EnumDesktopWindows(desktop,observe,0);time.sleep(.05)
                    assert windows,('Missing startup error dialog',scenario)
                    message='\n'.join(messages)
                    match=re.search(r'Diagnostic ZIP:\n([^\n]+)',message)
                    assert match,('Early startup errors must identify a diagnostic ZIP',scenario,message)
                    archive=Path(match[1].strip())
                    assert archive.is_file(),('Reported ZIP must exist before dismissal',archive)
                    with zipfile.ZipFile(archive) as z:session=z.read('session.log').decode('utf-8')
                    if scenario=='folder':
                        fallback=archive
                        assert 'application folders could not be prepared' in message, message
                        assert str(data) not in session,('Folder diagnostics must filter personal paths',session)
                    else:
                        assert 'OpenShao version:' in session and 'CPU:' in session and 'OS:' in session,(
                            'Launcher failures must retain version and system information',session)
                    assert user.PostMessageW(windows[-1],0x0010,0,0)
                    process.wait(timeout=8)
                    expected='Cannot prepare application data' if scenario=='folder' else 'Cannot read settings.toml'
                    assert expected in session,('Error reason must survive in the log',scenario,session)
                    print('PASS:',scenario,'startup failure retains its cause and diagnostic log')
                finally:
                    if process.poll() is None:process.terminate();process.wait(timeout=5)
                    process.close()
                    if fallback and fallback.is_relative_to(Path(tempfile.gettempdir())/'OpenShao/reports'):
                        fallback.unlink(missing_ok=True)
finally:
    user.CloseDesktop(desktop)
