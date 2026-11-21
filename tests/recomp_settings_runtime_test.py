"""Opt-in real-game Konfigure Recompilation regression with isolated saves and virtual input."""
from pathlib import Path
import ctypes as c
from ctypes import wintypes as w
import hashlib,json,os,re,shutil,struct,subprocess,time

import argparse
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--exe',type=Path,required=True)
parser.add_argument('--game',type=Path,required=True,help='Original extracted game folder, default.xbe or disc image')
parser.add_argument('--save-fixture',type=Path,required=True,help='Read-only source title directory containing profile saves')
parser.add_argument('--mods',type=Path,help='Read-only source mods directory for language packs')
parser.add_argument('--cache-fixture',type=Path,help='Optional read-only prewarmed shader cache directory for faster isolated startup')
parser.add_argument('--verify-pause-exit',action='store_true',help='In the pause Load Game scenario, confirm Yes and verify desktop exit')
parser.add_argument('--coop-load',action='store_true',help='Verify that keyboard P1 and one virtual controller P2 can load an occupied Ko-op save')
parser.add_argument('--language',default='en',choices=['en','fr','de','es','it','pt-BR'])
parser.add_argument('--voice-language',default='en',choices=['en','fr','de','es','it','pt-BR'])
parser.add_argument('--aspect',default='original',choices=['original','16:9','16:10','21:9','32:9','auto'])
parser.add_argument('--cinematic-bars',action='store_true',help='Enable cinematic bars through Enhancements in the cinematic-bars scenario')
parser.add_argument('--cursor-stability',action='store_true',help='Capture the main-menu selection star repeatedly at rest')
parser.add_argument('--return-audit',action='store_true',help='Sample the real gold marker and Main return movie without taking a screenshot each frame')
parser.add_argument('--return-probe-frame',type=int,help='Capture Main during the native clip16 return once its movie frame reaches this number')
parser.add_argument('--cursor-visual-quick',action='store_true',help='Capture the main Game Mode selection ornament for visual comparison')
parser.add_argument('--menu-roundtrip',action='store_true',help='Visit every screen reachable from Main, including Survival, and audit Main backdrop frame by frame')
parser.add_argument('--scenario',default='navigation',choices=['navigation','survival','menu-layout','persistence','display','native-options','audio-output','ornament-motion','reload','write-failure','remapping','prompts','transition','polish','presentation','input-handoff','details','headings','restart','voices','language-preview','brightness','story-exit','storage-error','exit-desktop','scorpion-preview','scorpion-campaign','scorpion-campaign-enabled','scorpion-quickstart','scorpion-double-save','pause-recomp','pause-recomp-load','cinematic-bars','save-lists','save-pages','save-pages-reload','campaign-roster'])
parser.add_argument('--survival-koop',action='store_true')
parser.add_argument('--survival-locked',action='store_true',help='Start Survival from a save root without the global unlock to verify locked menu behavior')
parser.add_argument('--survival-quick',action='store_true',help='Skip intermediate captures during a Survival runtime probe')
parser.add_argument('--survival-difficulty',type=int,default=1,choices=range(4))
parser.add_argument('--survival-fighter',type=int,default=4,choices=range(8),help='Native fighter ID to choose in a Survival probe')
parser.add_argument('--survival-check-back',action='store_true',help='Return from difficulty to the Versus-backed Survival fighter screen and confirm again')
parser.add_argument('--survival-menu-back',action='store_true',help='Cancel both Survival medal pages, reenter, and verify the owned announcement stops')
parser.add_argument('--survival-keyboard-enter',action='store_true',help='Use short Enter key presses for the Survival medal menus')
parser.add_argument('--survival-keyboard-only',action='store_true',help='Disconnect the virtual controller before the solo Survival flow')
parser.add_argument('--survival-short-space',action='store_true',help='Require each Survival medal menu to accept a single 150 ms Space press')
parser.add_argument('--survival-capture-cursor',action='store_true',help='Capture extra settled frames of the native Survival selector')
parser.add_argument('--survival-capture-variants',action='store_true',help='Capture both Solo and Ko-op focus states before confirming Solo')
parser.add_argument('--survival-frame-audit',action='store_true',help='Sample native character, difficulty and Main actor states throughout each frame/transition')
parser.add_argument('--survival-idle-seconds',type=int,default=0,help='Leave both Game Mode option pages idle for this many seconds and assert the native attract screen does not take control')
parser.add_argument('--survival-popup-idle-seconds',type=int,default=0,help='Leave the native Survival difficulty modal open without input to catch delayed portrait/stage transitions')
parser.add_argument('--survival-menu-only',action='store_true',help='Capture the two Survival medal pages and exit before fighter selection')
parser.add_argument('--survival-rules-only',action='store_true',help='Hold the first arena on the native A-confirmed rules frame for a visual probe')
parser.add_argument('--survival-characters-only',action='store_true',help='Stop after verifying the full native Survival roster and focus states')
parser.add_argument('--survival-levels',action='store_true',help='Probe each arena through isolated native map-transition fixtures')
parser.add_argument('--survival-verify-upgrades',action='store_true',help='Verify campaign move ranks and maximum health in Survival gameplay')
parser.add_argument('--survival-verify-start-select',action='store_true',help='Verify Start opens the original Select pause menu in Survival')
parser.add_argument('--slot',type=int,choices=range(1,10),help='Source save slot (default: 1, required for the campaign totem fixture)')
parser.add_argument('--page-files',type=Path,help='Saves folder of an earlier save-pages run whose page files the save-pages-reload scenario starts with')
parser.add_argument('--clean-profile',action='store_true',help='Use empty isolated saves for the brightness scenario')
parser.add_argument('--output',type=Path)
parser.add_argument('--settings',type=Path,help='Read-only configuration fixture for restart validation')
parser.add_argument('--restart-count',type=int,default=6,choices=range(1,7),help='Number of consecutive language changes in the restart and voices scenarios')
parser.add_argument('--resume-main-menu',action='store_true',help='Require the restart startup path to reach the main menu without synthetic input')
campaign_options = parser.add_argument_group('campaign roster')
campaign_options.add_argument('--campaign-fighter', type=int, choices=range(8), help='Player 1 selector slot (default: 4)')
campaign_options.add_argument('--campaign-partner', type=int, choices=range(8), help='Player 2 selector slot, enabling Ko-op')
campaign_options.add_argument('--campaign-check-back', action='store_true', help='Back out of difficulty and confirm the same selection again')
campaign_options.add_argument('--campaign-save-roundtrip', action='store_true', help='Load the isolated totem fixture selected by --slot, save through the native UI and reload it')
campaign_options.add_argument('--campaign-death-continue', action='store_true', help='After the save roundtrip, inject live HP 0 and require native death, the actual Continue prompt and checkpoint restoration')
campaign_options.add_argument('--campaign-traversal', action='store_true', help='Request native platform skill acquisition and traversal validation')
campaign_options.add_argument('--campaign-move-menus', action='store_true', help='Inspect the native Moves menu for each active player after startup')
campaign_options.add_argument('--campaign-frontend-only', action='store_true', help='Stop before confirming difficulty')
campaign_options.add_argument('--campaign-saved-unlocks', action='store_true', help='Use the copied profile’s campaign unlocks')
campaign_options.add_argument('--campaign-print-plan', action='store_true', help='Print the commands and virtual gamepad count without launching')
args=parser.parse_args()

campaign_scenario = args.scenario == 'campaign-roster'
if not campaign_scenario:
    for name in ('fighter', 'partner', 'check_back', 'save_roundtrip', 'death_continue', 'traversal', 'move_menus',
                 'frontend_only', 'saved_unlocks', 'print_plan'):
        value = getattr(args, 'campaign_' + name)
        if value is not None and value is not False:
            parser.error('--campaign-' + name.replace('_', '-') + ' requires --scenario campaign-roster')
if campaign_scenario:
    if args.coop_load:
        parser.error('--coop-load uses its own load fixture and cannot be combined with campaign-roster')
    fighter = 4 if args.campaign_fighter is None else args.campaign_fighter
    partner = args.campaign_partner
    virtual_gamepads = 1 if partner is None else 2
    campaign_commands = [['navigate', int(partner is not None)],
                         ['campaign-characters', fighter, partner, args.campaign_check_back]]
    if args.campaign_check_back and fighter == partner:
        parser.error('--campaign-check-back requires distinct fighters to reach difficulty')
    if not args.campaign_frontend_only and fighter != partner:
        campaign_commands.append(['campaign-start', fighter, partner])
    if args.campaign_move_menus:
        if args.campaign_frontend_only or fighter == partner:
            parser.error('--campaign-move-menus requires campaign gameplay')
        campaign_commands.append(['campaign-move-menus'])
    if args.campaign_save_roundtrip:
        if args.campaign_frontend_only or args.campaign_check_back or fighter == partner:
            parser.error('--campaign-save-roundtrip uses the original Load Game fixture and requires distinct fighters')
        if args.campaign_saved_unlocks:
            parser.error('--campaign-saved-unlocks applies to character selection')
        if args.slot is None:
            parser.error('--campaign-save-roundtrip requires --slot with a save beside a totem')
        campaign_commands = [['navigate', 3], ['enter-menu', '0x5967C0'],
                             ['campaign-save-roundtrip', fighter, partner]]
        if args.campaign_move_menus:
            campaign_commands.append(['campaign-move-menus'])
    if args.campaign_traversal:
        parser.error('Native campaign traversal probes are not implemented')
    if args.campaign_death_continue:
        if not args.campaign_save_roundtrip:
            parser.error('--campaign-death-continue requires --campaign-save-roundtrip')
        campaign_commands.append(['campaign-death-continue'])
    campaign_commands.append(['quit'])
    if args.campaign_print_plan:
        print(json.dumps(dict(commands=campaign_commands, virtual_gamepads=virtual_gamepads)))
        raise SystemExit(0)
else:
    virtual_gamepads = 1
if args.slot is None:args.slot=1
if args.clean_profile and args.scenario!='brightness':parser.error('--clean-profile requires --scenario brightness')
if args.cursor_stability and args.scenario!='menu-layout':parser.error('--cursor-stability requires --scenario menu-layout')
if args.return_audit and args.scenario!='transition':parser.error('--return-audit requires --scenario transition')
if args.return_probe_frame is not None and args.scenario!='transition':parser.error('--return-probe-frame requires --scenario transition')
if args.cursor_visual_quick and args.scenario!='menu-layout':parser.error('--cursor-visual-quick requires --scenario menu-layout')
if args.menu_roundtrip and args.scenario!='menu-layout':parser.error('--menu-roundtrip requires --scenario menu-layout')
if args.survival_keyboard_only and args.survival_koop:parser.error('--survival-keyboard-only requires solo play')
if args.survival_locked and args.scenario!='survival':parser.error('--survival-locked requires --scenario survival')
if (args.survival_frame_audit or args.survival_idle_seconds or args.survival_popup_idle_seconds) and args.scenario!='survival':
    parser.error('Survival frame/idle audit options require --scenario survival')
if os.name!='nt':parser.error('This native input/capture driver uses the owned hidden Win32 window')
if args.scenario=='reload' and not args.settings:parser.error('reload requires --settings with a saved configuration')
if args.settings and args.scenario not in ('reload','navigation','native-options'):
    parser.error('--settings is supported by reload, navigation and native-options')
