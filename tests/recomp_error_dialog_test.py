"""Inspect the real error dialog on a private Windows desktop."""
import argparse
import ctypes
from ctypes import wintypes
import msvcrt
import os
from pathlib import Path
import subprocess
import tempfile
import time


class StartupInfo(ctypes.Structure):
    _fields_ = [('cb', wintypes.DWORD), ('lpReserved', wintypes.LPWSTR), ('lpDesktop', wintypes.LPWSTR),
                ('lpTitle', wintypes.LPWSTR), ('dwX', wintypes.DWORD), ('dwY', wintypes.DWORD),
                ('dwXSize', wintypes.DWORD), ('dwYSize', wintypes.DWORD), ('dwXCountChars', wintypes.DWORD),
                ('dwYCountChars', wintypes.DWORD), ('dwFillAttribute', wintypes.DWORD), ('dwFlags', wintypes.DWORD),
                ('wShowWindow', wintypes.WORD), ('cbReserved2', wintypes.WORD), ('lpReserved2', ctypes.c_void_p),
                ('hStdInput', wintypes.HANDLE), ('hStdOutput', wintypes.HANDLE), ('hStdError', wintypes.HANDLE)]


class ProcessInfo(ctypes.Structure):
    _fields_ = [('hProcess', wintypes.HANDLE), ('hThread', wintypes.HANDLE),
                ('dwProcessId', wintypes.DWORD), ('dwThreadId', wintypes.DWORD)]


