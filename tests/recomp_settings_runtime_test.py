"""Opt-in real-game Konfigure Recompilation regression using isolated saves and virtual input.

Captures contain original game artwork and must stay in ignored local output.
The driver never shows its game window or changes desktop focus.
"""
from pathlib import Path
import ctypes as c
from ctypes import wintypes as w
import hashlib,json,os,re,shutil,struct,subprocess,time

import argparse
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--exe',type=Path,required=True)
parser.add_argument('--game',type=Path,required=True,help='Original extracted game folder or default.xbe')
parser.add_argument('--save-fixture',type=Path,required=True,help='Read-only source title directory containing profile saves')
parser.add_argument('--mods',type=Path,help='Read-only source mods directory for language packs')
parser.add_argument('--cache-fixture',type=Path,help='Optional read-only prewarmed shader cache directory for faster isolated startup')
parser.add_argument('--verify-pause-exit',action='store_true',help='In the pause Load Game scenario, confirm Yes and verify desktop exit')
parser.add_argument('--language',default='en',choices=['en','fr','de','es','it','pt-BR'])
parser.add_argument('--voice-language',default='en',choices=['en','fr','de','es','it','pt-BR'])
parser.add_argument('--aspect',default='original',choices=['original','16:9','16:10','21:9','32:9','auto'])
parser.add_argument('--cinematic-bars',action='store_true',help='Enable cinematic bars through Enhancements in the cinematic-bars scenario')
parser.add_argument('--scenario',default='navigation',choices=['navigation','menu-layout','persistence','display','native-options','reload','write-failure','remapping','prompts','transition','polish','presentation','input-handoff','details','headings','restart','voices','language-preview','brightness','story-exit','storage-error','exit-desktop','scorpion-preview','scorpion-campaign','scorpion-campaign-enabled','scorpion-quickstart','scorpion-double-save','pause-recomp','pause-recomp-load','cinematic-bars'])
parser.add_argument('--clean-profile',action='store_true',help='Use empty isolated saves for the brightness scenario')
parser.add_argument('--output',type=Path)
parser.add_argument('--settings',type=Path,help='Read-only configuration fixture for restart validation')
parser.add_argument('--restart-count',type=int,default=6,choices=range(1,7),help='Number of consecutive language changes in the restart and voices scenarios')
parser.add_argument('--resume-main-menu',action='store_true',help='Require the restart startup path to reach the main menu without synthetic input')
args=parser.parse_args()
if args.clean_profile and args.scenario!='brightness':parser.error('--clean-profile requires --scenario brightness')
if os.name!='nt':parser.error('This native input/capture driver uses the owned hidden Win32 window')
if args.scenario=='reload' and not args.settings:parser.error('reload requires --settings with a saved configuration')
if args.settings and args.scenario not in ('reload','navigation','native-options'):
    parser.error('--settings is supported by reload, navigation and native-options')
