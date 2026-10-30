"""Exercise the real launcher renderer preference on an isolated desktop."""
import argparse
from pathlib import Path
import tempfile
import tomllib
from recomp_launcher_test import Desktop, Launcher, WINDOWS


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--exe',type=Path,required=True)
    args=parser.parse_args()
    desktop=Desktop()
    try:
        with tempfile.TemporaryDirectory(prefix='openshao-renderer-launcher-',ignore_cleanup_errors=True) as folder:
            root=Path(folder); data=root/'data'; data.mkdir()
            config=data/'settings.toml'
            config.write_text('custom="keep"\nmaster_volume=0\n[launcher]\ncheck_updates=false\n')
            launcher=Launcher(args.exe.resolve(),['--configure','--data-dir',data],root,'first',desktop)
            state=launcher.wait(lambda s:s['page']=='play','The real Play page must open')
            assert state.get('renderer_selectable') is WINDOWS, 'The API selector must exist only on Windows'
            assert state.get('renderer')==('d3d12' if WINDOWS else 'vulkan'), 'The platform default must be selected'
            if WINDOWS:
                launcher.send('renderer vulkan')
                launcher.wait(lambda s:s.get('renderer')=='vulkan','The launcher must accept Vulkan')
                saved=tomllib.loads(config.read_text())
                assert saved['renderer']=='vulkan' and saved['custom']=='keep' and saved['master_volume']==0
                before=config.read_bytes()
                launcher.send('renderer unsupported')
                launcher.wait(lambda s:s.get('renderer')=='vulkan','An invalid API must not replace a valid preference')
                launcher.send('close'); assert launcher.exit_code()==0
                assert config.read_bytes()==before, 'Closing must preserve the selected API and unrelated preferences'
                launcher=Launcher(args.exe.resolve(),['--configure','--data-dir',data],root,'second',desktop)
                launcher.wait(lambda s:s.get('renderer')=='vulkan','A new launcher must reload the saved API')
            else:
                launcher.send('renderer d3d12')
                launcher.wait(lambda s:s.get('renderer')=='vulkan','Linux must keep its fixed Vulkan renderer')
            launcher.send('restore-video')
            launcher.wait(lambda s:s['popup'] is not None,'Recovery must report that settings were restored')
            saved=tomllib.loads(config.read_text())
            assert saved.get('renderer')==('d3d12' if WINDOWS else None), 'Video recovery must restore the supported platform default'
            assert saved['custom']=='keep', 'API recovery must preserve unrelated settings'
            launcher.send('dismiss','close'); assert launcher.exit_code()==0
    finally:
        for launcher in Launcher.started: launcher.close()
        desktop.close()
    print('PASS: platform renderer choice, persistence and launcher recovery')


if __name__=='__main__': main()