class PrivateProcess:
    def __init__(self, command, desktop, output, directory):
        self.api = ctypes.WinDLL('kernel32', use_last_error=True)
        self.api.CreateProcessW.argtypes = [wintypes.LPCWSTR, wintypes.LPWSTR, ctypes.c_void_p, ctypes.c_void_p,
                                           wintypes.BOOL, wintypes.DWORD, ctypes.c_void_p, wintypes.LPCWSTR,
                                           ctypes.POINTER(StartupInfo), ctypes.POINTER(ProcessInfo)]
        self.api.CloseHandle.argtypes = [wintypes.HANDLE]
        self.api.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
        self.api.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        self.api.TerminateProcess.argtypes = [wintypes.HANDLE, wintypes.UINT]
        info = StartupInfo()
        info.cb = ctypes.sizeof(info)
        info.lpDesktop = desktop
        info.dwFlags = 0x100
        handle = msvcrt.get_osfhandle(output.fileno())
        info.hStdOutput = info.hStdError = handle
        process = ProcessInfo()
        with open('NUL', 'rb') as source:
            input_handle = msvcrt.get_osfhandle(source.fileno())
            info.hStdInput = input_handle
            os.set_handle_inheritable(handle, True)
            os.set_handle_inheritable(input_handle, True)
            try:
                created = self.api.CreateProcessW(None, ctypes.create_unicode_buffer(subprocess.list2cmdline(command)),
                                                 None, None, True, 0x08004000, None, str(directory),
                                                 ctypes.byref(info), ctypes.byref(process))
                assert created, ctypes.get_last_error()
            finally:
                os.set_handle_inheritable(handle, False)
                os.set_handle_inheritable(input_handle, False)
        self.handle, self.pid = process.hProcess, process.dwProcessId
        self.api.CloseHandle(process.hThread)

    def poll(self):
        code = wintypes.DWORD()
        assert self.api.GetExitCodeProcess(self.handle, ctypes.byref(code))
        return None if code.value == 259 else code.value

    def wait(self, timeout):
        assert self.api.WaitForSingleObject(self.handle, int(timeout * 1000)) == 0, 'The private process did not exit'
        return self.poll()

    def terminate(self):
        self.api.TerminateProcess(self.handle, 255)

    def close(self):
        self.api.CloseHandle(self.handle)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', required=True, type=Path)
    args = parser.parse_args()
    user = ctypes.WinDLL('user32', use_last_error=True)
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user.CreateDesktopW.argtypes = [wintypes.LPCWSTR, wintypes.LPCWSTR, ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p]
    user.CreateDesktopW.restype = wintypes.HANDLE
    user.CloseDesktop.argtypes = [wintypes.HANDLE]
    user.SetThreadDesktop.argtypes = [wintypes.HANDLE]
    user.GetThreadDesktop.argtypes = [wintypes.DWORD]
    user.GetThreadDesktop.restype = wintypes.HANDLE
    user.EnumDesktopWindows.argtypes = [wintypes.HANDLE, callback_type, wintypes.LPARAM]
    user.EnumChildWindows.argtypes = [wintypes.HWND, callback_type, wintypes.LPARAM]
    user.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
    user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
    desktop_name = 'OpenShaoErrorTest' + str(time.time_ns())
    desktop = user.CreateDesktopW(desktop_name, None, None, 0, 0x01FF, None)
    assert desktop, 'Could not create the private test desktop'
    kernel = ctypes.WinDLL('kernel32')
    original_desktop = user.GetThreadDesktop(kernel.GetCurrentThreadId())
    assert user.SetThreadDesktop(desktop), ctypes.get_last_error()
    try:
        with tempfile.TemporaryDirectory(prefix='openshao-error-dialog-') as folder:
            root = Path(folder)
            data = root / 'app-data-\u00e7'
            with (root / 'process.log').open('wb') as output:
                process = PrivateProcess([str(args.exe.resolve()), '--no-launcher', '--data-dir', str(data), '--disc', str(root / 'missing.iso')],
                                         desktop_name, output, root)
                texts = []
                dialogs = []
                observed = set()

                def text(window):
                    buffer = ctypes.create_unicode_buffer(8192)
                    user.GetWindowTextW(window, buffer, len(buffer))
                    return buffer.value

                def child(window, _):
                    texts.append(text(window))
                    return True

                child_callback = callback_type(child)

                def observe(window, _):
                    owner = wintypes.DWORD()
                    user.GetWindowThreadProcessId(window, ctypes.byref(owner))
                    if owner.value == process.pid:
                        title = text(window)
                        observed.add(title)
                        if title == 'OpenShao':
                            user.EnumChildWindows(window, child_callback, 0)
                            if 'Diagnostic ZIP:' in '\n'.join(texts):
                                dialogs.append(window)
                    return True

                observer = callback_type(observe)
                try:
                    deadline = time.monotonic() + 20
                    while process.poll() is None and time.monotonic() < deadline and not dialogs:
                        ctypes.set_last_error(0)
                        enumerated = user.EnumDesktopWindows(desktop, observer, 0)
                        assert enumerated or not ctypes.get_last_error(), ctypes.get_last_error()
                        time.sleep(.05)
                    assert dialogs, ('The real startup failure did not show an error dialog', observed,
                                     (root / 'process.log').read_text(encoding='utf-8', errors='replace')[-4000:])
                    logs = list((data / 'logs').glob('*.log'))
                    assert len(logs) == 1, 'The session log is missing'
                    archives = list((data / 'reports').glob('*.zip'))
                    assert len(archives)==1 and str(archives[0]) in '\n'.join(texts), ('The ZIP must exist before the dialog opens',texts)
                    assert 'OpenShao > Support > Report a bug' in '\n'.join(texts), texts
                    assert 'Click OK to open the folder with this ZIP selected.' in '\n'.join(texts), texts
                    # The private desktop suppresses Explorer while exercising
                    # the same dismissal path used by the interactive dialog.
                    assert user.PostMessageW(dialogs[0], 0x0010, 0, 0), ctypes.get_last_error()
                    assert process.wait(timeout=5) != 0, 'The invalid source must still fail startup'
                    log = logs[0].read_text(encoding='utf-8', errors='replace')
                    assert 'Error dialog closed.' in log and str(data) not in log, log
                    assert 'Error dialog closed. Log folder:' not in log, log
                finally:
                    if process.poll() is None:
                        process.terminate()
                        process.wait(timeout=5)
                    process.close()
                assert not list(args.exe.parent.glob('missing.iso'))
    finally:
        user.SetThreadDesktop(original_desktop)
        user.CloseDesktop(desktop)
    print('PASS: the real error dialog identifies the completed ZIP and Support without using the user desktop')


if __name__ == '__main__':
    main()
