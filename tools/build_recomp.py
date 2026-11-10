"""Build the native executable and release package, with optional tests.

Windows uses the selected LLVM toolchain and Visual Studio's CMake.
Linux uses Clang, CMake and Ninja. --linux also builds the Linux package in WSL.
"""
import argparse
import os
from pathlib import Path, PureWindowsPath
import shutil
import subprocess
import sys

SDK_REVISION = 'f7b13fdbf9560a3b3928aea24e37c789b24408f0'

# Ubuntu and Debian package names. SDL is built from source: without these headers its X11, Wayland and audio
# back ends are left out and the game cannot open a window or play sound.
LINUX_PACKAGES = ('clang lld cmake ninja-build git python3 pkg-config libx11-dev libxext-dev libxrandr-dev '
                  'libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev libxkbcommon-dev libwayland-dev '
                  'wayland-protocols libdecor-0-dev libasound2-dev libpulse-dev libpipewire-0.3-dev libdbus-1-dev '
                  'libudev-dev libusb-1.0-0-dev')
LINUX_HEADERS = ('X11/Xlib.h', 'X11/extensions/XInput2.h', 'xkbcommon/xkbcommon.h', 'wayland-client.h',
                 'alsa/asoundlib.h', 'pulse/pulseaudio.h')


def check_sdk(parser, sdk):
    revision = subprocess.run(['git', '-c', 'safe.directory=*', '-C', str(sdk), 'rev-parse', 'HEAD'],
                              capture_output=True, text=True, check=True).stdout.strip()
    if revision != SDK_REVISION:
        parser.error('The SDK revision differs from the reviewed integration')


QUIET = False


def run_steps(commands, root, logs, env=None, **popen):
    for command, name in commands:
        logfile = logs / name
        print(f'{name}: {logfile}', flush=True)
        with logfile.open('w', encoding='utf-8') as log:
            if QUIET:
                returncode = subprocess.run(command, cwd=root, env=env, stdout=log, stderr=subprocess.STDOUT, **popen).returncode
            else:
                process = subprocess.Popen(command, cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                           text=True, encoding='utf-8', errors='replace', bufsize=1, **popen)
                for line in process.stdout:
                    log.write(line)
                    print(line, end='', flush=True)
                returncode = process.wait()
        if returncode:
            raise SystemExit(returncode)


def windows_build(parser, args, root):
    llvm, sdk, xbe = args.llvm.resolve(), args.sdk.resolve(), args.xbe.resolve()
    for file in (xbe, llvm / 'bin/clang++.exe', llvm / 'bin/lld-link.exe', sdk / 'cmake/XbeRecomp.cmake'):
        if not file.is_file():
            parser.error(f'Required file is missing: {file}')
    check_sdk(parser, sdk)
    vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    located = subprocess.run([str(vswhere), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], capture_output=True, text=True, check=True).stdout.strip()
    if not located:
        parser.error('Visual Studio with the C++ workload was not found')
    vs = Path(located)
    build = args.build_dir.resolve() if args.build_dir else root / 'build/release'
    build.parent.mkdir(parents=True, exist_ok=True)
    # Capture the developer environment internally. Do not print it: it may contain credentials.
    setup = build.parent / 'vs_env.cmd'
    setup.write_text(f'@echo off\ncall "{vs / "Common7/Tools/VsDevCmd.bat"}" -arch=amd64 -host_arch=amd64 >nul\nif errorlevel 1 exit /b 1\nset\n', encoding='utf-8')
    configured_env = subprocess.run(['cmd.exe', '/d', '/c', 'call', str(setup)], capture_output=True, text=True, check=True)
    env = os.environ.copy()
    for line in configured_env.stdout.splitlines():
        if '=' in line and not line.startswith('='):
            key, value = line.split('=', 1)
            env[key] = value
    cmake_root = vs / 'Common7/IDE/CommonExtensions/Microsoft/CMake'
    env['PATH'] = str(llvm / 'bin') + os.pathsep + str(cmake_root / 'Ninja') + os.pathsep + env.get('Path', env.get('PATH', ''))
    env.pop('Path', None)
    cmake = cmake_root / 'CMake/bin/cmake.exe'
    commands = [([str(cmake), '-S', str(root), '-B', str(build), '-G', 'Ninja',
                 f'-DCMAKE_C_COMPILER={llvm / "bin/clang.exe"}', f'-DCMAKE_CXX_COMPILER={llvm / "bin/clang++.exe"}',
                 f'-DCMAKE_LINKER={llvm / "bin/lld-link.exe"}', f'-DCMAKE_MAKE_PROGRAM={cmake_root / "Ninja/ninja.exe"}',
                 f'-DOPENSHAO_RECOMP_SDK={sdk}', f'-DOPENSHAO_GAME_XBE={xbe}',
                 '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_CXX_FLAGS_RELEASE=-O1', '-DCMAKE_C_FLAGS_RELEASE=-O1',
                 '-DOPENSHAO_GENERATED_OPT=-O1', '-DOPENSHAO_SHADOW_DIAGNOSTICS=OFF'], 'configure.log')]
    if not args.configure_only:
        commands.append(([str(cmake), '--build', str(build), '--parallel', str(args.jobs)], 'build.log'))
    if args.test and not args.configure_only:
        commands.append(([str(cmake_root / 'CMake/bin/ctest.exe'), '--test-dir', str(build), '--output-on-failure'], 'ctest.log'))
    if not args.configure_only:
        commands.append(([sys.executable, str(root / 'tools/package_release.py'),
                          '--exe', str(build / 'OpenShao.exe')], 'package.log'))
    if args.disc:
        commands[0][0].append(f'-DOPENSHAO_TEST_DISC={args.disc.resolve()}')
    run_steps(commands, root, build.parent, env,
              creationflags=subprocess.CREATE_NO_WINDOW | subprocess.BELOW_NORMAL_PRIORITY_CLASS)
    print('Completed requested build steps.', flush=True)
    if not args.configure_only:
        print(f'Executable: {build / "OpenShao.exe"}', flush=True)
        print(f'Release ZIP and checksums: {root / "dist"}', flush=True)