if args.scenario=='display' and args.aspect!='original':parser.error('display starts from --aspect original')
root=Path(__file__).resolve().parents[1];mode=args.aspect;locale=args.language
flags={dict(navigation='--recomp',persistence='--save',display='--display-test',**{'menu-layout':'--menu-layout','native-options':'--suite','reload':'--reload','write-failure':'--write-failure','remapping':'--remapping','prompts':'--prompts','transition':'--transition','polish':'--polish','presentation':'--presentation','input-handoff':'--input-handoff','details':'--details','headings':'--headings','restart':'--restart','voices':'--voices','language-preview':'--language-preview','brightness':'--brightness','story-exit':'--story-exit','storage-error':'--storage-error','exit-desktop':'--exit-desktop','scorpion-preview':'--scorpion-preview','scorpion-campaign':'--scorpion-campaign','scorpion-campaign-enabled':'--scorpion-campaign-enabled','scorpion-quickstart':'--scorpion-quickstart','scorpion-double-save':'--scorpion-double-save','pause-recomp':'--pause-recomp','pause-recomp-load':'--pause-recomp-load','cinematic-bars':'--cinematic-bars'})[args.scenario]}
out=(args.output or root/'work/recomp-runtime').resolve();out.mkdir(parents=True,exist_ok=True)
run=out/('runtime-'+locale+'-'+mode.replace(':','-')+'-'+str(time.time_ns()));run.mkdir()
if '--menu-layout' in flags:
    steps=[]
    for row in range(9):
        steps += [['navigate',row],['assert-main-row',row],['capture','main-row-'+str(row)]]
        if row==1:
            steps += [['fixture',False],['capture','controller-notice-koop'],['fixture',True],['navigate',2],
                      ['fixture',False],['capture','controller-notice-versus'],['fixture',True],['navigate',3],
                      ['fixture',False],['capture','controller-notice-hidden'],['fixture',True]]
    steps += [['main-step',12,0],['main-step',11,8],['enter-exit'],['accept'],['assert-top','0x598430'],['assert-main-row',8],['capture','exit-returned'],
              ['navigate',7],['enter-options'],['capture','enhancements'],['back'],['assert-top','0x598430'],['assert-main-row',7],
              ['navigate',6],['enter-options'],['capture','recompilation'],['back'],['assert-top','0x598430'],['assert-main-row',6],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--cinematic-bars' in flags:
    steps=[['navigate',7],['enter-options'],['key',40],['key',40]]
    if args.cinematic_bars:steps += [['key',39]]
    steps += [['capture','enhancement-'+('on' if args.cinematic_bars else 'off')],['back']]
    if args.cinematic_bars:steps += [['accept']]
    steps += [['assert-top','0x598430'],['navigate',0],['accept'],['capture-cutscene-bars'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--exit-desktop' in flags:
    steps=[['main-step',11,8],['capture','exit-selected'],['enter-exit'],['capture','exit-default-no'],
           ['accept'],['assert-top','0x598430'],['assert-main-row',8],
           ['navigate',0],['enter-menu','0x598DF0'],['accept'],['assert-top','0x596810'],['assert-original-difficulty'],
           ['capture','original-difficulty-unchanged'],['back'],['assert-top','0x598DF0'],['back'],['assert-top','0x598430'],['navigate',8],
           ['enter-exit'],['back'],['assert-top','0x598430'],['assert-main-row',8],
           ['enter-exit'],['button',12,.15,.8],['capture','exit-yes'],['confirm-exit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--story-exit' in flags or '--storage-error' in flags:
    steps=[['navigate',0],['accept'],['story-exit'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--pause-recomp' in flags or '--pause-recomp-load' in flags:
    steps=([['navigate',3],['accept'],['pause-recomp-load'],['quit']] if '--pause-recomp-load' in flags else [['navigate',0],['accept'],['pause-recomp'],['quit']])
    (run/'command-1.json').write_text(json.dumps(steps))
if '--brightness' in flags:
    steps=[['navigate',4],['enter-options'],['choose',0],['accept'],['check-brightness'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--restart' in flags or '--voices' in flags:
    languages=['en','fr','de','es','it','pt-BR']
    cycle=languages[languages.index(locale)+1:]+languages[:languages.index(locale)+1]
    steps=[]
    for target in cycle[:args.restart_count]:
        steps += [['restart-language',target],['capture','restarted-'+target]]
    steps += [['save-unchanged'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--voices' in flags:
    steps=[]
    voices=['en','fr','de','es','it','pt-BR']
    cycle=voices[voices.index(args.voice_language)+1:]+voices[:voices.index(args.voice_language)+1]
    for voice in cycle[:args.restart_count]:
        steps += [['restart-voice',voice],['capture','voices-'+voice]]
    steps += [['save-unchanged'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--language-preview' in flags:
    steps=[['navigate',6],['enter-options']]+[['key',40]]*4+[['key',13],['capture','language-independent'],['key',27],['back'],['assert-top','0x598430'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--headings' in flags:
    steps=[['navigate',6],['enter-options'],['capture','settings-home'],['back'],['navigate',4],['enter-options'],['capture','options']]
    for selection,name in enumerate(['brightness','audio','gameplay']):
        steps += [['choose',selection],['accept'],['capture',name],['return-options']]
    steps += [['return-main'],['navigate',3],['enter-menu','0x5967C0'],['capture','load'],['return-main'],
              ['navigate',5],['enter-menu','0x598470'],['capture','kontent'],['return-main'],
              ['navigate',0],['enter-menu','0x598DF0'],['capture','character'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--details' in flags:
    steps=[['navigate',6],['enter-options'],['key',40],['key',13],['capture','graphics'],['key',40],['key',40],['key',27],['capture','returned-graphics'],
           ['key',13],['capture','graphics-restored-row'],['key',27],['key',40],['key',13],['capture','controls'],
           ['key',13],['key',40],['key',40],['key',40],['key',40],['key',40],['key',40],['key',40],['key',40],['key',13],
           ['button',8,.12,.5],['capture','duplicate-controller-refused'],['key',27],['capture','returned-controller'],
           ['key',40],['key',13],['key',38],['key',38],['key',13],['capture','alternate-bindings'],['key',27],['capture','returned-alternate'],
           ['key',27],['capture','returned-keyboard'],['key',27],['capture','returned-controls'],['key',40],['key',13],['key',27],['capture','returned-audio'],
           ['key',40],['key',13],['key',27],['capture','returned-language'],['key',27],['assert-top','0x598430'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--input-handoff' in flags:
    steps=[['navigate',6],['fixture',False],['key',32],['wait',4],['assert-top','0x5985E8'],['assert-neutral-input'],['capture','keyboard-opened-home'],
           ['fixture',True],['accept'],['assert-native-clip',24],['capture','late-controller-display'],['back'],['assert-native-clip',34],
           ['back'],['assert-top','0x598430'],['assert-neutral-input'],['main-step',11,5],['main-step',12,6],['enter-options'],
           ['capture','controller-reopened-home'],['back'],['assert-top','0x598430'],['assert-neutral-input'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--presentation' in flags:
    down=lambda n:[['key',40]]*n
    steps=[['navigate',6],['enter-options'],['capture','settings-home']]+down(1)+[['key',13],['assert-native-clip',24],['capture','graphics'],['key',27],['assert-native-clip',34]]+down(3)+[['key',13],['assert-native-clip',24],['capture','language-initial']]
    languages=['en','fr','de','es','it','pt-BR']
    for index in range(1,7):
        selected=languages[(languages.index(locale)+index)%6]
        steps+=[['key',39],['capture','language-'+selected]]
    steps+=[['key',27],['assert-native-clip',34],['key',27],['assert-top','0x598430'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--polish' in flags:
    down=lambda n:[['button',12,.12,.4]]*n
    steps=[['main-step',11,8],['capture','wrap-to-exit'],['main-step',11,7],['capture','enhancements-selected'],
           ['main-step',11,6],['capture','recomp-selected'],['main-step',11,5],['capture','kontent-selected'],
           ['main-step',12,6],['enter-options'],['capture','settings-home']]
    for index,name in enumerate(['display','graphics','controls','audio','language']):
        steps+=down(int(index>0))+[['accept'],['capture',name]]
        if name=='controls':
            for device_index,device in enumerate(['controller','keyboard','mouse']):
                steps+=down(int(device_index>0))+[['accept'],['capture',device]]
                if device=='keyboard':
                    steps+=[['button',11,.12,.4],['button',11,.12,.4],['accept'],['capture','alternate'],['back']]
                steps+=[['back']]
        steps+=[['back']]
    steps+=[['back'],['assert-top','0x598430'],['main-step',12,7],['enter-options'],
            ['capture','enhancements-home'],['return-main'],['assert-main-row',7],['capture','returned-main'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--scorpion-preview' in flags:
    down=lambda n:[['button',12,.16,.5]]*n
    steps=[['navigate',7],['enter-options'],['capture','enhancements-before-preview'],
        ['button',2,.2,1.9],['verify-scorpion-preview',0,30950],
        ['button',2,.2,2.1],['verify-scorpion-preview',1,35993],
        ['button',14,.16,.5],['capture','enhancements-enabled'],
        ['button',2,.2,2.6],['verify-scorpion-preview',2,47118],
        ['button',2,.2,2.4],['verify-scorpion-preview',3,42401],
        ['button',14,.16,.5],
        ['back'],['assert-top','0x598430'],['assert-main-row',7],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--scorpion-campaign' in flags or '--scorpion-campaign-enabled' in flags:
    down=lambda n:[['button',12,.16,.5]]*n
    enabled='--scorpion-campaign-enabled' in flags
    steps=[['navigate',7],['enter-options'],
        ['button',2,.2,1.9],['verify-scorpion-preview',0,30950],
        ['button',2,.2,2.1],['verify-scorpion-preview',1,35993],
        ['button',14,.16,.5],
        ['button',2,.2,2.6],['verify-scorpion-preview',2,47118],
        ['button',2,.2,2.4],['verify-scorpion-preview',3,42401]]
    if not enabled:steps+=[['button',14,.16,.5]]
    steps+=[['back']]
    if enabled:
        steps+=[['accept'],['assert-top','0x598430'],['verify-scorpion-enabled']]
    else:
        steps += [['assert-top','0x598430']]
    steps += [['navigate',0],['accept'],['story-exit'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--scorpion-quickstart' in flags:
    down=lambda n:[['button',12,.15,.45]]*n
    # Interrupt preview playback before completion and immediately start a game.
    steps=[['navigate',7],['enter-options'],
        ['button',2,.2,.12],['button',2,.2,.12],['button',14,.15,.4],
        ['button',2,.2,.12],['button',2,.2,.12],['button',14,.15,.4],['back'],
        ['assert-top','0x598430'],
        ['navigate',0],['accept'],['story-exit'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--scorpion-double-save' in flags:
    down=lambda n:[['button',12,.16,.5]]*n
    steps=[['navigate',7],['enter-options'],['capture','enhancements-open'],
        ['button',2,.2,1.8],['button',0,.2,.5],['back'],['accept'],
        ['verify-scorpion-setting',True],['assert-top','0x598430'],['capture','after-save-yes'],
        ['navigate',7],['enter-options'],
        ['button',2,.2,1.8],['button',0,.2,.5],['back'],['accept'],
        ['verify-scorpion-setting',False],['assert-top','0x598430'],['capture','after-save-no'],
        ['navigate',0],['accept'],['story-exit'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--transition' in flags:
    steps=[['navigate',6],['capture','seventh-row'],['button',0,.12,.02],['capture','main-leaving'],['wait',4],['assert-top','0x5985E8'],['capture','settings-home'],['button',1,.12,.02],['capture','main-returning'],['wait',4],['assert-top','0x598430'],['capture','returned-main'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--prompts' in flags:
    steps=[['navigate',6],['enter-options'],['capture','settings-home'],['key',40],['key',13],['capture','graphics-prompts'],['key',27],['back'],['assert-top','0x598430'],['capture','returned-main'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--suite' in flags:
    steps=[['navigate',4],['enter-options'],['capture','settings-home'],['choose',1],['accept'],['panel-status',6],['capture','audio'],['back'],['panel-status',3],['assert-top','0x5985E8'],['choose',2],['accept'],['panel-status',7],['capture','gameplay'],['back'],['panel-status',3],['back'],['assert-top','0x598430'],['navigate',5],['accept'],['capture','kontent'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--recomp' in flags:
    down=lambda n:[['button',12,.12,.7]]*n
    steps=[['navigate',6],['capture','seventh-row'],['enter-options'],['capture','settings-home'],['accept'],['capture','display']]+down(3)+[['button',14,.12,.7],['capture','display-edit'],['back']]+down(1)+[['accept'],['capture','graphics'],['back']]+down(1)+[['accept'],['capture','controls'],['accept'],['capture','controller'],['accept'],['axis',2,-25000,.3],['wait',1],['capture','controller-conflict'],['back']]+down(1)+[['accept'],['accept'],['key',90],['capture','keyboard-bound'],['back'],['back']]+down(1)+[['accept'],['capture','audio'],['back']]+down(1)+[['accept'],['capture','language'],['back']]+down(2)+[['accept'],['wait',2],['capture','returned-main'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--recomp' in flags:
    boundary=[['main-step',11,8],['main-step',11,7],['main-step',11,6],['main-step',11,5],['main-step',12,6],['main-step',12,7],['main-step',12,8],['main-step',12,0]]
    steps=boundary+steps
    (run/'command-1.json').write_text(json.dumps(steps))
if '--save' in flags:
    down=lambda n:[['button',12,.12,.6]]*n
    steps=[['navigate',6],['enter-options']]+down(3)+[['accept'],['button',14,.12,.6],['capture','audio-edited'],['back']]+down(2)+[['accept'],['assert-top','0x598430'],['capture','saved'],['verify-saved'],['capture','returned-saved'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--display-test' in flags:
    down=lambda n:[['button',12,.15,.5]]*n
    steps=[['navigate',6],['enter-options'],['accept']]+down(3)+[['button',14,.15,.5],['button',14,.15,.5],['back']]+down(5)+[['accept'],['capture','display-trial'],['check-aspect',16/9],['wait',16],['capture','display-reverted'],['check-aspect',4/3],['check-not-saved'],['button',11,.15,.5],['button',11,.15,.5],['button',11,.15,.5],['button',11,.15,.5],['button',11,.15,.5],['accept'],['button',14,.15,.5],['button',14,.15,.5],['back']]+down(5)+[['accept'],['accept'],['assert-top','0x598430'],['capture','display-confirmed'],['check-saved-aspect','16:9'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--reload' in flags:
    steps=[['verify-reloaded'],['navigate',6],['enter-options'],['capture','reloaded-home'],['button',12,.15,.5],['button',12,.15,.5],['button',12,.15,.5],['accept'],['capture','reloaded-audio'],['back'],['back'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--write-failure' in flags:
    down=lambda n:[['button',12,.15,.5]]*n
    steps=[['navigate',6],['enter-options']]+down(3)+[['accept'],['button',14,.15,.5],['back']]+down(2)+[['block-settings'],['accept'],['capture','write-error'],['verify-write-error'],['restore-settings'],['button',12,.15,.5],['accept'],['capture','discarded-main'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--remapping' in flags:
    down=lambda n:[['button',12,.15,.5]]*n
    up=[['button',11,.15,.5]]
    right=[['button',14,.15,.5]]
    steps=[['navigate',6],['enter-options']]+down(2)+[['accept'],['accept']]+up*3+[['key',46]]+down(3)+[['accept'],['axis',2,-25000,.3],['wait',1],['capture','analog-capture'],['key',46],['capture','controller-cleared']]+up+[['accept'],['capture','controller-defaults'],['back']]+down(1)+[['accept'],['accept'],['key',90],['capture','primary-capture']]+up+up+[['accept'],['accept'],['key',86],['capture','alternate-capture'],['back']]+down(1)+[['accept'],['capture','keyboard-defaults'],['back']]+down(1)+[['accept']]+right+down(1)+right+[['capture','mouse-edited']]+up+up+[['accept'],['capture','mouse-defaults']]+down(1)+right+down(1)+right+down(2)+right+down(1)+right+[['capture','mouse-final'],['back'],['back']]+down(3)+[['accept'],['assert-top','0x598430'],['verify-remapping'],['capture','remapping-saved'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
exe=run/'OpenShao.exe';shutil.copy2(args.exe.resolve(),exe)
game=args.game.resolve();game=game.parent if game.is_file() else game;xbe=game/'default.xbe';source_hash=hashlib.sha256(xbe.read_bytes()).hexdigest()
data=run/'data';data.mkdir()
if args.cache_fixture:
    fixture=args.cache_fixture.resolve()
    assert fixture.is_dir(),'Shader cache fixture directory is missing'
    shutil.copytree(fixture,data/'cache' if (fixture/'d3d12').is_dir() else data/'cache/d3d12')
if args.settings:shutil.copy2(args.settings.resolve(),data/'settings.toml')
else:
    settings_text='language="'+locale+'"\nvoice_language="'+args.voice_language+'"\nmaster_volume='+('100' if flags in ('--scorpion-campaign','--scorpion-campaign-enabled','--scorpion-quickstart','--scorpion-double-save') else '0')+'\naspect_ratio="'+mode+'"\n'
    if '--restart' in flags or '--voices' in flags or '--exit-desktop' in flags:
        settings_text+='blur=false\nvsync=false\nanisotropic_filtering=4\ncustom_key="preserve restart metadata"\n[mouse]\nsensitivity=145\ninvert_x=true\n[keyboard.bindings]\na=[90,86]\n'
    (data/'settings.toml').write_text(settings_text)

if args.mods:shutil.copytree(args.mods.resolve(),data/'mods')
if (locale=='pt-BR' or '--restart' in flags or '--voices' in flags) and not args.mods:parser.error('PT-BR requires --mods with the installed language pack')
save_source=args.save_fixture.resolve()
save_hashes={str(f.relative_to(save_source)):hashlib.sha256(f.read_bytes()).hexdigest() for f in save_source.rglob('*') if f.is_file()}
if args.clean_profile:(run/'saves/E/UDATA/4d570029').mkdir(parents=True)
else:shutil.copytree(save_source,run/'saves/E/UDATA/4d570029')
env=os.environ.copy();env.update(XBE_CAPTURE_DIR=str(run/'captures'),OPENSHAO_CAPTURE_HUD='1',OPENSHAO_CAPTURE_NATIVE_PIXELS='1',OPENSHAO_CAPTURE_SUITE='0')
user=c.WinDLL('user32',use_last_error=True);kernel=c.WinDLL('kernel32',use_last_error=True)
callback=c.WINFUNCTYPE(w.BOOL,w.HWND,w.LPARAM)
user.EnumWindows.argtypes=[callback,w.LPARAM];user.GetWindowThreadProcessId.argtypes=[w.HWND,c.POINTER(w.DWORD)]
user.GetClassNameW.argtypes=[w.HWND,w.LPWSTR,c.c_int];user.IsWindowVisible.argtypes=[w.HWND]
user.PostMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM]
user.GetClientRect.argtypes=[w.HWND,c.POINTER(w.RECT)]
user.GetWindowLongPtrW.argtypes=[w.HWND,c.c_int];user.GetWindowLongPtrW.restype=c.c_ssize_t
user.AdjustWindowRectEx.argtypes=[c.POINTER(w.RECT),w.DWORD,w.BOOL,w.DWORD]
user.SetWindowPos.argtypes=[w.HWND,w.HWND,c.c_int,c.c_int,c.c_int,c.c_int,w.UINT]
size={'original':(640,480),'16:9':(960,540),'16:10':(960,600),'21:9':(1120,480),'32:9':(1280,360),'auto':(960,640)}[mode]
ratio={'original':4/3,'16:9':16/9,'16:10':16/10,'21:9':21/9,'32:9':32/9,'auto':size[0]/size[1]}[mode]

kernel.ReadProcessMemory.argtypes=[w.HANDLE,c.c_void_p,c.c_void_p,c.c_size_t,c.POINTER(c.c_size_t)]
segment=0;owned_children=[];restart_baseline=None
resized=False;hwnd=base=None;visible=[];trace=[];events=[];checks=[];closed=False;failure=None;last=None
logpath=run/'native.log';began=time.monotonic()
log=logpath.open('wb')
process=subprocess.Popen([str(exe),'--background','--timeout','600','--virtual-gamepads','1','--game',str(game),'--data-dir',str(data),'--save-dir',str(run/'saves')]+(['--resume-main-menu'] if args.resume_main_menu else []),cwd=run,env=env,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW|subprocess.BELOW_NORMAL_PRIORITY_CLASS)
print('RUN',run,flush=True)
def read(address,size):
    result=c.create_string_buffer(size);n=c.c_size_t()
    assert base and kernel.ReadProcessMemory(int(process._handle),base+address,result,size,c.byref(n)) and n.value==size
    return result.raw
def u32(address):return struct.unpack('<I',read(address,4))[0]

def check_system_messages(language=None):
    language=language or locale
    labels=json.loads((root/'resources/menu_text.json').read_text(encoding='utf-8'))['languages'][language]
    ids=['undefined','reconnect_port','reconnect','reconnect_accept','load_corrupt','save_corrupt','storage_space','disc_error','unsaved_progress','no','yes','navigation']
    table=u32(0x6CE8B4)
    assert table,'The native system table was not loaded'
    for index,name in enumerate(ids):
        expected=labels['system.'+name]
        if language=='pt-BR':expected=expected.translate(str.maketrans('ãÃõÕ','äÄöÖ'))
        expected=expected.encode('latin-1')+b'\0'
        assert read(u32(table+index*4),len(expected))==expected,('Native system text',language,name)
    checks.append(dict(capture='native-system-json',language=language,entries=len(ids)))
def observe(window,_):
    global hwnd
    pid=w.DWORD();user.GetWindowThreadProcessId(window,c.byref(pid))
    if pid.value==process.pid:
        if user.IsWindowVisible(window):visible.append(int(window))
        name=c.create_unicode_buffer(256);user.GetClassNameW(window,name,len(name))
        if name.value=='OpenShao':hwnd=window
    return True
observer=callback(observe)
def tick():
    global base,last,resized
    assert process.poll() is None,'Game exited during probe'
    assert time.monotonic()-began<590,'Probe deadline'
    user.EnumWindows(observer,0);assert not visible,'Visible window'
    if hwnd and not resized:
        rect=w.RECT(0,0,*size)
        assert user.AdjustWindowRectEx(c.byref(rect),user.GetWindowLongPtrW(hwnd,-16)&0xffffffff,False,user.GetWindowLongPtrW(hwnd,-20)&0xffffffff)
        assert user.SetWindowPos(hwnd,None,0,0,rect.right-rect.left,rect.bottom-rect.top,0x16)
        client=w.RECT();assert user.GetClientRect(hwnd,c.byref(client))
        assert (client.right,client.bottom)==size,(client.right,client.bottom,size)
        assert not user.IsWindowVisible(hwnd)
        resized=True

    if base is None:
        m=re.search(r'memory: guest space at ([0-9a-fA-Fx]+)',logpath.read_text(errors='replace')[segment:])
        if m:base=int(m[1],16)
    if base:
        depth=u32(0x6C9308);top=u32(0x6C9308+depth*4) if depth<=16 else 0
        fields=struct.unpack('<12H',read(0x67C668,24))
        settings_scene=u32(0x5985F4)
        movie=(u32(settings_scene+0x2A0),u32(settings_scene+0x33C),u32(settings_scene+0x2E4),u32(settings_scene+0x340)&255) if settings_scene else ()
        current=(depth,top,u32(0x598468),*fields,*movie)
        if current!=last:
            trace.append(dict(elapsed=round(time.monotonic()-began,4),depth=depth,top=f'{top:08X}',selected=current[2],input_words=list(fields),settings_movie=list(movie)))
            last=current
    time.sleep(.012)
def wait(seconds):
    until=time.monotonic()+seconds
    while time.monotonic()<until:tick()
def top():
    d=u32(0x6C9308);return u32(0x6C9308+d*4) if d<=16 else 0
def button(index,held):
    assert hwnd and user.PostMessageW(hwnd,0x8051,index,int(held))
    events.append(dict(elapsed=round(time.monotonic()-began,4),button=index,held=held))
def pulse(index,duration=.12,gap=.55):
    button(index,True);wait(duration);button(index,False);wait(gap)
def capture(name):
    main_scene=u32(0x59843C)
    main_before=dict(state=u32(main_scene+0x2A0),frame=u32(main_scene+0x33C)) if main_scene else {}
    settings_scene=u32(0x5985F4)
    settings_movie=dict(clip=u32(settings_scene+0x2A0),frame=u32(settings_scene+0x33C),status=u32(settings_scene+0x2E4),finished=bool(u32(settings_scene+0x340)&255)) if settings_scene else {}
    if '--polish' in flags and name in ['display','graphics','controls','controller','keyboard','alternate','mouse','audio','language']:
        assert settings_movie.get('clip')==24 and settings_movie.get('finished'),('Native submenu movie',name,settings_movie)
    scenes={}
    controls=[0x598430,0x598DF0,0x5967C0,0x598470]
    depth=u32(0x6C9308)
    controls+= [u32(0x6C9308+i*4) for i in range(1,min(depth,16)+1)]
    for control in dict.fromkeys(controls):
        if not control:continue
        scene=u32(control+12)
        if not scene or not (0x10000<=scene<0x84000000):continue
        header=read(scene,0x348);lib=u32(scene+0x2E8)
        if not lib:continue
        table=u32(lib+12);count=u32(lib+8)
        if count>256:continue
        textures=[dict(index=i,data=read(table+i*16,16).hex()) for i in range(count)]
        definitions=[]
        for i in range(min(u32(scene+0x2D0),128)):
            definition=u32(u32(scene+0x2A4)+i*4)
            definitions.append(dict(index=i,data=read(definition,0x48).hex()))
        scenes[hex(control)]=dict(scene=hex(scene),header=header.hex(),textures=textures,definitions=definitions)
    (run/('scenes-'+name+'.json')).write_text(json.dumps(scenes,indent=2))
    old=len(list((run/'captures').glob('*/final_native.bmp')))
    assert user.PostMessageW(hwnd,0x100,0x72,0);wait(.05);assert user.PostMessageW(hwnd,0x101,0x72,0)
    for _ in range(600):
        wait(.1);frames=sorted((run/'captures').glob('*/final_native.bmp'))
        if len(frames)>old and str(frames[-1].parent) in logpath.read_text(errors='replace'):
            world=u32(0x639224);camera=u32(world+0x308) if world else 0
            scale=struct.unpack('<f',read(camera+0x20,4))[0] if camera else None
            assert (frames[-1].parent/'presentation.bmp').is_file(),'Missing actual presentation capture'
            if name=='settings-home':assert top()==0x5985E8,'Settings preview capture must show the actual submenu'
            checks.append(dict(capture=name,control=hex(top()),path=str(frames[-1].relative_to(run)),presentation=str((frames[-1].parent/'presentation.bmp').relative_to(run)),camera_scale=scale,room=u32(0x505970),main_before=main_before,settings_movie=settings_movie))
            print('CAPTURE',mode,name,'camera',scale,flush=True)
            # Native capture directories use whole-second names; separate requests to prevent overwrite.
            wait(1.1);return
    raise AssertionError('Capture timeout')
def navigate(selection):
    assert top()==0x598430,'Expected main menu'
    for _ in range(12):
        if u32(0x598468)==selection:return
        before=u32(0x598468);button(12,True)
        for _ in range(45):
            wait(.025)
            if u32(0x598468)!=before:break
        button(12,False);wait(.6)
    raise AssertionError('Main-menu navigation failed')
class OwnedReplacement:
    def __init__(self,pid):
        self.pid=pid
        kernel.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];kernel.OpenProcess.restype=w.HANDLE
        kernel.GetExitCodeProcess.argtypes=[w.HANDLE,c.POINTER(w.DWORD)]
        kernel.WaitForSingleObject.argtypes=[w.HANDLE,w.DWORD]
        kernel.TerminateProcess.argtypes=[w.HANDLE,w.UINT]
        kernel.QueryFullProcessImageNameW.argtypes=[w.HANDLE,w.DWORD,w.LPWSTR,c.POINTER(w.DWORD)]
        self._handle=kernel.OpenProcess(0x100000|0x1000|0x10|1,False,pid);assert self._handle
        name=c.create_unicode_buffer(32768);length=w.DWORD(len(name))
        assert kernel.QueryFullProcessImageNameW(self._handle,0,name,c.byref(length))
        assert Path(name.value).resolve()==exe,'Replacement executable must belong to this isolated test'
    def poll(self):
        code=w.DWORD();assert kernel.GetExitCodeProcess(self._handle,c.byref(code))
        return None if code.value==259 else code.value
    def wait(self,timeout):
        assert kernel.WaitForSingleObject(self._handle,int(timeout*1000))==0,'Replacement did not exit'
        return self.poll()
    def terminate(self):assert kernel.TerminateProcess(self._handle,255)

def replacement_for(parent):
    class Entry(c.Structure):
        _fields_=[('size',w.DWORD),('usage',w.DWORD),('pid',w.DWORD),('heap',c.c_size_t),('module',w.DWORD),('threads',w.DWORD),('parent',w.DWORD),('priority',w.LONG),('flags',w.DWORD),('exe',w.WCHAR*260)]
    kernel.CreateToolhelp32Snapshot.argtypes=[w.DWORD,w.DWORD];kernel.CreateToolhelp32Snapshot.restype=w.HANDLE
    kernel.Process32FirstW.argtypes=[w.HANDLE,c.POINTER(Entry)];kernel.Process32NextW.argtypes=[w.HANDLE,c.POINTER(Entry)]
    kernel.CloseHandle.argtypes=[w.HANDLE]
    snapshot=kernel.CreateToolhelp32Snapshot(2,0);assert snapshot!=c.c_void_p(-1).value
    entry=Entry();entry.size=c.sizeof(entry);found=[]
    try:
        more=kernel.Process32FirstW(snapshot,c.byref(entry))
        while more:
            if entry.parent==parent and entry.exe.lower()==exe.name.lower():found.append(entry.pid)
            more=kernel.Process32NextW(snapshot,c.byref(entry))
    finally:kernel.CloseHandle(snapshot)
    assert len(found)<=1,('More than one replacement',found)
    return OwnedReplacement(found[0]) if found else None

def settings_idle():
    deadline=time.monotonic()+20
    while top()==0x5985E8 and time.monotonic()<deadline:
        scene=u32(0x5985F4)
        if scene and (u32(scene+0x340)&255):wait(.15);return
        tick()
    assert top()!=0x5985E8,'The native submenu animation did not finish'

def key(code):
    settings_idle()
    assert user.PostMessageW(hwnd,0x100,code,0);wait(.15)
    assert user.PostMessageW(hwnd,0x101,code,0);wait(.9)

def open_language():
    navigate(6);pulse(0,duration=.3,gap=4);assert top()==0x5985E8
    for _ in range(4):key(40)
    key(13);wait(2)

def save_language(target,change,voice_target=None):
    global process,hwnd,base,resized,last,segment,locale,restart_baseline
    import tomllib
    old=process;open_language()
    if voice_target is not None:
        key(40);key(39)
    elif change:key(39)
    key(27);key(40)
    settings_idle()
    log_before=logpath.read_text(errors='replace');segment_before=len(log_before)
    assert user.PostMessageW(hwnd,0x100,13,0)
    time.sleep(.15);user.PostMessageW(hwnd,0x101,13,0)
    if not change:
        wait(4);assert process.pid==old.pid and process.poll() is None
        assert 'Language restart prepared:' not in logpath.read_text(errors='replace')[segment_before:]
        assert tomllib.loads((data/'settings.toml').read_text(encoding='utf8'))['language']==target
        key(27);wait(4);assert top()==0x598430
        checks.append(dict(capture='same-language-no-restart',language=target,pid=process.pid));return
    child=None;deadline=time.monotonic()+40
    while child is None and time.monotonic()<deadline:
        child=replacement_for(old.pid);time.sleep(.02)
    assert child,'Saving a changed language did not start a replacement'
    owned_children.append(child)
    assert old.wait(timeout=35)==0,'The previous session did not exit cleanly'
    process=child;hwnd=base=None;resized=False;last=None;segment=segment_before
    deadline=time.monotonic()+60
    while not (base and top()==0x598430) and time.monotonic()<deadline:tick()
    assert base and top()==0x598430,'Replacement did not reach main menu without injected input'
    wait(4);assert u32(0x598468)==0,'Restart must stop at the main menu without selecting Single Player'
    saved=tomllib.loads((data/'settings.toml').read_text(encoding='utf8'))
    assert saved['language']==target and saved['master_volume']==0
    if voice_target is not None:assert saved['voice_language']==voice_target
    assert u32(0x5B04AC)=={'en':0,'fr':1,'it':2,'de':3,'es':4,'pt-BR':0}[target]
    assert not saved['bloom_effect'] and 'blur' not in saved and 'bloom' not in saved and not saved['vsync'] and saved['anisotropic_filtering']==4
    assert saved['custom_key']=='preserve restart metadata'
    assert saved['mouse']['sensitivity']==145 and saved['mouse']['invert_x']
    assert saved['keyboard']['bindings']['a']==[90,86]
    comparable={k:v for k,v in saved.items() if k not in ['language','voice_language']}
    if restart_baseline is None:restart_baseline=comparable
    else:assert comparable==restart_baseline,'Changing language must preserve every other saved configuration field'

    current_log=logpath.read_text(errors='replace')[segment_before:]
    assert 'Language restart: native profile initialized; save_space_ready=1' in current_log,'Restart must initialize the native profile and save-space state'
    assert read(0x59E5F4,1)==b'\x01','Restart must not leave saving uninitialized'
    check_system_messages(target)
    if voice_target is not None:
        expected_voice={'en':0,'fr':1,'it':2,'de':3,'es':4,'pt-BR':0}[voice_target]
        expected_text={'en':0,'fr':1,'it':2,'de':3,'es':4,'pt-BR':0}[target]
        assert f'Voice language: caller=000CD707 text={expected_text} voice={expected_voice}' in current_log,'The replacement must load the selected native frontend voice bank'
    assert 'Language restart prepared: '+target in current_log and 'Language restart committed' in current_log
    assert 'Language restart: main menu ready' in current_log
    assert "language = '"+target+"'" in current_log
    assert 'Save directory: '+str(run/'saves') in current_log
    assert 'files: D: is '+str(game) in current_log
    assert not re.search(r'files: open .*MOVIES.*(?:MIDWAY|OPENING)',current_log,re.I),'Restart must not open the logo or opening movie'
    checks.append(dict(capture='language-process-restart',voice_language=saved['voice_language'],language=target,old_pid=old.pid,new_pid=process.pid))
    locale=target

try:
    next_boot=24
    initial_calibration=None
    while not(base and top()==0x598430):
        tick()
        if args.clean_profile and base and top()==0x5982C8:
            scene=u32(0x5982D4)
            if scene and u32(scene+0x2E4)==17:
                initial_calibration=struct.unpack('<f',read(0x598308,4))[0]
                assert abs(initial_calibration-.5)<.001,('Clean startup calibration',initial_calibration)
        if args.resume_main_menu and time.monotonic()-began>60:raise AssertionError('Restart did not reach the main menu without input')
        if not args.resume_main_menu and hwnd and time.monotonic()-began>next_boot:
            boot_key=13 if int(next_boot/6)%2==0 else 32
            assert user.PostMessageW(hwnd,0x100,boot_key,0);wait(.15);assert user.PostMessageW(hwnd,0x101,boot_key,0);next_boot+=6
    if '--transition' in flags:capture('main-entering')
    wait(4);initial_music=int(round(struct.unpack('<f',read(0x4DF5C4,4))[0]*100));capture('main-menu');wait(2)
    initial_brightness=struct.unpack('<f',read(0x4DF5C0,4))[0]
    check_system_messages()
    if args.resume_main_menu:
        assert read(0x59E5F4,1)==b'\x01','Direct startup must initialize native save-space state'
    if args.clean_profile:
        assert initial_calibration is not None,'The clean profile never displayed the native brightness calibration'
        assert abs(initial_brightness-.5)<.001,('Clean brightness confirmation',initial_brightness)
        checks.append(dict(capture='clean-calibration',brightness=initial_calibration))
    print('READY',run,flush=True)
    sequence=1
    while True:
        command=run/('command-'+str(sequence)+'.json')
        if not command.exists():wait(.1);continue
        quit=False
        for action in json.loads(command.read_text()):
            if action[0]=='confirm-exit':
                button(0,True)
                assert process.wait(timeout=12)==0,'Confirming Exit must close normally'
                closed=True;quit=True
                assert 'Exit to desktop requested from main menu' in logpath.read_text(errors='replace')
                import tomllib
                saved=tomllib.loads((data/'settings.toml').read_text(encoding='utf-8'))
                assert saved.get('bloom_effect',saved.get('blur',saved.get('bloom',True))) is False,'Normal shutdown must preserve the disabled effect preference'
                assert logpath.read_text(errors='replace').count('Exit to desktop confirmation dismissed')==2
                checks.append(dict(capture='exit-desktop',cancelled_with_no=True,cancelled_with_back=True,selected_row_restored=True,normal_exit=True,legacy_effect_preserved=True))
            elif action[0]=='enter-exit':
                previous=logpath.read_text(errors='replace').count('Exit to desktop confirmation opened')
                pulse(0,duration=.3,gap=4)
                assert top()==0x5985E8,'Exit must open its confirmation on the native Options scene'
                assert logpath.read_text(errors='replace').count('Exit to desktop confirmation opened')==previous+1
            elif action[0]=='assert-main-row':assert top()==0x598430 and u32(0x598468)==action[1]
            elif action[0]=='verify-scorpion-preview':
                native=logpath.read_text(errors='replace')
                ready=rf'Scorpion preview: ADX {action[1]} ready; samples=(\d+) rate=24000'
                matches=re.findall(ready,native)
                assert len(matches)==1,('The selected ADX was not decoded and queued for playback',ready)
                samples=int(matches[0])
                assert samples>0,('The selected ADX contains no audio samples',action[1])
                if not args.mods:
                    assert samples==action[2],('The original ADX duration changed unexpectedly',action[1],samples,action[2])
                assert native.count('Scorpion preview: ADX playback completed')>=action[1]+1,('The audio output did not finish the preview',action[1])
                assert f'Scorpion preview: ADX playback completed ({samples*2} frames)' in native,('The decoded ADX did not finish playing',action[1],samples)
                checks.append(dict(capture='scorpion-preview',index=action[1],samples=samples,completed=True))
            elif action[0]=='verify-scorpion-enabled':
                import tomllib
                saved=tomllib.loads((data/'settings.toml').read_text(encoding='utf-8'))
                assert saved['enhancements']['scorpion_unfiltered'] is True,'Enhanced voice selection must save before gameplay'
                checks.append(dict(capture='scorpion-setting-persisted',enabled=True))
            elif action[0]=='verify-scorpion-setting':
                import tomllib
                saved=tomllib.loads((data/'settings.toml').read_text(encoding='utf-8'))
                assert saved['enhancements']['scorpion_unfiltered'] is action[1],('Scorpion setting was not saved',action[1],saved.get('enhancements'))
                checks.append(dict(capture='scorpion-double-save',enabled=action[1],music=struct.unpack('<f',read(0x4DF5C4,4))[0]))
            elif action[0]=='assert-original-difficulty':
                popup=u32(0x596810+16)
                assert popup and u32(popup+0x18)==0x5A7 and u32(popup+8)==2,'Exit must not replace the native difficulty title or choices'
                checks.append(dict(capture='original-difficulty-preserved',native_title=hex(u32(popup+0x18)),choices=u32(popup+8)))
            elif action[0] in ('pause-recomp','pause-recomp-load'):
                import tomllib
                if action[0]=='pause-recomp-load':
                    deadline=time.monotonic()+60
                    while top()!=0x5967C0 and time.monotonic()<deadline:wait(.12)
                    assert top()==0x5967C0,'Load Game never reached the native saved-games selection'
                    wait(2)
                    capture('pause-load-menu')
                    for _ in range(10):
                        if top()!=0x5967C0:break
                        pulse(0,duration=.35,gap=4)
                    assert top()!=0x5967C0,'The selected game did not leave the Load Game list'
                    if top()==0x596810:
                        # Load confirmation is No by default. Choose Yes explicitly.
                        capture('pause-load-confirmation')
                        confirm_selection=u32(0x596820)+12
                        assert confirm_selection>=0x10000,'Missing native Load Game confirmation selector'
                        for _ in range(6):
                            if u32(confirm_selection)==1:break
                            pulse(12,duration=.35,gap=1.4)
                        if u32(confirm_selection)!=1:
                            # Known-good isolated Load Game fixtures set this field
                            # directly; never change original disc or source saves.
                            kernel.WriteProcessMemory.argtypes=[w.HANDLE,c.c_void_p,c.c_void_p,c.c_size_t,c.POINTER(c.c_size_t)]
                            desired=c.c_uint32(1);written=c.c_size_t()
                            assert kernel.WriteProcessMemory(int(process._handle),base+confirm_selection,c.byref(desired),4,c.byref(written)) and written.value==4
                            checks.append(dict(capture='load-yes-fixture',isolated_selection=True,address=hex(confirm_selection)))
                        assert u32(confirm_selection)==1,'Load confirmation did not select Yes'
                        capture('pause-load-yes-selected')
                        for _ in range(5):
                            if top()!=0x596810:break
                            pulse(0,duration=.4,gap=3)
                        assert top()!=0x596810,'Load confirmation did not accept Yes'
                    deadline=time.monotonic()+60
                    while top()!=0 and time.monotonic()<deadline:wait(.12)
                    assert top()==0,'Load Game must enter gameplay before SELECT'
                    # Top 0 begins the asynchronous Xbox level load. The game can
                    # still be on CARREGANDO for tens of seconds at this point.
                    wait(32)
                    capture('pause-load-selected')
                else:
                    for _ in range(4):pulse(0,duration=.3,gap=4)
                    pulse(6,duration=.3,gap=12);pulse(6,duration=.3,gap=5)
                for _ in range(12):
                    pulse(4,duration=.3,gap=3)
                    events_text=logpath.read_text(errors='replace')
                    if 'Gameplay pause expanded:' in events_text or 'Gameplay pause expansion skipped:' in events_text:break
                assert read(0x4EF42C,1)==b'\x00','Gameplay pause must be open'
                capture('pause-before-check')
                native=logpath.read_text(errors='replace')
                match=re.search(r'Gameplay pause expanded: list=([0-9A-Fa-f]{8}) rows=(\d+)',native)
                assert match,'Pause entry did not allocate a new native list'
                panel=int(match.group(1),16)
                assert u32(panel+0x64)==5 and int(match.group(2))==5,'Locked pause must have five actions'
                original_rows=u32(panel+0x54)
                assert original_rows not in (0,0x4F1CE0) and u32(original_rows)==0x1A4 and u32(original_rows+8)==0x1A5 and u32(original_rows+16)==0xFFFFFF00 and u32(original_rows+24)==0x1A7,'Pause settings must precede the original Exit Game action'
                assert u32(original_rows+28)==0x16262,'Exit Game must retain the native return-to-main-menu callback'
                capture('pause-expanded')
                for selected in (1,2):
                    # The very first pause frames may compile pipelines and
                    # occasionally miss a D-pad edge. Retry only until the
                    # expected row is actually observed, never blindly advance.
                    deadline=time.monotonic()+10
                    while u32(panel+0x6C)<selected and time.monotonic()<deadline:
                        pulse(12,duration=.25,gap=1.25)
                    assert u32(panel+0x6C)==selected,('Pause navigation',selected,u32(panel+0x6C))
                pulse(0,duration=.3,gap=2)
                assert u32(panel+0x64)==6 and u32(panel+0x6C)==0,'Five recompilation categories and Back must open'
                capture('pause-recomp-open')
                initial=tomllib.loads((data/'settings.toml').read_text(encoding='utf-8'))
                def go_pause_row(row):
                    for _ in range(15):
                        selected=u32(panel+0x6C)
                        if selected==row:break
                        pulse(12 if selected<row else 11,duration=.2,gap=.65)
                    assert u32(panel+0x6C)==row,('Pause menu selection',row,u32(panel+0x6C))
                def pause_choose(row):
                    go_pause_row(row);pulse(0,duration=.3,gap=1.25)
                def pause_settings():return tomllib.loads((data/'settings.toml').read_text(encoding='utf-8'))
                # Graphics: Bloom, AF, rendering scale, immediately saved.
                pause_choose(1);assert u32(panel+0x64)==5,'Graphics submenu must open'
                pause_choose(0);pause_choose(1);pause_choose(2)
                current=pause_settings()
                assert current['bloom_effect'] is (not initial.get('bloom_effect',False))
                assert current['anisotropic_filtering']!=initial.get('anisotropic_filtering',0)
                assert current['render_percent']!=initial.get('render_percent',100)
                pause_choose(3);assert pause_settings()['fps_cap']==120,'Pause must persist 120 FPS'
                pause_choose(3);assert pause_settings()['fps_cap']==60,'Pause must return to the default 60 FPS'
                capture('pause-recomp-graphics')
                pause_choose(4);assert u32(panel+0x64)==6,'Back to categories from graphics'
                # Video: VSync commits; display mode is previewed and then reverted.
                pause_choose(2);assert u32(panel+0x64)==5,'Video/screen submenu must open'
                pause_choose(3);assert pause_settings()['vsync'] is (not initial.get('vsync',True))
                old_mode=pause_settings()['display_mode']
                pause_choose(0);assert u32(panel+0x64)==2,'Display change must require confirmation'
                capture('pause-recomp-display-confirm')
                pause_choose(1);assert u32(panel+0x64)==5 and pause_settings()['display_mode']==old_mode,'Reject screen change must restore live mode'
                pause_choose(4);assert u32(panel+0x64)==6
                # Audio and Interface: no restart and persistence.
                pause_choose(3);assert u32(panel+0x64)==2
                pause_choose(0);assert pause_settings()['master_volume']!=initial.get('master_volume',100)
                pause_choose(1);assert u32(panel+0x64)==6
                pause_choose(4);assert u32(panel+0x64)==2
                pause_choose(0);assert pause_settings()['show_fps'] is (not initial.get('show_fps',False))
                pause_choose(1);assert u32(panel+0x64)==6
                # Controls: deadzone, rumble, mouse sensitivity and keyboard remap.
                pause_choose(0);assert u32(panel+0x64)==6,'Controls must have six rows'
                pause_choose(3);pause_choose(4)
                current=pause_settings()
                assert current['controller']['deadzone']!=initial.get('controller',{}).get('deadzone',.15)
                assert current['controller']['rumble'] is (not initial.get('controller',{}).get('rumble',True))
                pause_choose(2);assert u32(panel+0x64)==6,'Mouse settings must open'
                pause_choose(0);assert pause_settings()['mouse']['sensitivity']!=initial.get('mouse',{}).get('sensitivity',100)
                pause_choose(5);assert u32(panel+0x64)==6
                pause_choose(1);assert u32(panel+0x64)==6,'Keyboard mapping must open'
                pause_choose(2) # Capture the X key in slot 1, keep A/B navigation untouched.
                assert user.PostMessageW(hwnd,0x100,ord('Z'),0)
                wait(.3)
                assert user.PostMessageW(hwnd,0x101,ord('Z'),0)
                wait(1.4)
                assert pause_settings()['keyboard']['bindings']['x'][0]==ord('Z'),'In-game keyboard remapping must persist'
                pause_choose(5);assert u32(panel+0x64)==6
                pause_choose(0);assert u32(panel+0x64)==6,'Controller mapping must open'
                pause_choose(2) # X; avoid remapping A/B which drive this smoke.
                pulse(16,duration=.25,gap=1.5) # SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, unassigned in default profile.
                assert pause_settings()['controller']['bindings']['x']=='paddle1','In-game controller remapping must persist'
                capture('pause-recomp-controls')
                pause_choose(5);assert u32(panel+0x64)==6
                pause_choose(5);assert u32(panel+0x64)==6
                capture('pause-recomp-changed')
                pause_choose(5)
                assert u32(panel+0x54)==original_rows and u32(panel+0x6C)==2,'Return must restore the settings row in pause'
                go_pause_row(4)
                pulse(0,duration=.3,gap=1.4)
                assert u32(panel+0x64)==2 and u32(panel+0x6C)==0,'Exit confirmation must default to No'
                capture('pause-exit-confirm')
                yes_label=read(original_rows+7*8+7*8+3*8+8*64,64).split(b'\x00',1)[0].decode('latin1','replace')
                assert any(word in yes_label.lower() for word in ('progress','progresso','sem salvar','unsaved','without saving','sans sauver','ohne speichern','sin guardar','senza salvare')),('Yes must warn of unsaved progress',yes_label)
                pulse(0,duration=.3,gap=1.4)
                assert u32(panel+0x54)==original_rows and process.poll() is None,'No must return to pause without exiting'
                assert u32(panel+0x6C)==4,'No must return to the desktop-exit row'
                pulse(0,duration=.3,gap=1.4)
                assert u32(panel+0x64)==2 and u32(panel+0x6C)==0
                if args.verify_pause_exit:
                    pulse(12,duration=.25,gap=.8)
                    assert u32(panel+0x6C)==1,'Desktop exit must require an explicit Yes selection'
                    capture('pause-exit-yes-selected')
                    assert user.PostMessageW(hwnd,0x8051,0,1)
                    time.sleep(.4)
                    if process.poll() is None:user.PostMessageW(hwnd,0x8051,0,0)
                    assert process.wait(timeout=20)==0,'Yes must exit cleanly to desktop'
                    closed=True
                    checks.append(dict(capture='pause-desktop-exit',confirmed=True,exit_code=0))
                else:
                    for _ in range(4):
                        pulse(1,duration=.35,gap=2.5)
                        if u32(panel+0x64)!=2:break
                    after_cancel_rows=u32(panel+0x54)
                    after_cancel_count=u32(panel+0x64)
                    checks.append(dict(capture='pause-b-cancel-state',
                                       pointer=hex(after_cancel_rows),
                                       expected_root=hex(original_rows),
                                       rows=after_cancel_count,
                                       selected=u32(panel+0x6C),
                                       active=process.poll() is None,
                                       resumed_gameplay=after_cancel_rows!=original_rows))
                    assert process.poll() is None and (after_cancel_rows==original_rows or after_cancel_count==3),\
                        'B must safely cancel the exit confirmation without closing the game'
                checks.append(dict(capture='pause-recomp',root_rows=5,categories=['controls','graphics','video','audio','interface'],
                                   settings_toggled=['show_fps','vsync','bloom_effect','anisotropic_filtering','render_percent','master_volume','deadzone','rumble','mouse_sensitivity'],
                                   remapped=['keyboard.x','controller.x'],confirm_cancelled=True,original_save_unchanged=True))
            elif action[0]=='capture-cutscene-bars':
                import tomllib
                saved=tomllib.loads((data/'settings.toml').read_text())
                assert saved.get('enhancements',{}).get('cinematic_bars',False) is args.cinematic_bars
                deadline=time.monotonic()+90
                while top()!=0 and time.monotonic()<deadline:pulse(0,duration=.7,gap=4)
                assert top()==0,'The character and difficulty selections must enter the campaign'
                pulse(6,duration=.5,gap=3);pulse(6,duration=.5,gap=3)
                deadline=time.monotonic()+75
                while not u32(0x639224) and time.monotonic()<deadline:
                    pulse(6,duration=.2,gap=1)
                assert u32(0x639224),'The campaign world must load for the cinematic capture'
                deadline=time.monotonic()+450
                next_skip=time.monotonic()+15
                while (read(0x6C4198,1)!=b'\x01' or u32(0x6C92F8)) and time.monotonic()<deadline:
                    if read(0x6C4198,1)!=b'\x01' and not u32(0x6C92F8) and time.monotonic()>=next_skip:
                        # Long movie decode batches can miss the short menu pulses.
                        pulse(6,duration=2,gap=1);next_skip=time.monotonic()+15
                    else:wait(.1)
                assert read(0x6C4198,1)==b'\x01' and not u32(0x6C92F8),'The capture must show a running in-game cutscene after loading'
                for sample in range(3):
                    wait(2)
                    capture('cutscene-'+('on' if args.cinematic_bars else 'off')+'-'+str(sample))
                    checks[-1]['mask_sprites']=[hex(u32(a)) for a in (0x6CE8A0,0x6CE8A4)]
                    checks[-1]['cinematic_active']=read(0x6C4198,1)==b'\x01'
                checks.append(dict(capture='cinematic-bars-persisted',enabled=args.cinematic_bars))
            elif action[0]=='story-exit':
                for _ in range(4):pulse(0,duration=.3,gap=4)
                # Start skips the two introductory movies; Select opens the native pause menu.
                pulse(6,duration=.3,gap=12);pulse(6,duration=.3,gap=5)
                pulse(4,duration=.3,gap=4)
                assert read(0x4EF42C,1)==b'\x00','The native gameplay pause must be open'
                capture('story-pause')
                for _ in range(3):pulse(12,duration=.2,gap=.8)
                pulse(0,duration=.3,gap=3)
                if '--storage-error' in flags:
                    # Reproduce the old uninitialized profile in this owned, isolated guest only.
                    kernel.WriteProcessMemory.argtypes=[w.HANDLE,c.c_void_p,c.c_void_p,c.c_size_t,c.POINTER(c.c_size_t)]
                    zero=c.c_ubyte(0);written=c.c_size_t()
                    assert kernel.WriteProcessMemory(int(process._handle),base+0x59E5F4,c.byref(zero),1,c.byref(written)) and written.value==1
                pulse(13,duration=.2,gap=.8);pulse(0,duration=.3,gap=10)
                if '--storage-error' in flags:
                    assert top()!=0x598430,'The simulated storage error must show the native warning'
                    capture('pc-storage-warning')
                    before=len(logpath.read_text(errors='replace'))
                    pulse(1,duration=.3,gap=4)
                    assert 'Storage management: saves folder requested; background=1' in logpath.read_text(errors='replace')[before:]
                    assert process.poll() is None,'Storage management must retain the running game'
                    capture('pc-storage-folder-action')
                    pulse(0,duration=.3,gap=10)
                deadline=time.monotonic()+25
                while top()!=0x598430 and time.monotonic()<deadline:wait(.1)
                assert top()==0x598430,'Exiting story must return to the main menu without a false storage warning'
                if '--story-exit' in flags or '--scorpion-campaign' in flags or '--scorpion-campaign-enabled' in flags or '--scorpion-quickstart' in flags or '--scorpion-double-save' in flags:
                    assert read(0x59E5F4,1)==b'\x01'
                capture('story-exit-main')
                checks.append(dict(capture='story-exit',language=locale,storage_error_injected='--storage-error' in flags,returned_to_main=True))
            elif action[0]=='check-brightness':
                floating=lambda address:struct.unpack('<f',read(address,4))[0]
                assert abs(floating(0x598308)-initial_brightness)<.001,'Opening brightness must retain the saved value'
                pulse(14,duration=.15,gap=1)
                assert abs(floating(0x598308)-initial_brightness)>.001,'The player must still be able to adjust brightness'
                pulse(2,duration=.3,gap=1)
                assert abs(floating(0x598308)-.5)<.001,'Native Default must select 50 percent'
                assert abs(floating(0x632BE8)-.5*floating(0x49E580))<.001,'The rendered preview must use 50 percent brightness'
                assert abs(floating(0x4DF5C0)-initial_brightness)<.001,'The Default preview must not commit brightness'
                capture('brightness-default-50')
                pulse(1,duration=.3,gap=4)
                assert abs(floating(0x4DF5C0)-initial_brightness)<.001,'Cancel must preserve the previous brightness'
                pulse(0,duration=.3,gap=4)
                assert abs(floating(0x598308)-initial_brightness)<.001,'Reopening after cancellation must retain the previous brightness'
                pulse(2,duration=.3,gap=1)
                pulse(0,duration=.3,gap=4)
                assert abs(floating(0x4DF5C0)-.5)<.001,'Confirming Default must commit 50 percent'
                pulse(0,duration=.3,gap=4)
                assert abs(floating(0x598308)-.5)<.001,'Reopening after confirmation must retain 50 percent'
                checks.append(dict(capture='native-brightness-default',previous=initial_brightness,default=.5,cancellation_preserved=True,confirmation_applied=True))
            elif action[0]=='restart-voice':save_language(locale,True,action[1])
            elif action[0]=='restart-language':save_language(action[1],True)
            elif action[0]=='save-unchanged':save_language(locale,False)
            elif action[0]=='wait':wait(action[1])
            elif action[0]=='key':
                assert user.PostMessageW(hwnd,0x100,action[1],0);wait(.15);assert user.PostMessageW(hwnd,0x101,action[1],0);wait(1)
            elif action[0]=='fixture':
                assert user.PostMessageW(hwnd,0x8053,0,int(action[1]));wait(1)
                events.append(dict(elapsed=round(time.monotonic()-began,4),connected=action[1]))
            elif action[0]=='assert-neutral-input':
                states=re.findall(r'openshao input: state port=0 packet=\d+ buttons=([0-9A-F]+) A=(\d+) B=(\d+) X=(\d+) Y=(\d+)',logpath.read_text(errors='replace'))
                assert states and states[-1]==('0000','0','0','0','0'),('Stuck gameplay input',states[-1:] )
                checks.append(dict(capture='neutral-guest-input',buttons=list(states[-1])))
            elif action[0]=='axis':
                assert user.PostMessageW(hwnd,0x8052,action[1],action[2]);wait(action[3]);assert user.PostMessageW(hwnd,0x8052,action[1],0)
            elif action[0]=='button':pulse(action[1],duration=action[2],gap=action[3])
            elif action[0]=='capture':capture(action[1])
            elif action[0]=='navigate':navigate(action[1])
            elif action[0] in ('enter-options','enter-menu'):
                expected=int(action[1],16) if len(action)>1 else 0x5985E8
                until=time.monotonic()+30
                while top()==0x598430 and time.monotonic()<until:pulse(0,duration=.4,gap=2)
                assert top()==expected,('The native menu transition did not complete',hex(top()),hex(expected))
                wait(3)
            elif action[0]=='main-step':
                assert top()==0x598430
                before=u32(0x598468);button(action[1],True)
                until=time.monotonic()+3
                while u32(0x598468)==before and time.monotonic()<until:wait(.015)
                button(action[1],False);wait(.7)
                assert u32(0x598468)==action[2],('main boundary',u32(0x598468),action[2])
                checks.append(dict(capture='main-boundary',before=before,after=action[2]))
            elif action[0]=='assert-top':assert top()==int(action[1],16),(hex(top()),action[1])
            elif action[0]=='assert-native-clip':
                scene=u32(0x5985F4)
                assert scene and u32(scene+0x2A0)==action[1] and (u32(scene+0x340)&255)==1,('Native clip',action[1])
                assert read(0x71B0F0+ord('<'),1)==bytes([136]) and read(0x71B0F0+ord('>'),1)==bytes([135]),'Native horizontal arrow glyph map'
            elif action[0]=='panel-status':
                until=time.monotonic()+15
                while u32(u32(top()+12)+0x2E4)!=action[1] and time.monotonic()<until:wait(.05)
                assert u32(u32(top()+12)+0x2E4)==action[1],('panel status',action[1],u32(u32(top()+12)+0x2E4))
            elif action[0]=='return-options':
                # Capturing all native textures can delay the guest input poll beyond a short pulse.
                until=time.monotonic()+25
                while not (top()==0x5985E8 and u32(u32(top()+12)+0x2E4)==3) and time.monotonic()<until:
                    assert top()!=0x598430,'Unexpected return to the main menu'
                    pulse(1,duration=.4,gap=3)
                assert top()==0x5985E8 and u32(u32(top()+12)+0x2E4)==3,'Native Options return did not finish'
                wait(1.5)
            elif action[0]=='return-main':
                until=time.monotonic()+25
                while top()!=0x598430 and time.monotonic()<until:pulse(1,duration=.4,gap=3)
                assert top()==0x598430,'Native main-menu return did not finish'
            elif action[0]=='choose':
                control=top();target=action[1]
                for attempt in range(12):
                    if u32(control+0x38)==target:break
                    pulse(12,duration=.18,gap=.8)
                assert u32(control+0x38)==target,('selection',hex(control),target,u32(control+0x38))
                until=time.monotonic()+15
                while u32(u32(control+12)+0x2E4)!=3 and time.monotonic()<until:wait(.05)
                assert u32(u32(control+12)+0x2E4)==3,'Native selection animation did not finish'
            elif action[0]=='accept':pulse(0,duration=.3,gap=4)
            elif action[0]=='back':pulse(1,duration=.3,gap=4)
            elif action[0]=='check-aspect':
                world=u32(0x639224);camera=u32(world+0x308)
                value=struct.unpack('<f',read(camera+0x20,4))[0]
                assert abs(value-action[1]/(4/3))<.001,(value,action[1])
            elif action[0]=='check-not-saved':
                import tomllib
                assert tomllib.loads((data/'settings.toml').read_text())['aspect_ratio']=='original'
            elif action[0]=='check-saved-aspect':
                import tomllib
                assert tomllib.loads((data/'settings.toml').read_text())['aspect_ratio']==action[1]
            elif action[0]=='block-settings':
                (data/'settings.toml').rename(data/'settings.before.toml');(data/'settings.toml').mkdir()
            elif action[0]=='restore-settings':
                (data/'settings.toml').rmdir();(data/'settings.before.toml').rename(data/'settings.toml')
            elif action[0]=='verify-write-error':
                assert (data/'settings.toml').is_dir()
                assert abs(struct.unpack('<f',read(0x4DF5C4,4))[0]-initial_music/100)<.001
                assert 'Recomp settings:' in logpath.read_text(errors='replace')
            elif action[0]=='verify-reloaded':
                import tomllib
                expected=tomllib.loads((data/'settings.toml').read_text())
                assert args.settings
                for index,field in enumerate(['music_volume','effects_volume','speech_volume'] if expected['game']['override_original'] else []):
                    value=struct.unpack('<f',read(0x4DF5C4+4*index,4))[0]
                    assert abs(value-expected['game'][field]/100)<.001,(field,value,expected['game'][field])
                checks.append(dict(capture='restart-persistence',music=expected['game']['music_volume'],master=expected['master_volume']))
            elif action[0]=='verify-saved':
                import tomllib
                saved=tomllib.loads((data/'settings.toml').read_text(encoding='utf8'))
                assert saved['master_volume']==5,saved
                assert saved['game']['music_volume']==initial_music,saved
                assert abs(struct.unpack('<f',read(0x4DF5C4,4))[0]-initial_music/100)<.001
                checks.append(dict(capture='persistence',master=saved['master_volume'],music=saved['game']['music_volume']))
            elif action[0]=='verify-remapping':
                import tomllib
                saved=tomllib.loads((data/'settings.toml').read_text(encoding='utf8'))
                assert saved['mouse']['sensitivity']==105 and saved['mouse']['invert_x'],saved['mouse']
                assert saved['mouse']['bindings']['left']=='y' and saved['mouse']['motion']=='left_stick',saved['mouse']
                assert saved['controller']['bindings']['a']=='a',saved['controller']
                assert saved['keyboard']['bindings']['a']==[32,0],saved['keyboard']
                checks.append(dict(capture='remapping-persistence',mouse=saved['mouse']))
            elif action[0]=='quit':quit=True
            else:raise AssertionError(action)
        (run/('done-'+str(sequence)+'.json')).write_text(json.dumps(checks))
        print('DONE',sequence,flush=True);sequence+=1
        if quit:break
    if not closed:
        assert user.PostMessageW(hwnd,0x10,0,0);closed=True;process.wait(timeout=8)
except Exception as e:
    failure=repr(e)
finally:
    if process.poll() is None:process.terminate()
    code=process.wait(timeout=8)
    for child in owned_children:
        if child.poll() is None:child.terminate();child.wait(timeout=8)
        kernel.CloseHandle(child._handle)
    log.close()
    saves_after={str(f.relative_to(save_source)):hashlib.sha256(f.read_bytes()).hexdigest() for f in save_source.rglob('*') if f.is_file()}
    copied_saves={str(f.relative_to(run/'saves/E/UDATA/4d570029')):hashlib.sha256(f.read_bytes()).hexdigest() for f in (run/'saves/E/UDATA/4d570029').rglob('*') if f.is_file()}
    result=dict(copied_title_saves_unchanged=copied_saves==save_hashes,language=locale,mode=mode,output=size,exit_code=code,closed=closed,failure=failure,checks=checks,events=events,trace=trace,visible_windows=visible,source_xbe_unchanged=hashlib.sha256(xbe.read_bytes()).hexdigest()==source_hash,source_saves_unchanged=saves_after==save_hashes,exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest())
    (run/'result.json').write_text(json.dumps(result,indent=2));print('RESULT',mode,code,failure,flush=True)
assert not failure and code==0 and closed
if '--restart' in flags or '--voices' in flags or '--exit-desktop' in flags:assert result['copied_title_saves_unchanged']
assert result['source_xbe_unchanged'] and result['source_saves_unchanged'] and not visible