if args.scenario=='display' and args.aspect!='original':parser.error('display starts from --aspect original')
root=Path(__file__).resolve().parents[1];mode=args.aspect;locale=args.language
survival_scenario=args.scenario=='survival'
if survival_scenario:args.scenario='navigation'
flags={dict(navigation='--recomp',persistence='--save',display='--display-test',**{'menu-layout':'--menu-layout','native-options':'--suite','audio-output':'--audio-output','ornament-motion':'--ornament-motion','reload':'--reload','write-failure':'--write-failure','remapping':'--remapping','prompts':'--prompts','transition':'--transition','polish':'--polish','presentation':'--presentation','input-handoff':'--input-handoff','details':'--details','headings':'--headings','restart':'--restart','voices':'--voices','language-preview':'--language-preview','brightness':'--brightness','story-exit':'--story-exit','storage-error':'--storage-error','exit-desktop':'--exit-desktop','scorpion-preview':'--scorpion-preview','scorpion-campaign':'--scorpion-campaign','scorpion-campaign-enabled':'--scorpion-campaign-enabled','scorpion-quickstart':'--scorpion-quickstart','scorpion-double-save':'--scorpion-double-save','pause-recomp':'--pause-recomp','pause-recomp-load':'--pause-recomp-load','cinematic-bars':'--cinematic-bars','save-lists':'--save-lists','save-pages':'--save-pages','save-pages-reload':'--save-pages-reload','campaign-roster':'--campaign-roster'})[args.scenario]}
out=(args.output or root/'work/recomp-runtime').resolve();out.mkdir(parents=True,exist_ok=True)
if args.coop_load:flags={'--coop-load'}
run=out/('runtime-'+locale+'-'+mode.replace(':','-')+'-'+str(time.time_ns()));run.mkdir()
if '--coop-load' in flags:
    steps=[['navigate',3],['enter-menu','0x5967C0'],['wait',2],['coop-load'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--menu-layout' in flags:
    steps=[]
    for row in [0,1,2,9,3,4,5,6,7,8]:
        steps += [['navigate',row],['assert-main-row',row],['capture','main-row-'+str(row)]]
        if args.cursor_stability and row in (0,9,8):
            steps += [['wait',.3],['capture','cursor-row-'+str(row)+'-short'],
                      ['wait',1.2],['capture','cursor-row-'+str(row)+'-long']]
        if row==1:
            steps += [['fixture',False],['capture','controller-notice-koop'],['fixture',True],['navigate',2],
                      ['fixture',False],['capture','controller-notice-versus'],['fixture',True],['navigate',3],
                      ['fixture',False],['capture','controller-notice-hidden'],['fixture',True]]
    steps += [['main-step',12,0],['main-step',11,8],['enter-exit'],['accept'],['assert-top','0x598430'],['assert-main-row',8],['capture','exit-returned'],
              ['navigate',7],['enter-options'],['capture','enhancements'],['back'],['assert-top','0x598430'],['assert-main-row',7],
              ['navigate',6],['enter-options'],['capture','recompilation'],['back'],['assert-top','0x598430'],['assert-main-row',6],['quit']]
    if args.cursor_visual_quick:steps=[['navigate',9],['assert-main-row',9],['capture','gold-and-star'],['quit']]
    if args.menu_roundtrip:
        # Every path in and out of Main, audited per frame; keyboard and virtual pad are the two players.
        steps=[['capture','roundtrip-start'],
               ['navigate',9],['accept'],['capture','roundtrip-game-mode'],
               ['accept'],['capture','roundtrip-players'],
               ['tap-to-row',12,1],['fixture',False],['wait',1],['capture','roundtrip-players-koop-notice'],['fixture',True],['wait',1],
               ['accept'],['assert-top','0x598DF0'],['capture','roundtrip-survival-koop-fighters'],
               ['back'],['assert-top','0x598430'],['capture','roundtrip-back-to-koop'],
               # Let the Players page settle, or Up is ignored and the run continues in Ko-op.
               ['wait',1.5],['tap-to-row',11,0],
               ['accept'],['assert-top','0x598DF0'],['capture','roundtrip-survival-fighters'],
               ['back'],['assert-top','0x598430'],['capture','roundtrip-back-to-players'],
               # Confirm a player and press Back once the roster accepts input.
               ['button',0,.12,2.5],['assert-top','0x598DF0'],['button',1,.12,.4],['capture','roundtrip-quick-back'],
               ['wait',2],['button',0,.12,2.5],['assert-top','0x598DF0'],['button',1,.12,.4],['capture','roundtrip-quick-back-2'],
               # Fighter chosen, difficulty popup, then Back twice to Main's Players page.
               ['wait',2],['accept-until','0x598DF0'],['wait',1.5],['pick-fighter'],['capture','roundtrip-difficulty'],
               # One short Back (B on the controller that confirmed) per screen: popup, then roster.
               ['button',1,.1,2.5],['assert-top','0x598DF0'],['capture','roundtrip-back-to-fighters'],
               ['button',1,.1,3],['assert-top','0x598430'],['capture','roundtrip-back-from-difficulty'],
               ['back'],['capture','roundtrip-back-to-modes'],
               ['back'],['assert-top','0x598430'],['capture','roundtrip-after-game-mode'],
               ['wait',3],['capture','roundtrip-after-game-mode-3s']]
        for row,target,name in [(0,'0x598DF0','single-player'),(1,'0x598DF0','koop'),(2,'0x598DF0','versus'),(3,'0x5967C0','load-game'),
                                (4,None,'options'),(5,'0x598470','kontent'),(6,None,'recompilation'),(7,None,'enhancements')]:
            steps+=[['navigate',row],['enter-menu',target] if target else ['enter-options'],['capture','roundtrip-'+name],
                    ['return-main'],['wait',1.5],['capture','roundtrip-after-'+name]]
        steps+=[['navigate',9],['accept'],['back'],['assert-top','0x598430'],['wait',3],['capture','roundtrip-final'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--cinematic-bars' in flags:
    steps=[['navigate',7],['enter-options'],['key',40],['key',40]]
    if args.cinematic_bars:steps += [['key',39]]
    steps += [['capture','enhancement-'+('on' if args.cinematic_bars else 'off')],['back']]
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
if '--save-lists' in flags:
    # Main-menu Load Game, then the first slot; a totem save places the hero at its totem, where A opens Save Game.
    steps=[['navigate',3],['enter-menu','0x5967C0'],['wait',2],['capture','load-main'],
           ['trigger',5],['capture','load-page-2'],['trigger',4],['trigger',4],['capture','load-page-10'],
           ['trigger',5],['capture','load-page-1'],
           ['slot-variants','load'],['save-list-totem'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--save-pages' in flags:
    # Page and save/delete on page 2, leave with page 2 shown, delete on page 1; save-pages-reload continues.
    steps=[['navigate',3],['enter-menu','0x5967C0'],['wait',2],['capture','load-main'],
           ['trigger',5],['capture','load-page-2'],['trigger',4],['trigger',4],['capture','load-page-10'],
           ['trigger',5],['capture','load-page-1'],['pages-session'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--save-pages-reload' in flags:
    steps=[['navigate',3],['enter-menu','0x5967C0'],['wait',2],['pages-reload'],['quit']]
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
    # PT-BR needs a pack: visit the five original languages both ways and restore the start value.
    languages=['en','fr','de','es','it','pt-BR']
    if args.mods:
        for index in range(1,len(languages)+1):
            selected=languages[(languages.index(locale)+index)%len(languages)]
            steps+=[['key',39],['capture','language-'+selected]]
    else:
        for selected in languages[1:5]:steps+=[['key',39],['capture','language-'+selected]]
        for selected in reversed(languages[:4]):steps+=[['key',37],['capture','language-back-'+selected]]
    steps+=[['key',27],['assert-native-clip',34],['return-main'],['quit']]
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
if '--ornament-motion' in flags:
    steps=[['navigate',5],['accept'],['wait',1],['back'],['navigate',6],['enter-options'],['wait',1],
           ['accept'],['wait',1],['back']]+[['key',40]]*3+[
           ['accept'],['wait',1],['back'],['back'],['assert-top','0x598430'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--audio-output' in flags:
    steps=[['navigate',4],['enter-options'],['choose',1],['accept'],['panel-status',6],
           ['capture','retail-audio-initial'],['native-audio-wrap'],
           ['capture','retail-audio-speech'],['back'],['panel-status',3],['back'],['assert-top','0x598430'],
           ['navigate',6],['enter-options']]+[['key',40]]*3+[
           ['key',13],['capture','recomp-audio-initial'],['key',40],['capture','recomp-audio-selected'],
           ['key',39],['capture','recomp-audio-surround'],['assert-audio-mode',1,False],
           ['key',27],['key',40],['key',40],['key',13],['wait',3],['assert-top','0x598430'],['assert-audio-mode',2,True],
           ['navigate',6],['enter-options']]+[['key',40]]*3+[
           ['key',13],['key',40],['key',39],['capture','recomp-audio-mono-draft'],
           ['assert-audio-mode',2,True],['key',27],['key',40],['key',40],['key',40],['key',13],['wait',3],
           ['assert-top','0x598430'],['assert-audio-mode',2,True],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--suite' in flags:
    steps=[['navigate',4],['enter-options'],['capture','settings-home'],['choose',1],['accept'],['panel-status',6],['capture','audio'],['back'],['panel-status',3],['assert-top','0x5985E8'],['choose',2],['accept'],['panel-status',7],['capture','gameplay'],['back'],['panel-status',3],['back'],['assert-top','0x598430'],['navigate',5],['accept'],['capture','kontent'],['quit']]
    (run/'command-1.json').write_text(json.dumps(steps))
if '--recomp' in flags:
    down=lambda n:[['button',12,.12,.7]]*n
    steps=[['navigate',6],['capture','seventh-row'],['enter-options'],['capture','settings-home'],['accept'],['capture','display']]+down(3)+[['button',14,.12,.7],['capture','display-edit'],['back']]+down(1)+[['accept'],['capture','graphics'],['back']]+down(1)+[['accept'],['capture','controls'],['accept'],['capture','controller'],['accept'],['axis',2,-25000,.3],['wait',1],['capture','controller-conflict'],['back']]+down(1)+[['accept'],['accept'],['key',90],['capture','keyboard-bound'],['back'],['back']]+down(1)+[['accept'],['capture','audio'],['back']]+down(1)+[['accept'],['capture','language'],['back']]+down(1)+[['accept'],['capture','interface'],['back']]+down(2)+[['accept'],['wait',2],['capture','returned-main'],['quit']]
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
game=args.game.resolve();disc=game.suffix.lower() in ('.iso','.xiso')
if disc:xbe=game;source_hash=str(game.stat().st_size)+':'+str(game.stat().st_mtime_ns)
else:game=game.parent if game.is_file() else game;xbe=game/'default.xbe';source_hash=hashlib.sha256(xbe.read_bytes()).hexdigest()
data=run/'data';data.mkdir()
if campaign_scenario:
    (run/'command-1.json').write_text(json.dumps(campaign_commands))
if survival_scenario:(run/'command-1.json').write_text(json.dumps([['survival'],['quit']]))
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
if (survival_scenario and not args.survival_locked) or args.menu_roundtrip:
    # Survival probes start with a saved portal discovery; real saves and unlocks stay untouched.
    (run/'saves/openshao-survival-unlock.dat').write_bytes(b'OpenShao Survival Unlocked v1\n')
if args.page_files:
    for page in sorted(args.page_files.resolve().glob('openshao-save-page-*.bin')):shutil.copy2(page,run/'saves'/page.name)
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
survival_frame_log=(run/'survival-frames.jsonl').open('w',encoding='utf-8',buffering=1) if args.survival_frame_audit else None
# Per-frame Main and backdrop state for the menu round trip (about every 16 ms).
roundtrip_frame_log=(run/'roundtrip-frames.jsonl').open('w',encoding='utf-8',buffering=1) if args.menu_roundtrip else None
last_roundtrip_sample=0
return_frames=[]
last_return_sample=0
gold_handle=None
return_seen_custom=False
last_survival_frame_sample=0.0
logpath=run/'native.log';began=time.monotonic()
log=logpath.open('wb')
process=subprocess.Popen([str(exe),'--background','--timeout','600','--virtual-gamepads',str(virtual_gamepads),'--disc' if disc else '--game',str(game),'--data-dir',str(data),'--save-dir',str(run/'saves')]+(['--resume-main-menu'] if args.resume_main_menu else []),cwd=run,env=env,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW|subprocess.BELOW_NORMAL_PRIORITY_CLASS)
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
ornament_motion=[]
last_ornament_frame=None
def sample_ornament_motion():
    global last_ornament_frame
    depth=u32(0x6C9308)
    control=u32(0x6C9308+4*depth) if depth<=16 else 0
    if control not in (0x598430,0x598470,0x5985E8):return
    scene=u32(control+12)
    if not scene:return
    native=(control,u32(scene+0x2A0),u32(scene+0x33C))
    if native==last_ornament_frame:return
    queue=read(0x63F5E0,0xD804);count=struct.unpack_from('<I',queue)[0]
    if count>512:return
    quads=[]
    for i in range(count):
        points=[struct.unpack_from('<2f',queue,4+i*0x6C+0x10+v*16) for v in range(3)]
        left=min(p[0] for p in points);right=max(p[0] for p in points)
        top_y=min(p[1] for p in points);bottom=max(p[1] for p in points)
        if -200<left<180 and 5<right-left<180 and 4<bottom-top_y<160:
            quads.append(dict(index=i,texture=hex(struct.unpack_from('<I',queue,4+i*0x6C+0x68)[0]),
                              bounds=[round(v,4) for v in (left,top_y,right,bottom)],
                              uv=[round(v,5) for v in struct.unpack_from('<6f',queue,4+i*0x6C+0x40)],
                              alpha=queue[4+i*0x6C+0x5B]))
    # Wait for the flipped lower cap (queued after the shaft) instead of a partial title.
    lower_cap=any(q['uv'][3]>.98 and q['uv'][4]<.01 and 115<q['bounds'][2]-q['bounds'][0]<135 for q in quads)
    if not lower_cap or native!=(control,u32(scene+0x2A0),u32(scene+0x33C)):return
    ornament_motion.append(dict(seconds=round(time.monotonic()-began,4),control=hex(control),
                                clip=native[1],frame=native[2],status=u32(scene+0x2E4),
                                finished=bool(u32(scene+0x340)&255),quads=quads))
    last_ornament_frame=native
def tick():
    global base,last,resized,last_survival_frame_sample,last_roundtrip_sample,last_return_sample,gold_handle,return_seen_custom
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
        if m:
            # The reservation is logged before the title's data pages are committed.
            candidate=int(m[1],16);probe=c.c_uint32();count=c.c_size_t()
            if kernel.ReadProcessMemory(int(process._handle),candidate+0x6C9308,c.byref(probe),4,c.byref(count)) and count.value==4:
                base=candidate
    if base:
        depth=u32(0x6C9308);top=u32(0x6C9308+depth*4) if depth<=16 else 0
        if args.return_audit and time.monotonic()-last_return_sample>=.032:
            last_return_sample=time.monotonic()
            scene=u32(0x59843C)
            clip=u32(scene+0x2A0) if scene else 0
            frame=u32(scene+0x33C) if scene else 0
            row=u32(0x598468)
            finished=bool(read(scene+0x340,1)[0]) if scene else False
            if top==0x5985E8:return_seen_custom=True
            gold_y=gold_alpha=None
            sample_gold=(top==0x598430 and row==6 and ((gold_handle is None and clip==6 and finished) or return_seen_custom))
            if sample_gold:
                count=u32(0x63F5E0)
                if 0<count<=512:
                    queue=read(0x63F5E4,count*0x6C)
                    for i in range(count):
                        record=i*0x6C
                        handle=struct.unpack_from('<I',queue,record+0x68)[0]
                        points=[struct.unpack_from('<2f',queue,record+0x10+v*16) for v in range(3)]
                        left=min(p[0] for p in points);right=max(p[0] for p in points)
                        quad_top=min(p[1] for p in points);bottom=max(p[1] for p in points)
                        # Identify the 64x60 glyph at rest, then follow its GPU handle rather than its asset CRC.
                        if gold_handle is None and clip==6 and finished and 40<=left<=49 and 59<=right-left<=69 and 52<=bottom-quad_top<=67 and 285<=quad_top<=325:
                            gold_handle=handle
                        if gold_handle is not None and handle==gold_handle:
                            gold_y=quad_top if gold_y is None else min(gold_y,quad_top)
                            alpha=queue[record+0x5B]
                            gold_alpha=alpha if gold_alpha is None else max(gold_alpha,alpha)
            if sample_gold or return_seen_custom:
                return_frames.append(dict(t=round(last_return_sample-began,4),top=hex(top),row=row,
                    clip=clip,frame=frame,finished=finished,white_visible=u32(0x592D20+0xC),
                    gold_y=round(gold_y,3) if gold_y is not None else None,gold_alpha=gold_alpha))
        if roundtrip_frame_log is not None and time.monotonic()-last_roundtrip_sample>=.016:
            last_roundtrip_sample=time.monotonic()
            main_movie=u32(0x59843C)
            roundtrip_frame_log.write(json.dumps(dict(t=round(last_roundtrip_sample-began,4),top=hex(top),depth=depth,
                main_row=u32(0x598468),main_clip=u32(main_movie+0x2A0) if main_movie else None,
                main_frame=u32(main_movie+0x33C) if main_movie else None,main_status=u32(main_movie+0x2E4) if main_movie else None,
                characters_clip=u32(u32(0x598DFC)+0x2A0) if u32(0x598DFC) else None,
                backdrops=[u32(o+0xC) for o in (0x593A40,0x58E838,0x58F558,0x593068,0x5912E0,0x591970,0x58E4F0)]),
                separators=(',',':'))+'\n')
        if survival_frame_log is not None:
            now=time.monotonic()
            if now-last_survival_frame_sample>=.016:
                last_survival_frame_sample=now
                main_movie=u32(0x59843C)
                chars_movie=u32(0x598DFC)
                roster_count=read(0x598C54,1)[0]
                sample=dict(t=round(now-began,4),top=hex(top),main_selection=u32(0x598468),
                    main_scene=u32(0x598430+0xC),
                    main_clip=u32(main_movie+0x2A0) if main_movie else None,
                    main_frame=u32(main_movie+0x33C) if main_movie else None,
                    characters_clip=u32(chars_movie+0x2A0) if chars_movie else None,
                    characters_frame=u32(chars_movie+0x33C) if chars_movie else None,
                    roster=list(read(0x598C28,min(roster_count,8))),
                    status=[u32(0x598DF0+0x48+i*12) for i in range(8)],
                    actor_movie=[dict(clip=u32(0x58F8A0+i*0x348+0x2A0),
                                      frame=u32(0x58F8A0+i*0x348+0x33C),
                                      finished=read(0x58F8A0+i*0x348+0x340,1)[0])
                                 for i in range(8)],
                    popup_status=u32(0x596810+0x1C))
                survival_frame_log.write(json.dumps(sample,separators=(',',':'))+'\n')
        fields=struct.unpack('<12H',read(0x67C668,24))
        settings_scene=u32(0x5985F4)
        movie=(u32(settings_scene+0x2A0),u32(settings_scene+0x33C),u32(settings_scene+0x2E4),u32(settings_scene+0x340)&255) if settings_scene else ()
        character=[]
        if survival_scenario and top==0x598DF0:
            scene=u32(0x598DFC);selected=u32(0x598E2C)
            character=[u32(scene+0x2A0),read(scene+0x340,1)[0],selected,read(0x598E34,2).hex(),
                       [u32(0x598E38+i*12) for i in range(8)],
                       [read(0x58F8A0+i*0x348+0x340,1)[0] for i in range(8)]] if scene else []
        current=(depth,top,u32(0x598468),*fields,*movie,json.dumps(character))
        if current!=last:
            trace.append(dict(elapsed=round(time.monotonic()-began,4),depth=depth,top=f'{top:08X}',selected=current[2],input_words=list(fields),settings_movie=list(movie),character_state=character))
            last=current
    if base and '--ornament-motion' in flags:sample_ornament_motion()
    time.sleep(.001 if '--ornament-motion' in flags else .012)
def wait(seconds):
    until=time.monotonic()+seconds
    while time.monotonic()<until:tick()
def top():
    d=u32(0x6C9308);return u32(0x6C9308+d*4) if d<=16 else 0
def button(index,held,controller=0):
    assert hwnd and user.PostMessageW(hwnd,0x8051,index|(controller<<16),int(held))
    event = dict(elapsed=round(time.monotonic()-began,4),button=index,held=held)
    if campaign_scenario:
        event['virtual_controller'] = controller
    events.append(event)
def pulse(index,duration=.12,gap=.55,controller=0):
    button(index,True,controller);wait(duration);button(index,False,controller);wait(gap)

def campaign_acknowledged_accept(player):
    packet = re.compile(rf'openshao input: state port={player} packet=\d+ buttons=[0-9A-F]+ A=255\b')
    offset = logpath.stat().st_size
    button(0, True, controller=player)
    seen = False
    try:
        deadline = time.monotonic() + 4
        while time.monotonic() < deadline:
            with logpath.open('rb') as log:
                log.seek(offset)
                if packet.search(log.read().decode('utf-8', errors='replace')):
                    seen = True
                    break
            wait(.05)
    finally:
        button(0, False, controller=player)
    wait(.7)
    assert seen, ('Guest did not receive Accept from the virtual controller', player + 1)

def held_until_seen(press,release,field):
    # Hold the control until the game reads it, so a press during a capture pause is not lost.
    wait(.6)
    before=logpath.read_text(errors='replace').count(field)
    press();until=time.monotonic()+4
    while time.monotonic()<until and logpath.read_text(errors='replace').count(field)<=before:wait(.05)
    seen=logpath.read_text(errors='replace').count(field)>before
    wait(.2);release();wait(1.2)
    assert seen,('The game never read the control',field.strip())
def trigger(axis):
    # SDL axis 4 is LT, 5 is RT; a virtual trigger rests at -32768 (0 is half pulled).
    held_until_seen(lambda:user.PostMessageW(hwnd,0x8052,axis,32767),lambda:user.PostMessageW(hwnd,0x8052,axis,-32768),
                    ' LT=255' if axis==4 else ' RT=255')
    events.append(dict(elapsed=round(time.monotonic()-began,4),trigger=axis))
def press_a():
    held_until_seen(lambda:button(0,True),lambda:button(0,False),' A=255');wait(4)
def press_x():
    held_until_seen(lambda:button(2,True),lambda:button(2,False),' X=255')
def answer(index):
    # The game does not see A or B while the delete question is open: give it time to read them.
    wait(1);button(index,True);wait(.6);button(index,False);wait(2)
def page_file(page):return run/'saves'/('openshao-save-page-%02d.bin'%page)
def page_slot_used(page,slot):
    data=page_file(page).read_bytes() if page_file(page).exists() else b''
    return len(data)==24+9*0x51AC and data[24+(slot-1)*0x51AC+0x84]==1
def save_file():
    files=sorted((run/'saves/E/UDATA/4d570029').glob('*/save.dat'))
    assert files,'Missing native save file'
    return files[0].read_bytes()
def save_slots(data):return [data[0x131C+i*0x51AC:0x131C+(i+1)*0x51AC] for i in range(9)]
def slot_used(slot):return read(slot_records()[slot-1]+0x84,1)==b'\x01'
def load_selected():
    # Accept the selected main-menu slot and answer Yes to the native load confirmation.
    for _ in range(10):
        if top()!=0x5967C0:break
        pulse(0,duration=.35,gap=4)
    if top()==0x596810:
        confirm_selection=u32(0x596820)+12
        for _ in range(6):
            if u32(confirm_selection)==1:break
            pulse(12,duration=.35,gap=1.4)
        if u32(confirm_selection)!=1:write_memory(confirm_selection,struct.pack('<I',1))
        for _ in range(5):
            if top()!=0x596810:break
            pulse(0,duration=.4,gap=3)
    deadline=time.monotonic()+60
    while top()!=0 and time.monotonic()<deadline:wait(.12)
    assert top()==0,'The selected save did not load'
def enter_load_game():
    navigate(3)
    until=time.monotonic()+30
    while top()==0x598430 and time.monotonic()<until:pulse(0,duration=.4,gap=2)
    assert top()==0x5967C0,'Load Game did not open'
    wait(3)
def write_memory(address,data):
    kernel.WriteProcessMemory.argtypes=[w.HANDLE,c.c_void_p,c.c_void_p,c.c_size_t,c.POINTER(c.c_size_t)]
    buffer=c.create_string_buffer(bytes(data),len(data));written=c.c_size_t()
    assert kernel.WriteProcessMemory(int(process._handle),base+address,buffer,len(data),c.byref(written)) and written.value==len(data),'Memory fixture write failed'
def slot_records():
    buffer=u32(0x59E600)
    assert buffer,'Save buffer is not loaded'
    return [buffer+0x131C+i*0x51AC for i in range(9)]
def slot_variants(prefix):
    # Worst-case slot lists in an isolated process; originals restored afterwards.
    records=slot_records();saved=[read(r,0xE0) for r in records]
    rooms=[0x00,0x20,0x40,0x60,0x67,0x80,0xA0,0xC0,0x1E]
    frames=(99*3600+59*60)*60
    for variant,koop in (('solo',0),('koop',1)):
        for i,r in enumerate(records):
            if read(r+0x84,1)!=b'\x01':continue
            write_memory(r,struct.pack('<I',frames))
            write_memory(r+0x88,struct.pack('<I',koop))
            write_memory(r+0xC8,struct.pack('<I',rooms[i]))
            write_memory(r+0xCC,struct.pack('<II',i%2,(i+1)%2))
            write_memory(r+0xD4,struct.pack('<I',i%4))
            write_memory(r+0xDC,struct.pack('<I',100))
        wait(1);capture(prefix+'-'+variant)
    for r,data in zip(records,saved):write_memory(r,data)
    wait(.5)
def capture(name):
    if name=='main-returning' and args.return_probe_frame is not None:
        deadline=time.monotonic()+4
        while time.monotonic()<deadline:
            scene=u32(0x59843C)
            if top()==0x598430 and scene and u32(scene+0x2A0)==16 and u32(scene+0x33C)<=args.return_probe_frame:break
            tick()
        else:raise AssertionError('Main return clip16 frame was not observed for visual capture')
    if args.survival_quick and name not in ('main-menu','main-row-9','survival-level-1','survival-level-9'):return
    main_scene=u32(0x59843C)
    main_before=dict(state=u32(main_scene+0x2A0),frame=u32(main_scene+0x33C),status=u32(main_scene+0x2E4)) if main_scene else {}
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
        # The log redacts paths, so a finished capture is recognized by its presentation image.
        if len(frames)>old and (frames[-1].parent/'presentation.bmp').is_file():
            world=u32(0x639224);camera=u32(world+0x308) if world else 0
            scale=struct.unpack('<f',read(camera+0x20,4))[0] if camera else None
            assert (frames[-1].parent/'presentation.bmp').is_file(),'Missing actual presentation capture'
            if name=='settings-home':assert top()==0x5985E8,'Settings preview capture must show the actual submenu'
            cursor_state={}
            if 'survival-modes' in name or 'survival-players' in name:
                kontent=u32(0x59847C)
                cursor_state=dict(active=read(0x592D20,1)[0],visible=u32(0x592D20+0xC),clip=u32(0x592D20+0x2A0),frame=u32(0x592D20+0x33C),finished=read(0x592D20+0x340,1)[0],status=u32(0x592D20+0x2E4),flags=u32(0x592D20+0x18),
                                  x=struct.unpack('<f',read(0x4DF4E0,4))[0],y=struct.unpack('<f',read(0x4DF4E4,4))[0],step=struct.unpack('<f',read(0x4DF4DC,4))[0],
                                  menu_finished=read(kontent+0x340,1)[0] if kontent else 0)
                print('CURSOR_STATE',name,json.dumps(cursor_state),flush=True)
            checks.append(dict(capture=name,control=hex(top()),path=str(frames[-1].relative_to(run)),presentation=str((frames[-1].parent/'presentation.bmp').relative_to(run)),camera_scale=scale,room=u32(0x505970),main_before=main_before,settings_movie=settings_movie,cursor_state=cursor_state,
                               backdrops=({hex(o):u32(o+0xC) for o in (0x593A40,0x58E838,0x58F558,0x593068,0x5912E0,0x591970,0x58E4F0)} if args.menu_roundtrip else None)))
            print('CAPTURE',mode,name,'camera',scale,flush=True)
            # Native capture directories use whole-second names; separate requests to prevent overwrite.
            wait(1.1);return
    raise AssertionError('Capture timeout')
def navigate(selection):
    assert top()==0x598430,'Expected main menu'
    for _ in range(12):
        if u32(0x598468)==selection:return
        before=u32(0x598468)
        if args.survival_keyboard_only:assert user.PostMessageW(hwnd,0x100,40,0)
        else:button(12,True)
        for _ in range(45):
            wait(.025)
            if u32(0x598468)!=before:break
        if args.survival_keyboard_only:assert user.PostMessageW(hwnd,0x101,40,0)
        else:button(12,False)
        if args.cursor_stability:
            # Sample the first half second, which the settled screenshot used to skip.
            for elapsed,delta in ((0,0),(.05,.05),(.15,.10),(.30,.15),(.50,.20),(.65,.15)):
                if delta:wait(delta)
                scene=u32(0x59843C)
                sample=dict(row=u32(0x598468),elapsed=elapsed,
                            selector_active=u32(0x592D20+0xC),
                            clip=u32(scene+0x2A0) if scene else None,
                            menu=hex(top()))
                with (run/'cursor-timing.jsonl').open('a',encoding='utf-8') as log:
                    log.write(json.dumps(sample)+'\n')
                assert sample['selector_active']==1 and sample['clip'] in range(1,7), 'Main-menu sparkle disappeared during navigation'
        else:wait(.6)
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

def campaign_roster():
    count = read(0x598C54, 1)[0]
    assert 0 < count <= 8, ('Native roster count', count)
    return list(read(0x598C28, count))

def campaign_unlocks():
    if not args.campaign_saved_unlocks:
        write_memory(0x5A378D, b'\x01\x01')
    story_flags = list(read(0x5A378D, 2))
    expected = ([0, 1, 2] if story_flags[0] == 1 else []) + [3, 4] + ([5, 6, 7] if story_flags[1] == 1 else [])
    checks.append(dict(capture='campaign-unlock-fixture',
                       story_flags=story_flags, expected_campaign_slots=expected))
    return expected

def campaign_select(slot, player, available):
    selected = 0x598E2C + player * 4
    for _ in range(len(available) + 1):
        assert top() == 0x598DF0, ('Unexpected menu during fighter navigation', hex(top()))
        if u32(selected) == slot:
            return
        previous = u32(selected)
        button(14, True, controller=player)
        try:
            deadline = time.monotonic() + 2
            while u32(selected) == previous and time.monotonic() < deadline:
                tick()
        finally:
            button(14, False, controller=player)
        wait(.7)
    raise AssertionError(('Could not select requested fighter', player + 1, slot, u32(selected)))

def campaign_leave_difficulty(back):
    difficulty = 0x596810
    assert top() == difficulty
    deadline = time.monotonic() + 25
    attempts = 0
    while top() == difficulty and time.monotonic() < deadline:
        popup = u32(difficulty + 0x10)
        if not popup or u32(popup + 0x28) != 1:
            wait(.1)
            continue
        attempts += 1
        button(1 if back else 0, True, controller=0)
        try:
            held_until = min(deadline, time.monotonic() + 1.2)
            while top() == difficulty and time.monotonic() < held_until:
                tick()
        finally:
            button(1 if back else 0, False, controller=0)
        wait(.6)
    checks.append(dict(capture='campaign-difficulty-input', back=back,
                       attempts=attempts, destination=hex(top()), virtual_controller=0))
    assert top() != difficulty, ('Native difficulty did not accept input', back, attempts)

def campaign_characters(fighter, partner, check_back):
    characters, difficulty = 0x598DF0, 0x596810
    assert top() == 0x598430 and u32(0x598468) == int(partner is not None)
    available = campaign_unlocks()
    requested = [fighter] if partner is None else [fighter, partner]
    assert all(slot in available for slot in requested), ('Requested slot is locked', requested, available)
    deadline = time.monotonic() + 22
    while top() == 0x598430 and time.monotonic() < deadline:
        pulse(0, duration=.25, gap=2)
    assert top() == characters, ('Campaign Main entry must open the native selector', hex(top()))
    wait(4)

    def confirm_selected(label):
        assert campaign_roster() == available, ('Campaign roster', campaign_roster(), available)
        assert u32(0x598468) == 2, 'Campaign selection must display the eight-slot portrait view'
        for player, slot in enumerate(requested):
            campaign_select(slot, player, available)
        selected = [u32(0x598E2C + player * 4) for player in range(len(requested))]
        assert selected == requested, ('Requested fighter slots changed', selected, requested)
        capture('campaign-characters-' + label)
        checks.append(dict(capture='campaign-exact-slots', phase=label, slots=selected,
                           roster=available, coop=partner is not None))
        for player in range(len(requested)):
            duplicate = player == 1 and fighter == partner
            if duplicate:
                assert [u32(0x598E2C + port * 4) for port in range(2)] == requested, 'Duplicate probe requires the same selected slot'
                campaign_acknowledged_accept(player)
                continue
            for _ in range(6):
                if top() != characters or read(0x598E34 + player, 1) == b'\x01':
                    break
                button(0, True, controller=player)
                try:
                    deadline = time.monotonic() + 1.5
                    while (top() == characters and read(0x598E34 + player, 1) != b'\x01'
                           and time.monotonic() < deadline):
                        tick()
                finally:
                    button(0, False, controller=player)
                wait(.8)
        if fighter == partner:
            wait(4)
            assert top() == characters and read(0x598E34, 2) == b'\x01\x00', (
                'Native Ko-op must reject P2 confirming P1’s locked fighter', hex(top()),
                list(read(0x598E34, 2)))
            capture('campaign-duplicate-rejected')
            checks.append(dict(capture='campaign-native-duplicate-rejection', slot=fighter))
            return
        deadline = time.monotonic() + 22
        while top() == characters and time.monotonic() < deadline:
            wait(.1)
        assert top() == difficulty, ('Exact fighter selection must open difficulty', hex(top()),
                                     list(read(0x598E34, 2)))
        assert u32(0x598468) == 2 and campaign_roster() == available, 'Difficulty changed the roster backdrop'
        capture('campaign-difficulty-' + label)

    confirm_selected('initial')
    if check_back:
        campaign_leave_difficulty(back=True)
        assert top() == characters, ('Difficulty Back must return to the roster', hex(top()))
        returned = [u32(0x598E2C + player * 4) for player in range(len(requested))]
        assert returned == requested, ('Difficulty Back changed the selected fighters', requested, returned)
        capture('campaign-characters-after-back')
        confirm_selected('reconfirmed')

def campaign_start(fighter, partner):
    expected = [(83, 89, 88, 1, 0, 82, 90, 84)[slot]
                for slot in (fighter, partner) if slot is not None]
    coop = partner is not None
    campaign_leave_difficulty(back=False)

    actors = [0x5CE880 + (player + 1) * 0x3238 for player in range(len(expected))]
    deadline = time.monotonic() + 135
    next_skip = time.monotonic() + 9
    initial_play_frames = None
    while time.monotonic() < deadline:
        state = u32(0x5ACEBC)
        if state and u32(0x505970) == 96 and initial_play_frames is None:
            initial_play_frames = u32(state)
        components = [u32(actor + 0x200) for actor in actors]
        if (state and u32(0x505970) == 96 and u32(0x639224)
                and all(component and u32(component + 0x370) for component in components)
                and read(0x4EF42C, 1) == b'\x01'
                and read(0x6C4198, 1) == b'\x00' and not u32(0x6C92F8)
                and initial_play_frames is not None and u32(state) > initial_play_frames):
            break
        if time.monotonic() >= next_skip:
            pulse(6, duration=.35, gap=.8)
            next_skip = time.monotonic() + 10
        else:
            wait(.1)
    else:
        raise AssertionError(('Normal campaign room did not become active', u32(0x505970), hex(top())))

    heroes = []
    for player, (actor, component) in enumerate(zip(actors, components)):
        heroes.append(dict(slot=(fighter, partner)[player],
                           saved_fighter=u32(state + 0xCC + 4 * player),
                           actor_fighter=struct.unpack('<h', read(actor + 0x1DFE, 2))[0],
                           health=u32(component + 0x368), maximum_health=u32(component + 0x370),
                           experience=u32(state + 0x48F0 + (player + 1) * 0x50),
                           ranks=list(read(state + 0x1C4 + player * 0x1C, 28))))
    snapshot = dict(capture='campaign-room-96-start', room=u32(0x505970), mode=u32(state + 0x88),
                    health_pickups=u32(state + 0xE0), health_tier=u32(state + 0x90),
                    base_health=u32(state + 0x94), heroes=heroes,
                    cinematic=read(0x6C4198, 1)[0], movie_loading=u32(0x6C92F8),
                    initial_play_frames=initial_play_frames, play_frames=u32(state))
    checks.append(snapshot)
    (run/'campaign-start.json').write_text(json.dumps(snapshot, indent=2))
    assert snapshot['mode'] == int(coop), ('Native campaign mode', snapshot)
    assert snapshot['health_pickups'] == 0 and snapshot['health_tier'] == 1, ('Campaign starting health progression', snapshot)
    assert snapshot['base_health'] == u32(0x50B544 + 4 * int(coop)), ('Native zero-pickup health table', snapshot)
    for hero, identity in zip(heroes, expected):
        assert hero['saved_fighter'] == hero['actor_fighter'] == identity, ('Campaign fighter identity', hero)
        assert 0 < hero['health'] <= hero['maximum_health'] == snapshot['base_health'] // len(expected), (
            'Native campaign hero health', hero)
        assert hero['ranks'][4:11] == [0] * 7, ('Campaign platform skills already acquired', hero)
        if identity in (83, 84, 89, 90):
            assert hero['ranks'][:4] == [1] * 4, ('Extra fighter inherited another fighter’s starting ranks', hero)
    native_log = logpath.read_text(errors='replace')
    assert 'Survival session started' not in native_log and 'Survival rules ready' not in native_log
    assert u32(0x4E6E20 + 0x6C0 + 0x18) != 0xFFFFFD20, 'Survival rules were posted in normal campaign'
    capture('campaign-room-96')

def campaign_move_menus():
    active, tab, player_owner, selected = 0x59E3D4, 0x4EF410, 0x4EFA60, 0x4EFB38
    extra_labels = {83: 545, 84: 550, 89: 560, 90: 565}
    state = u32(0x5ACEBC)
    identities = [u32(state + 0xCC + player * 4) for player in range(1 + u32(state + 0x88))]
    def progression():
        return [(u32(state + 0x48F0 + (player + 1) * 0x50),
                 list(read(state + 0x1C4 + player * 0x1C, 28)))
                for player in range(len(identities))]

    def drive(index, player, reached, *, axis=False):
        def set_input(held):
            if axis:
                assert user.PostMessageW(hwnd, 0x8052, index | (player << 16), 32767 if held else -32768)
            else:
                button(index, held, controller=player)

        for _ in range(6):
            if reached():
                return
            set_input(True)
            try:
                deadline = time.monotonic() + 1.2
                while not reached() and time.monotonic() < deadline:
                    tick()
            finally:
                set_input(False)
            wait(.7)
        assert reached(), ('Native Moves input did not reach its destination', player + 1, index)

    for player, identity in enumerate(identities):
        assert not u32(active), 'A previous Moves menu remained open'
        before = progression()
        deadline = time.monotonic() + 100
        while time.monotonic() < deadline and not u32(active):
            # The first-room scene and active native tutorial messages can consume Start.
            if not u32(0x6C92F8) and not u32(0x632F90):
                pulse(6, duration=.25, gap=2, controller=player)
            else:
                wait(.15)
        assert u32(active), ('Start must open the native Moves menu', player + 1)
        # Start opens Main. The native tab-switch callback sets the Moves state to 1.
        drive(5, player, lambda: u32(tab) == 1, axis=True)
        deadline = time.monotonic() + 12
        while not u32(selected) and time.monotonic() < deadline:
            wait(.15)
        wait(1)
        category = u32(selected)
        assert category and u32(player_owner) == player, ('Native Moves owner', u32(player_owner), player)
        root_node = u32(category + 0x10)
        capture(f'campaign-moves-p{player + 1}-root')
        root_identity = u32(root_node + 0x18) if root_node else None
        checks.append(dict(capture='campaign-moves-root', player=player + 1,
                           expected_identity=identity, root_identity=root_identity,
                           selected_label=u32(category + 0x18)))
        assert root_identity == identity, ('Native Moves selected another fighter’s root', identity, root_identity)
        if identity in extra_labels:
            assert u32(category + 0x18) == 461, 'Extra fighter must open on Specials'
            drive(0, player, lambda: u32(selected) != category)
            for column in range(3):
                button_node = u32(selected)
                assert u32(button_node + 0x10) == category and u32(button_node + 0x18) == 466 + column
                drive(0, player, lambda: u32(selected) != button_node)
                leaf = u32(selected)
                label = extra_labels[identity] + column * 100
                capture(f'campaign-moves-p{player + 1}-special-{column + 1}')
                sample = dict(capture='campaign-moves-special', player=player + 1, fighter=identity,
                              label=u32(leaf + 0x18), description=u32(leaf + 0x1C),
                              cost=u32(leaf + 0x28), acquired=u32(leaf + 0x2C),
                              preview_request=u32(0x59E38C), preview_active=u32(0x59E3BC))
                checks.append(sample)
                assert u32(leaf + 0x10) == button_node and u32(leaf + 4) == 0, ('Native reference leaf', sample)
                assert (sample['label'], sample['description']) == (label, label + 50), ('Native move text', sample)
                assert (sample['cost'], sample['acquired']) == (0, 1), ('Reference cost or acquisition state', sample)
                assert not (sample['preview_request'] or sample['preview_active']), ('Unexpected reference preview', sample)
                campaign_acknowledged_accept(player)
                assert u32(selected) == leaf and progression() == before, ('Reference technique accepted a purchase', sample)
                for variant in range(1, u32(button_node + 0xC)):
                    previous = u32(selected)
                    drive(12, player, lambda: u32(selected) != previous)
                    node = u32(selected)
                    assert u32(node + 0x10) == button_node and u32(node + 4) == 0
                    assert u32(node + 0x28) == 0 and u32(node + 0x2C) == 1
                    assert not (u32(0x59E38C) or u32(0x59E3BC)), 'Unexpected variant preview'
                    capture(f'campaign-moves-p{player + 1}-special-{column + 1}-variant-{variant}')
                    campaign_acknowledged_accept(player)
                    assert u32(selected) == node and progression() == before
                    checks.append(dict(capture='campaign-moves-reference', player=player + 1,
                                       fighter=identity, section='special-variant', label=u32(node + 0x18),
                                       description=u32(node + 0x1C)))
                drive(1, player, lambda: u32(selected) == button_node)
                if column < 2:
                    drive(12, player, lambda: u32(selected) != button_node)
            drive(1, player, lambda: u32(selected) == category)
            def inspect_references(parent, path):
                count = u32(parent + 0xC)
                assert 0 < count <= 7, ('Unexpected reference section size', path, count)
                drive(0, player, lambda: u32(selected) != parent)
                for row in range(count):
                    node = u32(selected)
                    label = u32(node + 0x18)
                    assert u32(node + 0x10) == parent, ('Incorrect reference parent', path, row)
                    if u32(node + 0xC):
                        inspect_references(node, f'{path}-{row + 1}')
                    else:
                        description = u32(node + 0x1C)
                        assert label != 0xFFFFFFFF and description != 0xFFFFFFFF
                        assert u32(node + 0x28) == 0 and u32(node + 0x2C) == 1
                        assert not (u32(0x59E38C) or u32(0x59E3BC)), ('Unexpected reference preview', path, row)
                        capture(f'campaign-moves-p{player + 1}-{path}-{row + 1}')
                        campaign_acknowledged_accept(player)
                        assert u32(selected) == node and progression() == before
                        checks.append(dict(capture='campaign-moves-reference', player=player + 1,
                                           fighter=identity, section=path, label=label, description=description))
                    if row + 1 < count:
                        drive(12, player, lambda: u32(selected) != node)
                drive(1, player, lambda: u32(selected) == parent)

            drive(0, player, lambda: u32(selected) != category)
            throw_group = u32(u32(category + 0x14) + 12)
            for _ in range(3):
                previous = u32(selected)
                drive(12, player, lambda: u32(selected) != previous)
            assert u32(selected) == throw_group and u32(throw_group + 0x18) == 469
            inspect_references(throw_group, 'throws')
            drive(1, player, lambda: u32(selected) == category)
            for section_label, section_name in ((402, 'basic'), (463, 'combos'), (400, 'fatalities'), (462, 'platform')):
                previous = u32(selected)
                drive(12, player, lambda: u32(selected) != previous)
                section = u32(selected)
                assert u32(section + 0x18) == section_label
                inspect_references(section, section_name)
        drive(6, player, lambda: not u32(active))
        assert progression() == before, ('Moves menu navigation changed XP or ranks', player + 1)

def campaign_save_roundtrip(fighter, partner):
    identities = [(83, 89, 88, 1, 0, 82, 90, 84)[slot]
                  for slot in (fighter, partner) if slot is not None]
    source_record = read(slot_records()[args.slot - 1], 0x51AC)
    saved_room = struct.unpack_from('<I', source_record, 0xC8)[0]

    def drive(index, reached, seconds=20, controller=0):
        deadline = time.monotonic() + seconds
        while not reached() and time.monotonic() < deadline:
            button(index, True, controller=controller)
            try:
                held_deadline = min(deadline, time.monotonic() + 1.5)
                while not reached() and time.monotonic() < held_deadline:
                    tick()
            finally:
                button(index, False, controller=controller)
            wait(.7)
        assert reached(), ('Campaign save fixture input timed out', index, hex(top()))

    def snapshot():
        state = u32(0x5ACEBC)
        heroes = []
        for player in range(len(identities)):
            actor = 0x5CE880 + (player + 1) * 0x3238
            component = u32(actor + 0x200)
            heroes.append(dict(saved_fighter=u32(state + 0xCC + player * 4),
                               actor_fighter=struct.unpack('<h', read(actor + 0x1DFE, 2))[0],
                               experience=u32(state + 0x4940 + player * 0x50),
                               ranks=list(read(state + 0x1C4 + player * 28, 28)),
                               health=u32(component + 0x368), maximum_health=u32(component + 0x370)))
        return dict(room=u32(0x505970), mode=u32(state + 0x88),
                    health_pickups=u32(state + 0xE0), heroes=heroes)

    def load():
        saved_frames = u32(slot_records()[0])
        drive(0, lambda: top() != 0x5967C0)
        deadline = time.monotonic() + 35
        while (top() == 0x596810 and
               (not u32(0x596820) or u32(u32(0x596820) + 0x28) != 1)
               and time.monotonic() < deadline):
            wait(.1)
        if len(identities) == 2:
            assert top() == 0x596810 and u32(0x59684C) == 6, 'Ko-op load must request the second player'
            drive(6, lambda: u32(0x59684C) != 6, controller=1)
        assert top() == 0x596810 and u32(0x59684C) == 1, 'Native load confirmation must open'
        selection = u32(0x596820) + 12
        drive(12, lambda: u32(selection) == 1)
        drive(0, lambda: top() != 0x596810)
        deadline = time.monotonic() + 150
        while time.monotonic() < deadline:
            if (top() == 0 and u32(0x505970) == saved_room and u32(0x639224)
                    and all(u32(0x5CE880 + (i + 1) * 0x3238 + 0x200) for i in range(len(identities)))
                    and read(0x4EF42C, 1) == b'\x01' and not u32(0x6C92F8)
                    and read(0x6C4198, 1) == b'\x00'
                    and u32(u32(0x5ACEBC)) > saved_frames):
                wait(3)
                return
            wait(.15)
        capture('campaign-save-load-timeout')
        raise AssertionError(('Totem fixture did not load', hex(top()), u32(0x505970),
                              saved_frames, u32(u32(0x5ACEBC)), u32(0x6C92F8)))

    wait(3)
    record = slot_records()[0]
    assert source_record[0x84] == 1, 'The requested source slot must contain a save'
    if args.slot != 1:
        write_memory(record, source_record)
    original_record = read(record, 0x51AC)
    # Prepares only the child's copied record: proves serialization, not progression.
    write_memory(record + 0x88, struct.pack('<I', len(identities) - 1))
    fixture_heroes = []
    for player, identity in enumerate(identities):
        write_memory(record + 0xCC + player * 4, struct.pack('<I', identity))
        ranks = bytearray(original_record[0x1C4 + player * 28:0x1E0 + player * 28])
        ranks[:4] = bytes([2, 3, 4, 2])
        write_memory(record + 0x1C4 + player * 28, ranks)
        write_memory(record + 0x4940 + player * 0x50, struct.pack('<I', 1234 + player * 1111))
        fixture_heroes.append(dict(identity=identity, experience=1234 + player * 1111, ranks=list(ranks)))
    checks.append(dict(capture='campaign-save-injected-fixture', source_slot=args.slot, copied_slot=1, room=saved_room,
                       identities=identities, injected_fields=['mode', 'fighter IDs', 'base special ranks', 'XP'],
                       heroes=fixture_heroes, native_acquisition=False, ordinary_traversal=False))
    capture('campaign-save-injected-load-list')
    load()
    before = snapshot()
    (run/'campaign-save-before.json').write_text(json.dumps(before, indent=2))
    capture('campaign-save-loaded-at-totem')
    assert [hero['saved_fighter'] for hero in before['heroes']] == identities, before
    assert [hero['actor_fighter'] for hero in before['heroes']] == identities, before
    assert before['mode'] == len(identities) - 1, before
    assert before['health_pickups'] == struct.unpack_from('<I', original_record, 0xE0)[0], before
    for hero, fixture_hero in zip(before['heroes'], fixture_heroes):
        assert hero['ranks'] == fixture_hero['ranks'] and hero['experience'] == fixture_hero['experience'], (
            'Initial native load changed fixture progression', hero, fixture_hero)
    # The original loaded position is in reach of its save totem. Native RB opens it.
    drive(10, lambda: bool(u32(0x59E600)), seconds=30)
    wait(3)
    capture('campaign-save-totem-list')
    trigger(5)
    assert not slot_used(1) and not page_file(2).exists(), 'The isolated destination page must start empty'
    capture('campaign-save-empty-page-2')

    saved_record = None

    def save_record_ready():
        nonlocal saved_record
        try:
            page = page_file(2).read_bytes()
        except (FileNotFoundError, PermissionError):
            # The native write can briefly replace or hold the destination open.
            return False
        if len(page) != 24 + 9 * 0x51AC or page[24 + 0x84] != 1:
            return False
        saved_record = page[24:24 + 0x51AC]
        return True

    drive(0, save_record_ready, seconds=30)
    wait(3)
    capture('campaign-save-written-page-2')
    for player, identity in enumerate(identities):
        assert struct.unpack_from('<I', saved_record, 0xCC + player * 4)[0] == identity
        assert struct.unpack_from('<I', saved_record, 0x4940 + player * 0x50)[0] == before['heroes'][player]['experience']
        assert list(saved_record[0x1C4 + player * 28:0x1E0 + player * 28]) == before['heroes'][player]['ranks']
    pulse(1, duration=.35, gap=3)
    old_count = logpath.read_text(errors='replace').count('Gameplay pause expanded:')
    drive(4, lambda: logpath.read_text(errors='replace').count('Gameplay pause expanded:') > old_count)
    panel = int(re.findall(r'Gameplay pause expanded: list=([0-9A-Fa-f]{8})',
                          logpath.read_text(errors='replace'))[-1], 16)
    drive(12, lambda: u32(panel + 0x6C) == 4)
    capture('campaign-save-exit-selected')
    # Exit Game (0xA28E0) sets this mask during its Yes/No scene; the frontend stack stays empty.
    drive(0, lambda: u32(0x4EFA50) == 3)
    scene_index = u32(0x59E358)
    assert scene_index < 8, ('Native Exit Game scene', scene_index)
    scene = u32(0x4EF3C8 + 0x668 + scene_index * 4)
    deadline = time.monotonic() + 12
    question_list = 0
    while scene and u32(0x4EFA50) == 3 and time.monotonic() < deadline:
        question_list = u32(scene + 0x48)
        if question_list and read(scene + 0xC, 1) == b'\x01' and u32(question_list + 0x6C) == 1:
            break
        wait(.1)
    else:
        raise AssertionError(('Native Exit Game Yes/No list was not ready', scene_index, hex(scene), hex(question_list)))
    capture('campaign-save-exit-question')
    drive(13, lambda: u32(question_list + 0x6C) == 0)
    drive(0, lambda: u32(0x4EFA50) != 3)
    deadline = time.monotonic() + 45
    while top() != 0x598430 and time.monotonic() < deadline:
        wait(.1)
    assert top() == 0x598430, 'Native Exit Game did not return to Main'
    enter_load_game()
    assert slot_used(1), 'The newly saved page must remain selected after returning to Main'
    assert read(slot_records()[0], 0x51AC) == saved_record, 'The Load list must read the newly serialized record'
    capture('campaign-save-reload-list')
    load()
    after = snapshot()
    capture('campaign-save-reloaded-at-totem')
    for snapshot_value in (before, after):
        for hero in snapshot_value['heroes']:
            assert 0 < hero['health'] <= hero['maximum_health'], snapshot_value
    # Current health can be restored by a save totem; persistent progression is exact.
    persistent = lambda value: dict(room=value['room'], mode=value['mode'], health_pickups=value['health_pickups'],
                                   heroes=[{key: hero[key] for key in ('saved_fighter', 'actor_fighter', 'experience', 'ranks', 'maximum_health')}
                                           for hero in value['heroes']])
    assert persistent(before) == persistent(after), ('Campaign serialization changed progression', before, after)
    result = dict(capture='campaign-save-roundtrip', fixture_injected=True, native_save_ui=True,
                  native_load_ui=True, page=2, slot=1, before=before, after=after,
                  ordinary_traversal_verified=False, death_continue_verified=False)
    checks.append(result)
    (run/'campaign-save-roundtrip.json').write_text(json.dumps(result, indent=2))

def campaign_death_continue():
    state = u32(0x5ACEBC)
    count = 1 + u32(state + 0x88)
    assert count in (1, 2) and top() == 0, 'The diagnostic requires active Solo or Ko-op gameplay'
    checkpoint = read(state + 0x51AC, 0x51AC)

    def record_progression(record):
        number = 1 + struct.unpack_from('<I', record, 0x88)[0]
        assert number in (1, 2), 'The native checkpoint must contain a campaign record'
        return dict(room=struct.unpack_from('<I', record, 0xC8)[0], mode=number - 1,
                    health_pickups=struct.unpack_from('<I', record, 0xE0)[0],
                    heroes=[dict(identity=struct.unpack_from('<I', record, 0xCC + player * 4)[0],
                                 experience=struct.unpack_from('<I', record, 0x4940 + player * 0x50)[0],
                                 ranks=list(record[0x1C4 + player * 28:0x1E0 + player * 28]))
                            for player in range(number)])

    expected = record_progression(checkpoint)
    before = record_progression(read(state, 0x51AC))
    actors = [0x5CE880 + (player + 1) * 0x3238 for player in range(count)]
    components = [u32(actor + 0x200) for actor in actors]
    assert all(components), 'Native actor components must exist before the death diagnostic'
    checks.append(dict(capture='campaign-death-injected-hp', live_hp_injected=0,
                       natural_damage_verified=False, before=before, checkpoint=expected))
    capture('campaign-death-before-hp-fixture')
    for component in components:
        write_memory(component + 0x368, struct.pack('<I', 0))

    # Death calls 0x38CB0 (random text EF..121, kind 3 Continue/Quit); 0x85300 handles A.
    message = 0x4E6E20 + 0x6C0
    deadline = time.monotonic() + 80
    observed_death = False
    samples = []
    previous = None
    while time.monotonic() < deadline:
        manager = u32(0x589438)
        sample = dict(actor_states=[u32(actor + 0x1E04) for actor in actors],
                      phases=[u32(manager + 0x10 + player * 12) for player in range(count)] if manager else [],
                      message_active=read(message, 1)[0], message_kind=u32(message + 0x14),
                      message_text=u32(message + 0x18), accepts_a=read(message + 0x29, 1)[0])
        if sample != previous:
            samples.append(sample)
            previous = sample
        observed_death |= any(value == 12 for value in sample['actor_states']) or any(sample['phases'])
        if (observed_death and sample['message_active'] == sample['accepts_a'] == 1
                and sample['message_kind'] == 3 and 0xEF <= sample['message_text'] < 0x122):
            break
        wait(.05)
    else:
        capture('campaign-death-prompt-timeout')
        (run/'campaign-death-samples.json').write_text(json.dumps(samples, indent=2))
        raise AssertionError(('Native Continue prompt was not reached after HP 0', samples[-1:]))
    capture('campaign-death-native-continue-prompt')
    # The original prompt defaults to Continue (1); Up selects it if needed.
    deadline = time.monotonic() + 15
    while u32(0x59E09C) != 1 and time.monotonic() < deadline:
        pulse(11, duration=.4, gap=.7)
    assert u32(0x59E09C) == 1, 'Native Continue must be selected before accepting'
    deadline = time.monotonic() + 20
    while read(message, 1) == b'\x01' and u32(0x4E6E20 + 0x6A8) != 2 and time.monotonic() < deadline:
        button(0, True)
        try:
            held_deadline = min(deadline, time.monotonic() + 1.5)
            while (read(message, 1) == b'\x01' and u32(0x4E6E20 + 0x6A8) != 2
                   and time.monotonic() < held_deadline):
                tick()
        finally:
            button(0, False)
        wait(.7)
    assert read(message, 1) == b'\x00' or u32(0x4E6E20 + 0x6A8) == 2, 'Native Continue did not accept A'
    deadline = time.monotonic() + 150
    saved_frames = struct.unpack_from('<I', checkpoint)[0]
    while time.monotonic() < deadline:
        state = u32(0x5ACEBC)
        components = [u32(actor + 0x200) for actor in actors]
        if (top() == 0 and u32(0x505970) == expected['room'] and u32(state) > saved_frames
                and not u32(0x6C92F8) and read(0x6C4198, 1) == b'\x00'
                and read(0x4EF42C, 1) == b'\x01' and read(message, 1) == b'\x00'
                and all(component and u32(component + 0x368) > 0 for component in components)):
            break
        wait(.1)
    else:
        capture('campaign-continue-restore-timeout')
        raise AssertionError('Native Continue did not restore active checkpoint gameplay')
    after = record_progression(read(state, 0x51AC))
    capture('campaign-death-native-continue-restored')
    assert after == expected, ('Native Continue changed checkpoint progression', expected, after)
    assert [struct.unpack('<h', read(actor + 0x1DFE, 2))[0] for actor in actors] == [
        hero['identity'] for hero in expected['heroes']], 'Native Continue restored incorrect actor identities'
    result = dict(capture='campaign-death-continue', live_hp_injected=True,
                  native_prompt=True, native_checkpoint_restored=True, before=before,
                  checkpoint=expected, after=after, samples=samples,
                  health=[dict(current=u32(component + 0x368), maximum=u32(component + 0x370))
                          for component in components])
    checks.append(result)
    (run/'campaign-death-continue.json').write_text(json.dumps(result, indent=2))

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
    settings_idle()
    log_before=logpath.read_text(errors='replace');segment_before=len(log_before)
    assert user.PostMessageW(hwnd,0x100,13,0)
    time.sleep(.15);user.PostMessageW(hwnd,0x101,13,0)
    if not change:
        wait(4);assert process.pid==old.pid and process.poll() is None
        assert 'Language restart prepared:' not in logpath.read_text(errors='replace')[segment_before:]
        assert tomllib.loads((data/'settings.toml').read_text(encoding='utf8'))['language']==target
        key(27);key(27);wait(4);assert top()==0x598430
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
    brightness=struct.unpack('<f',read(0x4DF5C0,4))[0]
    rendered=struct.unpack('<f',read(0x632BE8,4))[0]
    multiplier=struct.unpack('<f',read(0x49E580,4))[0]
    assert abs(rendered-brightness*multiplier)<.001,('Restart brightness was not applied to rendering',brightness,rendered,multiplier)
    checks.append(dict(capture='restart-brightness',language=target,saved=brightness,rendered=rendered,multiplier=multiplier))
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
    assert not re.search(r'files: open .*MOVIES.*(?:MIDWAY|OPENING)',current_log,re.I),'Restart must not open the logo or opening movie'
    checks.append(dict(capture='language-process-restart',voice_language=saved['voice_language'],language=target,old_pid=old.pid,new_pid=process.pid))
    locale=target
    navigate(4);pulse(0,duration=.3,gap=4)
    assert top()==0x5985E8 and u32(0x598620)==0,'Native Options must open on Brightness'
    pulse(0,duration=.3,gap=4)
    assert top()==0x5982C8,'The native brightness calibration did not open after restart'
    draft=struct.unpack('<f',read(0x598308,4))[0]
    assert abs(draft-brightness)<.001,('Restart calibration slider lost the saved brightness',brightness,draft)
    capture('restart-brightness-slider-'+target)
    pulse(14 if brightness<.95 else 13,duration=.15,gap=1)
    assert abs(struct.unpack('<f',read(0x598308,4))[0]-brightness)>.001,'The restarted calibration slider must remain adjustable'
    pulse(1,duration=.3,gap=4)
    assert abs(struct.unpack('<f',read(0x4DF5C0,4))[0]-brightness)<.001,'Cancelling calibration after restart must preserve saved brightness'
    pulse(0,duration=.3,gap=4)
    assert abs(struct.unpack('<f',read(0x598308,4))[0]-brightness)<.001,'Reopening calibration after cancellation must retain saved brightness'
    pulse(1,duration=.3,gap=4);pulse(1,duration=.3,gap=4)
    assert top()==0x598430,'Calibration cancellation must return to Main'
    checks.append(dict(capture='restart-brightness-slider',language=target,saved=brightness,opened=draft,cancellation_preserved=True))

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
            if action[0]=='campaign-characters':
                campaign_characters(*action[1:])
            elif action[0]=='campaign-start':
                campaign_start(*action[1:])
            elif action[0]=='campaign-move-menus':
                campaign_move_menus()
            elif action[0]=='campaign-save-roundtrip':
                campaign_save_roundtrip(*action[1:])
            elif action[0]=='campaign-death-continue':
                campaign_death_continue()
            elif action[0]=='survival':
                def survival_accept(done,menu=False):
                    deadline=time.monotonic()+16
                    attempts=0
                    while not done() and time.monotonic()<deadline:
                        keycode=13 if menu and args.survival_keyboard_enter else 32
                        assert user.PostMessageW(hwnd,0x100,keycode,0)
                        held=time.monotonic()+(.15 if keycode==13 or (menu and args.survival_short_space) else 2)
                        events.append(dict(elapsed=round(time.monotonic()-began,4),key=keycode,held=True))
                        while (not done() or (menu and args.survival_short_space)) and time.monotonic()<held:wait(.01)
                        assert user.PostMessageW(hwnd,0x101,keycode,0)
                        events.append(dict(elapsed=round(time.monotonic()-began,4),key=keycode,held=False))
                        # Frame-audit runs snapshot right away; a long sleep hid entry-frame bugs.
                        wait(.03 if args.survival_frame_audit else .7)
                        attempts+=1
                        if menu and args.survival_short_space:
                            wait(3)
                            break
                    if not done():
                        quick=args.survival_quick;args.survival_quick=False
                        capture('survival-input-failed');args.survival_quick=quick
                        checks.append(dict(capture='survival-input-failed-state',main_port=u32(0x598460),main_p2=u32(0x598464),kontent_port=u32(0x5984A0),kontent_p2=u32(0x5984A4),character_port=u32(0x598E20),character_p2=u32(0x598E24)))
                    assert done(),('The native Survival menu did not accept the input',keycode,attempts,hex(top()),u32(0x598468))
                if args.survival_keyboard_only:
                    assert user.PostMessageW(hwnd,0x8053,0,0);wait(1)
                navigate(9);capture('survival-main-menu')
                before=logpath.read_text(errors='replace').count('Survival menu: modes')
                survival_accept(lambda:logpath.read_text(errors='replace').count('Survival menu: modes')>before,menu=True)
                deadline=time.monotonic()+16
                while time.monotonic()<deadline:
                    scene=u32(0x59843C)
                    if top()==0x598430 and scene and read(scene+0x340,1)==b'\x01':break
                    wait(.05)
                assert top()==0x598430,'Game Mode must stay on the main menu screen'
                capture('survival-modes')
                if args.survival_locked:
                    before_players=logpath.read_text(errors='replace').count('Survival menu: players')
                    key(32);wait(2)
                    assert top()==0x598430 and u32(0x598468)==0,\
                        'Locked Survival must remain on the Main-backed Modes screen after Accept'
                    assert logpath.read_text(errors='replace').count('Survival menu: players')==before_players,\
                        'Locked Survival must not open Players, characters or difficulty'
                    capture('survival-locked-after-accept')
                    checks.append(dict(capture='survival-global-lock',blocked=True,main_page=hex(top()),selected=u32(0x598468)))
                    quit=True
                    break
                if args.survival_capture_cursor:
                    capture('survival-modes-settled')
                    capture('survival-modes-later')
                if args.survival_idle_seconds:
                    attract_before=logpath.read_text(errors='replace').count('FRONT/MOVIES/OPENING.SFD')
                    wait(args.survival_idle_seconds)
                    assert top()==0x598430 and u32(0x598468)==0,'Modes idle must not return to Main or start attract presentation'
                    assert logpath.read_text(errors='replace').count('FRONT/MOVIES/OPENING.SFD')==attract_before,'Modes idle unexpectedly opened native attract/Opening movie'
                    assert u32(0x592D20+0xC)==1,'Modes idle must retain the native active selection sparkle'
                    capture('survival-modes-idle')
                    checks.append(dict(capture='survival-modes-idle-check',seconds=args.survival_idle_seconds,main_selection=u32(0x598468)))
                wait(1)
                before=logpath.read_text(errors='replace').count('Survival menu: players')
                survival_accept(lambda:logpath.read_text(errors='replace').count('Survival menu: players')>before,menu=True)
                wait(2)
                if args.survival_koop:key(40)
                wait(2)
                assert u32(0x598468)==int(args.survival_koop),'Survival player count must remain selected'
                capture('survival-players')
                if args.survival_capture_cursor:
                    capture('survival-players-settled')
                    capture('survival-players-later')
                if args.survival_idle_seconds:
                    attract_before=logpath.read_text(errors='replace').count('FRONT/MOVIES/OPENING.SFD')
                    wait(args.survival_idle_seconds)
                    assert top()==0x598430 and u32(0x598468)==int(args.survival_koop),'Players idle must preserve its selection and suppress attract presentation'
                    assert logpath.read_text(errors='replace').count('FRONT/MOVIES/OPENING.SFD')==attract_before,'Players idle unexpectedly opened native attract/Opening movie'
                    assert u32(0x592D20+0xC)==1,'Players idle must retain the native active selection sparkle'
                    capture('survival-players-idle')
                    checks.append(dict(capture='survival-players-idle-check',seconds=args.survival_idle_seconds,main_selection=u32(0x598468)))
                if args.survival_capture_variants and not args.survival_koop:
                    key(40);wait(3)
                    assert u32(0x598468)==1,'Players focus must move to Ko-op'
                    capture('survival-players-koop')
                    key(38);wait(3)
                    assert u32(0x598468)==0,'Players focus must return to Solo'
                if args.survival_menu_only:
                    checks.append(dict(capture='survival-menu-preview',modes=True,players=True))
                    quit=True
                    break
                if args.survival_menu_back:
                    stopped=logpath.read_text(errors='replace').count('Survival announcement stopped')
                    modes_before=logpath.read_text(errors='replace').count('Survival menu: modes')
                    key(27);wait(3)
                    assert logpath.read_text(errors='replace').count('Survival menu: modes')>modes_before,'Players Back must return to Modes'
                    assert logpath.read_text(errors='replace').count('Survival announcement stopped')>stopped,'Backing out must stop the owned announcement'
                    key(27);wait(3)
                    assert top()==0x598430 and u32(0x598468)==9,'Modes Back must restore the Game Mode main row'
                    modes_before=logpath.read_text(errors='replace').count('Survival menu: modes')
                    survival_accept(lambda:logpath.read_text(errors='replace').count('Survival menu: modes')>modes_before,menu=True)
                    wait(3)
                    players_before=logpath.read_text(errors='replace').count('Survival menu: players')
                    survival_accept(lambda:logpath.read_text(errors='replace').count('Survival menu: players')>players_before,menu=True)
                    if args.survival_koop:key(40)
                    wait(2)
                    assert u32(0x598468)==int(args.survival_koop),'Player selection must survive menu cancellation and reentry'
                    checks.append(dict(capture='survival-menu-back',announcement_stopped=True,reentered=True))
                survival_accept(lambda:top()==0x598DF0,menu=True)
                if args.survival_frame_audit:
                    capture('survival-characters-immediate')
                    capture('survival-characters-next')
                wait(4);assert top()==0x598DF0,('Survival must enter character selection',hex(top()))
                assert u32(0x598468)==2,'The complete Versus character screen must be displayed'
                available=list(read(0x598C28,read(0x598C54,1)[0]))
                # The Versus roster comes from saved progress bits, not a fixed list.
                def versus_unlocked(fighter):
                    key=u32(0x4E5F98+fighter*12)
                    return key<0x34*8 and bool(u32(0x5A375C+(key>>5)*4)&u32(0x4FC348+(key&31)*4))
                expected=[fighter for fighter in range(8) if versus_unlocked(fighter)]
                assert available==expected,('Survival must match the unlocked fighters in the native Versus save exactly',available,expected)
                assert available,'The native profile must have at least one selectable Versus fighter'
                for fighter in range(8):
                    if not versus_unlocked(fighter):
                        assert u32(0x598DF0+0x48+fighter*12)==0,('Locked fighter remains selectable in Survival',fighter)
                selected_fighter=args.survival_fighter if args.survival_fighter in available else available[0]
                for _ in range(len(available)):
                    if u32(0x598E2C)==selected_fighter:break
                    key(39)
                assert u32(0x598E2C)==selected_fighter,('Cannot select an unlocked Versus fighter in Survival',available,u32(0x598E2C))
                capture('survival-characters')
                if args.survival_characters_only:
                    states=[u32(0x598DF0+0x48+i*12) for i in range(8)]
                    checks.append(dict(capture='survival-roster-review',available=available,selected=u32(0x598DF0+0x3C),secondary=u32(0x598DF0+0x40),statuses=states,solo=not args.survival_koop))
                    quit=True
                    break
                deadline=time.monotonic()+35
                while top()==0x598DF0 and time.monotonic()<deadline:
                    key(32)
                    if args.survival_koop:pulse(6,duration=.2,gap=.8);pulse(0,duration=.2,gap=.8)
                assert top()==0x596810,('Character confirmation must open native difficulty',hex(top()))
                if args.survival_frame_audit:capture('survival-difficulty-immediate')
                assert u32(0x598468)==2,'The native difficulty popup must retain the Versus portrait background'
                assert list(read(0x598C28,read(0x598C54,1)[0]))==available,'Difficulty must not replace the saved Versus roster'
                if args.survival_popup_idle_seconds:
                    before_status=[u32(0x598DF0+0x48+i*12) for i in range(8)]
                    before_actors=[u32(0x58F8A0+i*0x348+0x2A0) for i in range(8)]
                    wait(args.survival_popup_idle_seconds)
                    assert top()==0x596810 and u32(0x598468)==2,'Difficulty idle unexpectedly changed or left the Versus-backed modal'
                    assert [u32(0x598DF0+0x48+i*12) for i in range(8)]==before_status,'Difficulty idle modified character locks/selections'
                    assert [u32(0x58F8A0+i*0x348+0x2A0) for i in range(8)]==before_actors,'Difficulty idle changed background fighter movies'
                    capture('survival-difficulty-idle')
                    checks.append(dict(capture='survival-difficulty-idle-check',seconds=args.survival_popup_idle_seconds,actors=before_actors,status=before_status))
                if args.survival_check_back:
                    key(27);wait(2)
                    assert top()==0x598DF0,('Difficulty Back must return to fighter selection',hex(top()))
                    assert u32(0x598468)==2,'Returning to character selection must restore the Versus portraits'
                    capture('survival-characters-after-back')
                    deadline=time.monotonic()+35
                    while top()==0x598DF0 and time.monotonic()<deadline:
                        key(32)
                        if args.survival_koop:pulse(6,duration=.2,gap=.8);pulse(0,duration=.2,gap=.8)
                    assert top()==0x596810,('Confirming again after Back must open difficulty',hex(top()))
                    assert u32(0x598468)==2,'Reconfirmation must retain the complete native portrait background'
                for _ in range(args.survival_difficulty):key(40)
                capture('survival-difficulty');survival_accept(lambda:top()==0)
                deadline=time.monotonic()+60
                while time.monotonic()<deadline:
                    if 'Survival rules ready' in logpath.read_text(errors='replace'):break
                    wait(.2)
                assert 'Survival rules ready' in logpath.read_text(errors='replace'),'The arena rules frame must appear before the first wave'
                rules_message=0x4E6E20+0x6C0
                assert read(rules_message,1)==b'\x01' and u32(rules_message+0x18)==0xFFFFFD20,'The original in-game rules message must be active'
                assert read(rules_message+0x29,2)==b'\x01\x01' and read(0x5AE5B0,1)==b'\x01','Rules must wait for A and pause the first arena'
                checks.append(dict(capture='survival-rules-modal',native_frame=True,wait_for_a=True,world_paused=True))
                if args.survival_verify_upgrades:
                    # Check the refill before releasing the rules frame; combat can lower health later.
                    paused_state=u32(0x5ACEBC)
                    assert u32(paused_state+0xE0)==4 and u32(paused_state+0x90)==1,\
                        'Survival must apply all four story health pickups instead of the debug tier cheat'
                    assert u32(paused_state+0x94)==(1200 if args.survival_koop else 600),\
                        'Native campaign health table must select the full four-pickup row before combat'
                    paused_health=[]
                    fatality_capacity=[]
                    for player in range(1,3 if args.survival_koop else 2):
                        component=u32(0x5CE880+player*0x3238+0x200)
                        assert component,('Survival hero has no health component before combat',player)
                        expected=u32(paused_state+0x94)//(2 if args.survival_koop else 1)
                        health,maximum=u32(component+0x368),u32(component+0x370)
                        assert health==maximum==expected,('Survival hero must be fully healed before the first wave',player,health,maximum,expected)
                        paused_health.append(dict(player=player,current=health,maximum=maximum))
                        actor=0x5CE880+player*0x3238
                        assert u32(actor+0x68)==player,('Unexpected native hero ID for Fatality level',player)
                        level=u32(paused_state+0x4928+player*0x50)
                        assert level==3,('Survival must unlock all three Fatality capacity levels',player,level)
                        charge=struct.unpack('<f',read(paused_state+0x4904+player*0x50,4))[0]
                        fatality_capacity.append(dict(player=player,unlocked_levels=level,current_charge=charge))
                    checks.append(dict(capture='survival-health-before-combat',heroes=paused_health,world_paused=True))
                    checks.append(dict(capture='survival-fatality-capacity',heroes=fatality_capacity,charged_by_gameplay=True))
                # Check a settled frame, not the black entrance frame.
                wait(3)
                capture('survival-rules')
                if not args.survival_quick:
                    frame=run/checks[-1]['presentation']
                    bmp=frame.read_bytes()
                    assert bmp[:2]==b'BM','Expected an actual BMP capture of the native rules presentation'
                    pixel_start=struct.unpack_from('<I',bmp,10)[0]
                    width,height=struct.unpack_from('<ii',bmp,18)
                    depth=struct.unpack_from('<H',bmp,28)[0]
                    assert width>0 and height and depth==32,('Unexpected rules capture format',width,height,depth)
                    stride=width*4
                    def backdrop_pixel(x,y):
                        row=y if height<0 else height-y-1
                        at=pixel_start+row*stride+x*4
                        return tuple(bmp[at:at+3])
                    frame_height=abs(height)
                    corners=[backdrop_pixel(x,y) for x,y in ((0,0),(width//2,0),(0,frame_height//2),
                                                                  (width-1,frame_height//2),(width-1,frame_height-1))]
                    assert all(max(rgb)<=16 for rgb in corners),('Survival rules must hide the arena behind an entirely black backdrop',corners)
                    assert max(backdrop_pixel(width//2,frame_height//2))>32,'The native framed rules panel must remain visible over black'
                    checks.append(dict(capture='survival-black-rules-verified',corners=corners,native_panel_visible=True))
                if args.survival_rules_only:
                    assert read(rules_message,1)==b'\x01' and u32(rules_message+0x18)==0xFFFFFD20 and read(0x5AE5B0,1)==b'\x01',\
                        'The native rules must remain visible and pause the arena after settling'
                    checks.append(dict(capture='survival-rules-held',world_paused=True,awaiting_a=True))
                    quit=True
                    break
                survival_accept(lambda:'Survival rules confirmed' in logpath.read_text(errors='replace'))
                deadline=time.monotonic()+60
                while time.monotonic()<deadline:
                    wait(.2)
                    if 'Survival voice 396797D8 handle' in logpath.read_text(errors='replace'):break
                assert top()==0 and u32(0x505970)==161,('Survival must start at level one',hex(top()),u32(0x505970))
                live=u32(0x5ACEBC)
                assert u32(live+0x88)==int(args.survival_koop)
                assert u32(live+0xD4)==min(args.survival_difficulty,2)
                assert u32(0x4EF42C)&255==1,'Survival must release gameplay controls after loading'
                before_position=struct.unpack('<3f',read(0x5D1AB8+0x80,12))
                assert user.PostMessageW(hwnd,0x100,68,0);wait(.8)
                assert user.PostMessageW(hwnd,0x101,68,0);wait(.5)
                after_position=struct.unpack('<3f',read(0x5D1AB8+0x80,12))
                assert sum((b-a)**2 for a,b in zip(before_position,after_position))>1,('The first hero must respond to gameplay input',before_position,after_position)
                capture('survival-level-1')
                native=logpath.read_text(errors='replace')
                assert native.index('Survival voice 5AFBC61B handle')<native.index('Survival rules confirmed')<native.index('Survival voice 396797D8 handle'),'Rules announcement, A confirmation and challenge announcement must remain ordered'
                audio_events=['Survival rules ready','Survival music deferred until rules fade',
                              'Survival rules dismissed: beginning black fade','Survival music started after rules fade',
                              'Survival rules confirmed: Level One released']
                assert all(label in native for label in audio_events),'Survival intro must defer the arena music and fade after A'
                assert [native.index(label) for label in audio_events]==sorted(native.index(label) for label in audio_events),\
                    'The rules, A dismissal, fade, deferred music and Level One must occur in that order'
                start=re.search(r'\[\s*([\d.]+)\].*Survival rules dismissed: beginning black fade',native)
                end=re.search(r'\[\s*([\d.]+)\].*Survival rules confirmed: Level One released',native)
                assert start and end and .4<=float(end.group(1))-float(start.group(1))<=2.5,\
                    'The rules fade must progress for its native ticks instead of immediately exposing gameplay'
                checks.append(dict(capture='survival-fade-sequenced',duration_seconds=round(float(end.group(1))-float(start.group(1)),3),music_deferred=True))
                for sound in ['6B3A3469','5AFBC61B','396797D8']:
                    assert re.search('Survival voice '+sound+r' handle (?!00000000)[0-9A-F]+',native),('Missing Survival recording',sound)
                enemies=[]
                for enemy in range(3,15):
                    actor=0x5CE880+enemy*0x3238;component=u32(actor+0x200)
                    if component and read(actor+0x70,1)==b'\x00' and read(actor+0x2737,1)!=b'\x01' and u32(component+0x368)>0:
                        enemies.append(enemy)
                assert enemies,'The first Survival arena must start its original combat encounter'
                if args.survival_verify_upgrades:
                    assert u32(live+0xE0)==4 and u32(live+0x90)==1,\
                        'Survival must keep all four native story health pickups'
                    assert u32(live+0x94)==(1200 if args.survival_koop else 600),\
                        'Survival must retain the native full-story base health in combat'
                    tuned=list(read(0x4F3390,27))
                    ranks=[]
                    for player in range(2):
                        levels=list(read(live+0x1C4+player*0x1C,27))
                        for skill in [*range(0,11),*range(13,23)]:
                            assert levels[skill]==tuned[skill],('A campaign skill did not reach its tuned maximum',player,skill,levels[skill],tuned[skill])
                        ranks.append(levels)
                    for player in range(1,3 if args.survival_koop else 2):
                        component=u32(0x5CE880+player*0x3238+0x200)
                        assert component,('Survival hero has no health component',player)
                        expected=u32(live+0x94)//(2 if args.survival_koop else 1)
                        health,maximum=u32(component+0x368),u32(component+0x370)
                        assert maximum==expected and 0<health<=maximum,\
                            ('Survival hero must retain maximum upgraded health capacity after combat starts',player,health,maximum,expected)
                    checks.append(dict(capture='survival-upgrades',story_health_pickups=4,health_tier=1,
                                       story_base_health=u32(live+0x94),player_ranks=ranks,
                                       campaign_skill_count=21,versus_skill_ranks=[r[11:13] for r in ranks],
                                       native_koop_ranks=[r[23:27] for r in ranks],
                                       ko_op=args.survival_koop,full_health_before_combat=True))
                if args.survival_verify_start_select:
                    def survival_pause(press,release,source):
                        assert read(0x4EF42C,1)==b'\x01',('Gameplay not active before Start',source)
                        before=logpath.read_text(errors='replace').count('Gameplay pause expanded:')
                        press();wait(.45);release()
                        deadline=time.monotonic()+12
                        while time.monotonic()<deadline:
                            if read(0x4EF42C,1)==b'\x00' and logpath.read_text(errors='replace').count('Gameplay pause expanded:')>before:break
                            wait(.1)
                        assert read(0x4EF42C,1)==b'\x00' and logpath.read_text(errors='replace').count('Gameplay pause expanded:')>before,\
                            ('Start did not open the native Select pause menu',source)
                        capture('survival-start-select-'+source)
                        key(27);wait(2)
                        assert read(0x4EF42C,1)==b'\x01',('B did not resume Survival after Start-as-Select',source)
                    survival_pause(lambda:user.PostMessageW(hwnd,0x100,13,0),lambda:user.PostMessageW(hwnd,0x101,13,0),'keyboard')
                    if args.survival_koop:
                        survival_pause(lambda:button(6,True),lambda:button(6,False),'controller-p2')
                    checks.append(dict(capture='survival-start-select',keyboard=True,controller_p2=args.survival_koop,native_select_menu=True))
                checks.append(dict(capture='survival-flow',koop=args.survival_koop,fighter=args.survival_fighter,available=available,difficulty=args.survival_difficulty,keyboard_only=args.survival_keyboard_only,short_space=args.survival_short_space,room=161,voices=True,enemies=enemies))
                if args.survival_levels:
                    for level,room in enumerate([162,163,164,165,166,187,191,190],2):
                        write_memory(0x4FC160,struct.pack('<I',room));write_memory(0x4FC164,struct.pack('<I',0));write_memory(0x5ACEDC,b'\x01')
                        deadline=time.monotonic()+60
                        while time.monotonic()<deadline:
                            wait(.2)
                            if u32(0x505970)==room and ('Survival level '+str(level)+' ready') in logpath.read_text(errors='replace'):break
                        wait(4);assert u32(0x505970)==room and ('Survival level '+str(level)+' ready') in logpath.read_text(errors='replace'),('Survival arena did not load',level,room)
                        capture('survival-level-'+str(level))
                    checks.append(dict(capture='survival-arenas',levels=9,native_completion=False))
            elif action[0]=='confirm-exit':
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
            elif action[0]=='coop-load':
                records=slot_records()
                if args.slot!=1:write_memory(records[0],read(records[args.slot-1],0x51AC))
                assert slot_used(1) and u32(records[0]+0x88)==1,'The selected fixture slot must contain a Ko-op save'
                # The keyboard accepts the save. The only controller must then become P2.
                key(32)
                assert top()==0x596810 and u32(0x59684C)==6,'A Ko-op save must ask player two to press Start'
                capture('coop-load-waiting-for-start')
                key(27)
                assert top()==0x5967C0,'Cancelling the join prompt must return to the save list'
                modes=re.findall(r'input: multiplayer=(\d)',logpath.read_text(errors='replace'))
                assert modes and modes[-1]=='0','Cancelling the join prompt must return the controller to shared P1'
                key(32)
                assert top()==0x596810 and u32(0x59684C)==6,'The Ko-op save must remain selectable after cancellation'
                pulse(6,duration=.4,gap=2)
                assert top()==0x596810 and u32(0x59684C)==1,('Start on the sole controller must join as P2 instead of remaining on P1',hex(top()),u32(0x59684C))
                capture('coop-load-player-two-joined')
                selection=u32(0x596820)+12
                for _ in range(4):
                    if u32(selection)==1:break
                    key(40)
                assert u32(selection)==1,'Keyboard P1 must be able to select Yes after P2 joins'
                key(32)
                until=time.monotonic()+60
                while top()!=0 and time.monotonic()<until:wait(.12)
                state=u32(0x5ACEBC)
                assert top()==0 and state and u32(state+0x88)==1,'The accepted save must leave the frontend with the Ko-op state loaded'
                checks.append(dict(capture='coop-load',cancelled_and_retried=True,joined_with_controller=True,keyboard_confirmed=True,mode=u32(state+0x88)))
                wait(15)
                capture('coop-save-loaded')
            elif action[0]=='slot-variants':
                slot_variants(action[1])
            elif action[0]=='save-list-totem':
                # Copy the totem save over slot 1 in this process only; saves on disk are never written.
                records=slot_records()
                if args.slot!=1:write_memory(records[0],read(records[args.slot-1],0x51AC))
                wait(1);capture('load-slot-'+str(args.slot))
                for _ in range(10):
                    if top()!=0x5967C0:break
                    pulse(0,duration=.35,gap=4)
                if top()==0x596810:
                    confirm_selection=u32(0x596820)+12
                    for _ in range(6):
                        if u32(confirm_selection)==1:break
                        pulse(12,duration=.35,gap=1.4)
                    if u32(confirm_selection)!=1:write_memory(confirm_selection,struct.pack('<I',1))
                    for _ in range(5):
                        if top()!=0x596810:break
                        pulse(0,duration=.4,gap=3)
                deadline=time.monotonic()+60
                while top()!=0 and time.monotonic()<deadline:wait(.12)
                wait(32)
                capture('loaded-at-totem')
                # The totem opens Save Game with the right shoulder (SDL button 10).
                pulse(10,duration=.35,gap=5)
                if top()==0:pulse(10,duration=.35,gap=5)
                capture('totem-save')
                slot_variants('totem')
                # RT turns to page 2; saving there creates the page file and leaves native slots untouched.
                native=read(slot_records()[0],0x51AC)
                trigger(5);capture('totem-page-2')
                page_file=run/'saves'/'openshao-save-page-02.bin'
                had_page=page_file.exists()
                pulse(0,duration=.35,gap=6)
                capture('totem-page-2-saved')
                assert page_file.exists(),'Saving on page 2 must create its page file'
                trigger(4);wait(1);capture('totem-page-1')
                assert read(slot_records()[0],0x51AC)==native,'Page 1 slots must not change when saving on page 2'
                checks.append(dict(capture='save-page-2',page_file=True,existed_before=had_page,page_1_unchanged=True))
                pulse(1,duration=.35,gap=3)
            elif action[0]=='pages-session':
                records=slot_records()
                if args.slot!=1:write_memory(records[0],read(records[args.slot-1],0x51AC))
                wait(1);load_selected();wait(32)
                capture('loaded-at-totem')
                pulse(10,duration=.35,gap=5)
                if top()==0:pulse(10,duration=.35,gap=5)
                capture('totem-save')
                disk=save_slots(save_file())  # the native slots as the save file holds them now
                trigger(5);capture('totem-page-2')
                assert not slot_used(1),'Page 2 must start empty'
                press_a();capture('totem-page-2-saved')
                assert page_slot_used(2,1) and slot_used(1),'Saving on page 2 must store slot 1 in its page file'
                # X asks first; B keeps the save, A deletes it from the page file.
                press_x();capture('totem-delete-question')
                answer(1);capture('totem-delete-kept')
                assert slot_used(1) and page_slot_used(2,1),'Answering No must keep the save and leave Save Game open'
                press_x();answer(0);capture('totem-deleted')
                assert not slot_used(1) and not page_slot_used(2,1),'Answering Yes must delete the save on page 2'
                press_a();capture('totem-page-2-saved-again')
                assert page_slot_used(2,1),'Saving again on page 2 must store slot 1'
                assert save_slots(save_file())==disk,'Saves on page 2 must leave the native slots untouched'
                checks.append(dict(capture='totem-page-2',saved=True,delete_no_kept=True,delete_yes_removed=True,page_1_unchanged=True))
                # Leave the campaign with page 2 still the wanted page: the main menu must show its save.
                pulse(1,duration=.35,gap=3)
                pulse(4,duration=.3,gap=4)
                assert read(0x4EF42C,1)==b'\x00','The native gameplay pause must be open'
                for _ in range(3):pulse(12,duration=.2,gap=.8)
                pulse(0,duration=.3,gap=3);pulse(13,duration=.2,gap=.8);pulse(0,duration=.3,gap=10)
                deadline=time.monotonic()+25
                while top()!=0x598430 and time.monotonic()<deadline:wait(.1)
                assert top()==0x598430,'Exiting the campaign must return to the main menu'
                enter_load_game();capture('load-after-exit')
                assert slot_used(1),'After leaving the campaign, page 2 must still show its save'
                trigger(4);capture('load-after-exit-page-1')
                selection=u32(0x5967C0+0x38)+1
                assert slot_used(selection),'The selected page 1 slot must hold a save'
                disk=save_slots(save_file())
                press_x();capture('main-delete-question')
                answer(0);wait(2);capture('main-deleted')
                after=save_slots(save_file())
                assert not slot_used(selection) and after[selection-1][0x84]==0,'Deleting on page 1 must update the native save file'
                assert all(after[i]==disk[i] for i in range(9) if i!=selection-1),'Deleting one slot must keep the others'
                checks.append(dict(capture='main-delete',slot=selection,native_file_updated=True,others_unchanged=True))
            elif action[0]=='pages-reload':
                capture('reload-load-main')
                empty=[i+1 for i in range(9) if not slot_used(i+1)]
                assert len(empty)==1,('The slot deleted on page 1 must stay deleted after restarting',empty)
                trigger(5);capture('reload-page-2')
                assert slot_used(1),'The page 2 save must load from its file after restarting'
                record=read(slot_records()[0],0x51AC)
                write_memory(0x5967C0+0x38,struct.pack('<I',0))
                wait(.5);load_selected();wait(20)
                capture('reload-loaded-page-2')
                room=u32(0x505970);saved_room=struct.unpack('<I',record[0xC8:0xCC])[0]
                assert room==saved_room,('Loading from page 2 must enter the saved room',room,saved_room)
                checks.append(dict(capture='reload',page_1_deleted_kept=empty,page_2_loaded=True,room=room))
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
                            # Isolated Load Game fixtures set this field directly; disc and source saves stay untouched.
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
                    # Top 0 starts the async level load; CARREGANDO can last tens of seconds.
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
                assert u32(panel+0x64)==7 and int(match.group(2))==7,'Locked pause must have seven actions with Photo Mode'
                original_rows=u32(panel+0x54)
                assert [u32(original_rows+i*8) for i in range(6)]==[0x1A4,0x1A5,0xFFFFFF00,0xFFFFFF09,0xFFFFFF0A,0x1A7]
                assert u32(original_rows+44)==0x16262,'Exit Game retains the original callback'
                initial=tomllib.loads((data/'settings.toml').read_text(encoding='utf-8'))
                def pause_press(index,mask,gap=1.2):
                    button(index,True)
                    deadline=time.monotonic()+8
                    while not (u32(0x67C668)&mask) and time.monotonic()<deadline:wait(.025)
                    seen=bool(u32(0x67C668)&mask)
                    wait(.045)
                    button(index,False)
                    deadline=time.monotonic()+8
                    while u32(0x67C668)&mask and time.monotonic()<deadline:wait(.025)
                    assert seen and not (u32(0x67C668)&mask),('Pause must read and release the button',index)
                    wait(gap)
                def go_pause_row(row):
                    for _ in range(15):
                        selected=u32(panel+0x6C)
                        if selected==row:break
                        index,mask=(12,0x4000) if selected<row else (11,0x1000)
                        pause_press(index,mask,gap=.4)
                    assert u32(panel+0x6C)==row,('Pause selection',row,u32(panel+0x6C))
                def pause_choose(row):
                    go_pause_row(row);pause_press(0,0x40)
                def pause_adjust(row):
                    go_pause_row(row);pause_press(14,0x2000)
                def pause_back():pause_press(1,0x20)
                def pause_settings():return tomllib.loads((data/'settings.toml').read_text(encoding='utf-8'))
                pause_choose(2);assert u32(panel+0x64)==5,'Five settings categories open'
                capture('pause-recomp-open')
                pause_choose(1);assert u32(panel+0x64)==5,'Graphics has five settings'
                for row in (3,1,0,2):pause_adjust(row)
                current=pause_settings()
                for key,default in [('bloom_effect',False),('vsync',True)]:
                    assert current[key] is (not initial.get(key,default))
                assert current['anisotropic_filtering']!=initial.get('anisotropic_filtering',0)
                assert current['render_percent']!=initial.get('render_percent',100)
                if current['vsync']:pause_adjust(2)
                pause_adjust(4);assert pause_settings()['fps_cap']==120
                capture('pause-recomp-graphics')
                pause_back();assert u32(panel+0x64)==5
                pause_choose(0);assert u32(panel+0x64)==5,'Display has pending Apply and Discard actions'
                old_mode=pause_settings()['display_mode']
                pause_adjust(0);assert pause_settings()['display_mode']==old_mode
                pause_choose(3);assert u32(panel+0x64)==2,'Apply starts display confirmation'
                capture('pause-recomp-display-confirm')
                pause_back()
                assert u32(panel+0x64)==5 and pause_settings()['display_mode']==old_mode
                pause_back();assert u32(panel+0x64)==5
                pause_choose(3);assert u32(panel+0x64)==1
                old_volume=pause_settings()['master_volume']
                index,mask=(14,0x2000) if old_volume==0 else (13,0x8000)
                pause_press(index,mask)
                assert pause_settings()['master_volume']!=old_volume
                pause_back();assert u32(panel+0x64)==5
                # Interface: Show FPS and photo notifications, then F9 must update the saved Show FPS too.
                pause_choose(4);assert u32(panel+0x64)==2,'Interface has Show FPS and photo notifications'
                pause_adjust(0);pause_adjust(1)
                current=pause_settings()
                assert current['show_fps'] is (not initial.get('show_fps',False)),'Interface Show FPS must persist'
                assert current['screenshot_notifications'] is (not initial.get('screenshot_notifications',True)),'Photo notifications must persist'
                capture('pause-recomp-interface')
                assert user.PostMessageW(hwnd,0x100,0x78,0);wait(.2);assert user.PostMessageW(hwnd,0x101,0x78,0xC0000001);wait(1.0)
                assert pause_settings()['show_fps'] is initial.get('show_fps',False),'F9 must save the Show FPS choice'
                pause_back();assert u32(panel+0x64)==5
                # Controls: deadzone, rumble, mouse sensitivity and keyboard remap.
                pause_choose(2);assert u32(panel+0x64)==5,'Controls must have five rows'
                pause_adjust(3);pause_adjust(4)
                current=pause_settings()
                assert current['controller']['deadzone']!=initial.get('controller',{}).get('deadzone',.15)
                assert current['controller']['rumble'] is (not initial.get('controller',{}).get('rumble',True))
                pause_choose(2);assert u32(panel+0x64)==4,'Mouse settings must open'
                pause_adjust(0);assert pause_settings()['mouse']['sensitivity']!=initial.get('mouse',{}).get('sensitivity',100)
                trigger(4);assert u32(panel+0x64)==3,'LT wraps to the last mouse page'
                capture('pause-recomp-mouse-last')
                trigger(5);assert u32(panel+0x64)==4,'RT wraps back to mouse settings'
                pause_back();assert u32(panel+0x64)==5
                pause_choose(1);assert u32(panel+0x64)==3,'Keyboard mapping must open'
                pause_choose(2) # Capture the X key in slot 1, keep A/B navigation untouched.
                assert user.PostMessageW(hwnd,0x100,ord('Z'),0)
                wait(.3)
                assert user.PostMessageW(hwnd,0x101,ord('Z'),0)
                wait(1.4)
                assert pause_settings()['keyboard']['bindings']['x'][0]==ord('Z'),'In-game keyboard remapping must persist'
                trigger(4);assert u32(panel+0x64)==1,'LT reaches the last alternate keyboard page'
                capture('pause-recomp-keyboard-alternate-last')
                trigger(5);assert u32(panel+0x64)==3,'RT returns to primary keyboard page one'
                pause_back();assert u32(panel+0x64)==5
                pause_choose(0);assert u32(panel+0x64)==3,'Controller mapping must open'
                trigger(4);assert u32(panel+0x64)==1,'LT wraps to the recenter binding'
                capture('pause-recomp-camera-binding')
                trigger(5);assert u32(panel+0x64)==3,'RT wraps to the first controller page'
                pause_choose(2) # X; avoid remapping A/B which drive this smoke.
                pulse(16,duration=.25,gap=1.5) # SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, unassigned in default profile.
                assert pause_settings()['controller']['bindings']['x']=='paddle1','In-game controller remapping must persist'
                capture('pause-recomp-controls')
                pause_back();assert u32(panel+0x64)==5,('Controller back to controls',u32(panel+0x64))
                pause_back();assert u32(panel+0x64)==5,('Controls back to categories',u32(panel+0x64))
                pause_back()
                assert u32(panel+0x54)==original_rows and u32(panel+0x6C)==2
                pause_choose(3);assert u32(panel+0x64)==4,'Enhancements opens directly from Select'
                for row,key in enumerate(('scorpion_unfiltered','restore_dialogue','cinematic_bars')):
                    before=pause_settings()['enhancements'][key]
                    pause_adjust(row)
                    assert pause_settings()['enhancements'][key] is not before
                capture('pause-enhancements')
                pause_choose(3);assert u32(panel+0x64)==3
                pause_back()
                assert u32(panel+0x64)==4 and read(0x4EF42C,1)==b'\x00'
                pause_back()
                assert u32(panel+0x54)==original_rows and u32(panel+0x6C)==3
                pause_choose(1);capture('pause-retail-options')
                pause_press(1,0x20,gap=2)
                assert read(0x4EF42C,1)==b'\x00' and u32(panel+0x54)==original_rows and u32(panel+0x6C)==1
                pause_choose(6)
                assert u32(panel+0x64)==2 and u32(panel+0x6C)==0,'Exit confirmation defaults to No'
                capture('pause-exit-confirm')
                pause_press(0,0x40,gap=1.4)
                assert u32(panel+0x54)==original_rows and process.poll() is None
                assert u32(panel+0x6C)==6,'No restores the desktop-exit row'
                pause_press(0,0x40,gap=1.4)
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
                    assert process.poll() is None and after_cancel_rows==original_rows and read(0x4EF42C,1)==b'\x00',\
                        'B must safely cancel the exit confirmation without closing the game'
                checks.append(dict(capture='pause-recomp',root_rows=7,categories=['display','graphics','controls','audio','interface'],
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
            elif action[0]=='trigger':trigger(action[1])
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
            elif action[0]=='native-audio-wrap':
                assert top()==0x5985E8 and u32(u32(0x5985F4)+0x2E4)==6
                settings_idle();button(11,True);deadline=time.monotonic()+10
                while u32(0x598620)==0 and time.monotonic()<deadline:wait(.025)
                button(11,False);wait(.6)
                assert u32(0x598620)==2,'Original Audio must retain only its three volume rows'
            elif action[0]=='assert-audio-mode':
                selected=action[1]
                assert u32(0x64CFCC)==(3 if selected==2 else selected),'Only Save may change the current audio routing'
                if action[2]:
                    import tomllib
                    saved=tomllib.loads((data/'settings.toml').read_text(encoding='utf-8'))
                    assert saved['game']['audio_output']==selected,'Audio Output must persist or remain unchanged after Discard'
                checks.append(dict(capture='audio-output-state',mode=selected,saved=action[2]))
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
            elif action[0]=='tap-to-row':
                # Short taps like a player; the Game Mode pages ignore directions while they animate.
                for _ in range(8):
                    if u32(0x598468)==action[2]:break
                    pulse(action[1],duration=.12,gap=1)
                assert top()==0x598430 and u32(0x598468)==action[2],('Main row after taps',u32(0x598468),action[2])
            elif action[0]=='pick-fighter':
                # Same as the Survival probe: focus an unlocked fighter, then confirm with Space until difficulty opens.
                assert top()==0x598DF0,('Expected the fighter roster',hex(top()))
                available=list(read(0x598C28,min(read(0x598C54,1)[0],8)))
                assert available,'No selectable fighter in the roster'
                for _ in range(8):
                    if u32(0x598E2C) in available:break
                    key(39)
                deadline=time.monotonic()+35
                while top()==0x598DF0 and time.monotonic()<deadline:key(32)
                assert top()==0x596810,('Confirming a fighter must open the difficulty popup',hex(top()))
            elif action[0]=='accept-until':
                # Retry A until the target is on top; confirm fighters with a held Space (P1).
                until=time.monotonic()+15
                while top()!=int(action[1],16) and time.monotonic()<until:
                    assert user.PostMessageW(hwnd,0x100,32,0);wait(2)
                    assert user.PostMessageW(hwnd,0x101,32,0);wait(1.5)
                assert top()==int(action[1],16),('Accept did not reach',hex(top()),action[1])
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
        assert user.PostMessageW(hwnd,0x10,0,0);closed=True;process.wait(timeout=30)
except Exception as e:
    failure=repr(e)
finally:
    if survival_frame_log is not None:survival_frame_log.close()
    if roundtrip_frame_log is not None:roundtrip_frame_log.close()
    if process.poll() is None:process.terminate()
    code=process.wait(timeout=8)
    for child in owned_children:
        if child.poll() is None:child.terminate();child.wait(timeout=8)
        kernel.CloseHandle(child._handle)
    log.close()
    saves_after={str(f.relative_to(save_source)):hashlib.sha256(f.read_bytes()).hexdigest() for f in save_source.rglob('*') if f.is_file()}
    copied_saves={str(f.relative_to(run/'saves/E/UDATA/4d570029')):hashlib.sha256(f.read_bytes()).hexdigest() for f in (run/'saves/E/UDATA/4d570029').rglob('*') if f.is_file()}
    result=dict(copied_title_saves_unchanged=copied_saves==save_hashes,language=locale,mode=mode,output=size,exit_code=code,closed=closed,failure=failure,checks=checks,events=events,trace=trace,visible_windows=visible,source_xbe_unchanged=(str(xbe.stat().st_size)+':'+str(xbe.stat().st_mtime_ns) if disc else hashlib.sha256(xbe.read_bytes()).hexdigest())==source_hash,source_saves_unchanged=saves_after==save_hashes,exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest())
    if ornament_motion:(run/'ornament-motion.json').write_text(json.dumps(ornament_motion,indent=2))
    if args.return_audit:
        (run/'main-return-frames.json').write_text(json.dumps(dict(gold_handle=hex(gold_handle) if gold_handle else None,frames=return_frames),indent=2))
        seen_settings=seen_return_movie=seen_recovered_focus=False
        for sample in return_frames:
            if sample['top']=='0x5985e8':seen_settings=True
            if seen_settings and sample['top']=='0x598430' and sample['row']==6 and sample['clip']==16:
                seen_return_movie=True
            if seen_return_movie and sample['top']=='0x598430' and sample['row']==6 and sample['clip']==6 and sample['finished'] and sample['white_visible']==1:
                seen_recovered_focus=True
        if not seen_settings or not seen_return_movie or not seen_recovered_focus:
            failure=failure or 'Missing complete custom Options -> Main clip16 -> restored focus6 transition'
        result['failure']=failure
    (run/'result.json').write_text(json.dumps(result,indent=2));print('RESULT',mode,code,failure,flush=True)
    if args.menu_roundtrip:
        # Main's six lake objects must be active within 0.1 s (retail 0.18-0.35 s); no courtyard behind Main.
        rows=[json.loads(line) for line in (run/'roundtrip-frames.jsonl').read_text().splitlines() if line]
        segments=[];current=[]
        for row in rows:
            if row['top']=='0x598430':current.append(row)
            elif current:segments.append((current,True));current=[]
        if current:segments.append((current,False))
        problems=[]
        for index,(segment,leaves) in enumerate(segments):
            lake=[all(r['backdrops'][:6]) for r in segment]
            if not any(lake):
                if segment[-1]['t']-segment[0]['t']>.5:problems.append('Main without its backdrop from %.2f s'%segment[0]['t'])
                continue
            first=lake.index(True)
            gap=segment[first]['t']-segment[0]['t']
            if index and gap>.1:problems.append('backdrop restored %.2f s after Main returned at %.2f s'%(gap,segment[0]['t']))
            last=len(lake)-1-lake[::-1].index(True)
            for r,on in zip(segment[first:last+1],lake[first:last+1]):
                if not on:problems.append('backdrop off at %.2f s while Main was shown'%r['t']);break
            if not leaves and last!=len(lake)-1:problems.append('backdrop off at the end of the run')
            if leaves and segment[-1]['t']-segment[last]['t']>.6:problems.append('backdrop off %.2f s before leaving at %.2f s'%(segment[-1]['t']-segment[last]['t'],segment[-1]['t']))
            # The roster starts its courtyard a frame before it replaces Main (exit clip 11).
            if any(r['backdrops'][6] and r['main_clip']!=11 for r in segment[1:]):problems.append('fighter courtyard active on Main near %.2f s'%segment[0]['t'])
        summary=['ROUNDTRIP Main appearances checked: %d'%len(segments)]+['ROUNDTRIP FAIL '+p for p in problems]
        summary.append('ROUNDTRIP PASS' if not problems else 'ROUNDTRIP %d problem(s)'%len(problems))
        (run/'roundtrip-summary.txt').write_text('\n'.join(summary)+'\n')
        for line in summary:print(line,flush=True)
        if problems and not failure:failure='Main backdrop audit: '+problems[0]
assert not failure and code==0 and closed
if '--restart' in flags or '--voices' in flags or '--exit-desktop' in flags:assert result['copied_title_saves_unchanged']
assert result['source_xbe_unchanged'] and result['source_saves_unchanged'] and not visible