def wsl_path(path, distro):
    """A Windows path as WSL sees it."""
    command = ['wsl.exe'] + (['-d', distro] if distro else []) + ['-e', 'wslpath', '-a', str(PureWindowsPath(path))]
    return subprocess.run(command, capture_output=True, text=True, check=True).stdout.strip()


def linux_build_through_wsl(parser, args, root):
    if not shutil.which('wsl.exe'):
        parser.error('--linux needs WSL: run "wsl --install -d Ubuntu-24.04" once, then see docs/BUILDING.md')
    distro = args.wsl_distro
    try:
        command = ['wsl.exe'] + (['-d', distro] if distro else []) + [
            '--cd', wsl_path(root, distro), '-e', 'python3', 'tools/build_recomp.py',
            '--xbe', wsl_path(args.xbe.resolve(), distro), '--sdk', wsl_path(args.sdk.resolve(), distro),
            '--jobs', str(args.jobs)]
    except (OSError, subprocess.CalledProcessError) as error:
        parser.error(f'WSL could not be started: {error}')
    if args.test:
        command.append('--test')
    if args.quiet:
        command.append('--quiet')
    print('Linux build in WSL...', flush=True)
    result = subprocess.run(command, cwd=root)
    if result.returncode:
        print('The Linux build failed; the Windows package is complete. See docs/BUILDING.md for the WSL setup.',
              flush=True)
        raise SystemExit(result.returncode)


def missing_linux_headers():
    roots = [Path('/usr/include'), Path('/usr/include/x86_64-linux-gnu'), Path('/usr/local/include')]
    return [header for header in LINUX_HEADERS if not any((folder / header).is_file() for folder in roots)]


def glibc_requirement(exe):
    """The newest glibc symbol version the executable needs, or None without objdump."""
    tool = shutil.which('llvm-objdump') or shutil.which('objdump')
    if not tool:
        return None
    symbols = subprocess.run([tool, '-T', str(exe)], capture_output=True, text=True).stdout
    versions = {tuple(int(part) for part in token[len('GLIBC_'):].split('.'))
                for token in symbols.replace('(', ' ').replace(')', ' ').split()
                if token.startswith('GLIBC_') and token[len('GLIBC_'):].replace('.', '').isdigit()}
    return '.'.join(map(str, max(versions))) if versions else None


