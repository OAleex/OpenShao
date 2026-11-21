"""Real-game regression: codes, held modifier, session unlock and pause/Nexus navigation."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--game', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--skip-original-cheats', action='store_true')
    args = parser.parse_args()
    run = args.output.resolve() / str(time.time_ns())
    run.mkdir(parents=True)
    exe = run / 'OpenShao.exe'
    shutil.copy2(args.exe.resolve(), exe)
    game = args.game.resolve()
    xbe = game / 'default.xbe'
    source_hash = hashlib.sha256(xbe.read_bytes()).hexdigest()
    data = run / 'data'
    data.mkdir()
    (data / 'settings.toml').write_text('language="en"\nmaster_volume=0\n')
    environment = os.environ.copy()
    environment.update(XBE_CAPTURE_DIR=str(run / 'captures'), OPENSHAO_CAPTURE_HUD='1',
                       OPENSHAO_CAPTURE_NATIVE_PIXELS='1', OPENSHAO_CAPTURE_SUITE='0')
    user, kernel = c.WinDLL('user32', use_last_error=True), c.WinDLL('kernel32', use_last_error=True)
    callback = c.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
    user.EnumWindows.argtypes = [callback,w.LPARAM]
    user.GetWindowThreadProcessId.argtypes = [w.HWND,c.POINTER(w.DWORD)]
    user.GetClassNameW.argtypes = [w.HWND,w.LPWSTR,c.c_int]
    user.IsWindowVisible.argtypes = [w.HWND]
    user.PostMessageW.argtypes = [w.HWND,w.UINT,w.WPARAM,w.LPARAM]
    kernel.ReadProcessMemory.argtypes = [w.HANDLE,c.c_void_p,c.c_void_p,c.c_size_t,c.POINTER(c.c_size_t)]
    hwnd, base = None, None
    visible, sent, releases, queue, screenshots, flags = [], [], [], [], [], []
    failure = None
    log_path = run / 'native.log'

    def observe(window, _):
        nonlocal hwnd
        owner = w.DWORD()
        user.GetWindowThreadProcessId(window,c.byref(owner))
        if owner.value == process.pid:
            if user.IsWindowVisible(window):
                visible.append(int(window))
            name = c.create_unicode_buffer(256)
            user.GetClassNameW(window,name,len(name))
            if name.value == 'OpenShao':
                hwnd = window
        return True

    observer = callback(observe)

    def read(address, size):
        result, count = c.create_string_buffer(size), c.c_size_t()
        assert base and kernel.ReadProcessMemory(int(process._handle),base+address,result,size,c.byref(count)) and count.value == size
        return result.raw

    def down(vk, elapsed, release=True):
        assert hwnd and user.PostMessageW(hwnd,0x100,vk,0)
        sent.append({'elapsed':elapsed,'key':vk,'down':True})
        if release:
            releases.append((elapsed+.3,vk))

    def up(vk, elapsed):
        trace = None
        if base:
            try:
                info = struct.unpack('<I',read(0x6ca3a4,4))[0]
                if info:
                    state = struct.unpack('<9I',read(info,36))
                    if state[1] <= state[3] <= 64:
                        trace = {'count':state[1], 'idle':state[0],
                                 'buttons':list(struct.unpack('<'+'H'*state[1],read(state[7],state[1]*2))) if state[1] else [],
                                 'held':struct.unpack('<H',read(0x67c668,2))[0]}
            except AssertionError:
                pass
        assert hwnd and user.PostMessageW(hwnd,0x101,vk,0)
        sent.append({'elapsed':elapsed,'key':vk,'down':False,'native_cheat':trace})

    def enqueue(delay, operation, value=None):
        queue.append((delay,operation,value))

    def prepare():
        # PxInput masks are read from the retail game's own registration tables.
        keys = {0x80:ord('J'),0x1000:38,0x4000:40,0x8000:37,0x2000:39,
                0x4:16,0x8:ord('E'),0x2:ord('C')}
        if not args.skip_original_cheats:
            for table, flag in [(0x4fc3cc,0x5a378e),(0x4fc3dc,0x5a378d),(0x4fc3ec,0x5a378c)]:
                enqueue(.1,'flag-clear',flag)
                enqueue(1,'hold',ord('Q'))
                for button in struct.unpack('<7H',read(table,14)):
                    enqueue(.45,'press',keys[button])
                enqueue(.7,'release',ord('Q'))
                enqueue(.7,'flag',flag)
        secret = [38]*4+[40]*4+[ord('J'),27]
        for vk in secret:
            enqueue(1.1,'press',vk)
        enqueue(.7,'locked')
        enqueue(.7,'hold',ord('Q'))  # White / default LB binding.
        for vk in secret[:8]:
            enqueue(.45,'press',vk)
        enqueue(.7,'release',ord('Q'))
        enqueue(.7,'hold',ord('Q'))
        for vk in secret[8:]:
            enqueue(.45,'press',vk)
        enqueue(.7,'locked')
        enqueue(.7,'release',ord('Q'))
        enqueue(.7,'hold',ord('Q'))
        for vk in [38]*4+[40]*3+[ord('J'),27]:
            enqueue(.45,'press',vk)
        enqueue(.7,'locked')
        enqueue(.7,'release',ord('Q'))
        enqueue(.7,'hold',ord('Q'))
        for vk in secret:
            enqueue(1.1,'press',vk)
        enqueue(.7,'unlocked')
        enqueue(.7,'release',ord('Q'))
        enqueue(.7,'selection')
        enqueue(.7,'capture','main-unlocked')
        previous = 0
        for at,vk in [(0,32),(6,32),(9,32),(16,32),(21,13),(31,13),(37,32),
                      (48,32),(54,32),(62,32),(67,32),(77,32),(104,8)]:
            enqueue(at-previous,'press',vk)
            previous = at
        for delay,operation,value in [
            (3,'pause-list',None),(.1,'capture','pause'),
            (1,'press',40),(.5,'press',40),(.5,'press',40),(.5,'press',40),(.5,'press',40),
            (.5,'press',32),(3,'capture','nexus'),
            (2,'press',27),(3,'pause-list',None),(.1,'capture','pause-return'),(2,'press',ord('J')),
            (3,'capture','nexus-reopen'),(2,'press',27),(2,'press',32),
            (3,'capture','gameplay')]:
            enqueue(delay,operation,value)

    print('RUN',run,flush=True)
    with log_path.open('wb') as log:
        process = subprocess.Popen([str(exe),'--background','--timeout','360','--game',str(game),
                                    '--data-dir',str(data),'--save-dir',str(run/'saves')],
                                   cwd=run,env=environment,stdout=log,stderr=subprocess.STDOUT,
                                   creationflags=subprocess.CREATE_NO_WINDOW|subprocess.BELOW_NORMAL_PRIORITY_CLASS)
        began = time.monotonic()
        started = accepted = prepared = closed = False
        next_event = 0
        waiting = None
        try:
            while process.poll() is None:
                elapsed = time.monotonic()-began
                user.EnumWindows(observer,0)
                assert not visible,'The test created a visible window'
                text = log_path.read_text(errors='replace')
                if base is None:
                    match = re.search(r'memory: guest space at ([0-9a-fA-Fx]+)',text)
                    if match:
                        base = int(match.group(1),16)
                if hwnd and elapsed>=24 and not started:
                    down(13,elapsed);started=True
                if hwnd and elapsed>=31 and not accepted:
                    down(32,elapsed);accepted=True
                if not prepared and 'retail menu top=00598430' in text:
                    prepare();prepared=True;next_event=elapsed+2
                    print('Main menu ready; checking original and secret codes.',flush=True)
                for due,vk in list(releases):
                    if elapsed>=due:
                        up(vk,elapsed);releases.remove((due,vk))
                frames = sorted((run/'captures').glob('*/final_native.bmp'))
                if waiting and len(frames)>len(screenshots):
                    screenshots.append({'stage':waiting,'path':str(frames[-1].relative_to(run))})
                    print('Captured',waiting,flush=True)
                    waiting=None
                    next_event=elapsed+(queue[0][0] if queue else 1)
                if prepared and queue and not waiting and not releases and elapsed>=next_event:
                    _,operation,value=queue.pop(0)
                    if operation in ('press','hold'):
                        down(value,elapsed,release=operation=='press')
                    elif operation=='release':
                        up(value,elapsed)
                    elif operation=='flag-clear':
                        assert read(value,1)==b'\x00',f'Original cheat flag was already set: {value:08x}'
                    elif operation=='flag':
                        assert read(value,1)==b'\x01',f'Original cheat did not set {value:08x}'
                        flags.append(hex(value));print('Original cheat verified:',hex(value),flush=True)
                    elif operation=='locked':
                        assert 'Nexus unlocked' not in text,'Incorrect sequence unlocked Nexus'
                    elif operation=='unlocked':
                        assert text.count('Nexus unlocked for this session')==1,'Valid sequence failed to unlock exactly once'
                        voice=re.search(r'Nexus unlocked for this session \(voice=([0-9A-Fa-f]+)\)',text)
                        assert voice and int(voice.group(1),16),'Nexus confirmation voice did not start'
                        print('Secret unlock verified.',flush=True)
                    elif operation=='selection':
                        assert struct.unpack('<I',read(0x598468,4))[0]==0,'Unexpected main-menu selection'
                    elif operation=='pause-list':
                        original=struct.unpack('<8I',read(0x4f1cc0,32))
                        assert original==(0x1a4,0x15263,0x1a5,0x11bcc,0x1a7,0x16262,0xffffffff,0),\
                            'The original static three-row Select List at 0x4F1CC0 was modified'
                        independent=struct.unpack('<10I',read(0x4f1ce0,40))
                        assert independent==(0x1a3,0x15506,0x1a4,0x15263,0x1a5,0x11bcc,
                                             0x1a7,0x16262,0xffffffff,0),\
                            'The adjacent retail four-row menu at 0x4F1CE0 was modified'
                        expanded=re.findall(r'Gameplay pause expanded: list=([0-9a-fA-F]{8}) rows=(\d+)',text)
                        assert expanded,'The native Select-list expansion did not run'
                        list_address,logged_rows=expanded[-1]
                        list_address=int(list_address,16)
                        owned=struct.unpack('<I',read(list_address+0x54,4))[0]
                        count=struct.unpack('<I',read(list_address+0x64,4))[0]
                        unlocked='Nexus unlocked for this session' in text
                        expected_count=6 if unlocked else 5
                        assert count==expected_count==int(logged_rows) and owned not in (0,0x4f1cc0,0x4f1ce0),\
                            'The Select-list must point to its owned 5/6-row allocation'
                        pairs=struct.unpack(f'<{(count+1)*2}I',read(owned,(count+1)*8))
                        expected=(0x1a4,0x15263,0x1a5,0x11bcc,0x1a7,0x16262,
                                  0xffffff00,0x135c1,0xffffff01,0x135c1)
                        if unlocked:expected+=(0x19f,0x135c6)
                        expected+=(0xffffffff,0)
                        assert pairs==expected,'The owned pause list does not preserve the retail rows and optional Nexus action'
                    elif operation=='capture':
                        down(0x72,elapsed);waiting=value
                    next_event=elapsed+(queue[0][0] if queue else 1)
                if prepared and not queue and not waiting and not releases and not closed:
                    assert user.PostMessageW(hwnd,0x10,0,0);closed=True
                assert elapsed<370,'The diagnostic exceeded its deadline'
                time.sleep(.04)
        except Exception as error:
            failure=repr(error)
        finally:
            if process.poll() is None:
                process.terminate()
            code=process.wait(timeout=8)
    result={'exit_code':code,'closed':closed,'failure':failure,'original_cheat_flags':flags,
            'screenshots':screenshots,'events':sent,'visible_windows':visible,
            'source_xbe_unchanged':hashlib.sha256(xbe.read_bytes()).hexdigest()==source_hash}
    text=log_path.read_text(errors='replace')
    result['nexus_opens']=text.count('Nexus opened from gameplay pause')
    (run/'result.json').write_text(json.dumps(result,indent=2))
    assert failure is None and code==0 and closed,f'{failure}; see {run / "result.json"}'
    assert result['source_xbe_unchanged'] and len(screenshots)==6 and result['nexus_opens']==2,result
    print('PASS: original codes, rejected/valid sequence, Nexus open/return/reopen and gameplay:',run,flush=True)


if __name__=='__main__':
    main()