def linux_build(parser, args, root):
    sdk, xbe = args.sdk.resolve(), args.xbe.resolve()
    for file in (xbe, sdk / 'cmake/XbeRecomp.cmake'):
        if not file.is_file():
            parser.error(f'Required file is missing: {file}')
    if args.llvm:
        clang, clangxx = args.llvm.resolve() / 'bin/clang', args.llvm.resolve() / 'bin/clang++'
    else:
        clang, clangxx = shutil.which('clang'), shutil.which('clang++')
    tools = {'clang': clang, 'clang++': clangxx, 'ld.lld': shutil.which('ld.lld'), 'cmake': shutil.which('cmake'),
             'ninja': shutil.which('ninja'), 'git': shutil.which('git')}
    missing = [name for name, path in tools.items() if not path or not Path(path).is_file()]
    headers = missing_linux_headers()
    if missing or headers:
        parser.error('Missing build tools or headers: ' + ', '.join(missing + headers) +
                     f'\nOn Ubuntu or Debian: sudo apt install {LINUX_PACKAGES}')
    check_sdk(parser, sdk)
    if args.build_dir:
        build = args.build_dir.resolve()
    elif str(root).startswith('/mnt/'):
        # Keep build output on the Linux filesystem to avoid slow Windows mount I/O.
        build = Path.home() / '.cache/openshao/linux-release'
    else:
        build = root / 'build/linux-release'
    build.mkdir(parents=True, exist_ok=True)
    commands = [(['cmake', '-S', str(root), '-B', str(build), '-G', 'Ninja',
                  f'-DCMAKE_C_COMPILER={clang}', f'-DCMAKE_CXX_COMPILER={clangxx}',
                  f'-DOPENSHAO_RECOMP_SDK={sdk}', f'-DOPENSHAO_GAME_XBE={xbe}',
                  '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_CXX_FLAGS_RELEASE=-O1', '-DCMAKE_C_FLAGS_RELEASE=-O1',
                  '-DOPENSHAO_GENERATED_OPT=-O1'], 'configure-linux.log')]
    if not args.configure_only:
        commands.append((['cmake', '--build', str(build), '--parallel', str(args.jobs)], 'build-linux.log'))
    if args.test and not args.configure_only:
        commands.append((['ctest', '--test-dir', str(build), '--output-on-failure'], 'ctest-linux.log'))
    if not args.configure_only:
        commands.append(([sys.executable, str(root / 'tools/package_release.py'),
                          '--exe', str(build / 'OpenShao')], 'package-linux.log'))
    logs = root / 'build'
    logs.mkdir(parents=True, exist_ok=True)
    run_steps(commands, root, logs)
    print('Completed requested build steps.', flush=True)
    if not args.configure_only:
        exe = build / 'OpenShao'
        print(f'Executable: {exe}', flush=True)
        glibc = glibc_requirement(exe)
        if glibc:
            print(f'Runs on Linux distributions with glibc {glibc} or newer', flush=True)
        print(f'Release archive and checksums: {root / "dist"}', flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--xbe', required=True, type=Path)
    parser.add_argument('--sdk', required=True, type=Path)
    parser.add_argument('--llvm', type=Path, help='LLVM folder (required on Windows; Linux defaults to the system Clang)')
    parser.add_argument('--jobs', type=int, default=2, choices=range(1, 9))
    parser.add_argument('--configure-only', action='store_true')
    parser.add_argument('--build-dir', type=Path, help='Build output directory')
    parser.add_argument('--test', action='store_true', help='Run CTest after compilation')
    parser.add_argument('--disc', type=Path, help='Enable the local real-disc Reset regression (Windows)')
    parser.add_argument('--linux', action='store_true', help='Windows: also build the Linux package inside WSL')
    parser.add_argument('--wsl-distro', help='WSL distribution for --linux (default: the WSL default)')
    parser.add_argument('--quiet', action='store_true', help='Write each step\'s output only to its log')
    args = parser.parse_args()
    global QUIET
    QUIET = args.quiet
    root = Path(__file__).resolve().parents[1]
    if sys.platform.startswith('linux'):
        linux_build(parser, args, root)
        return
    if not args.llvm:
        parser.error('--llvm is required on Windows')
    windows_build(parser, args, root)
    if args.linux and not args.configure_only:
        linux_build_through_wsl(parser, args, root)


if __name__ == '__main__':
    main()
