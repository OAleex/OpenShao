# Building

## Requirements

All systems:

- Python 3.11 or later and Git
- The original USA/Europe `default.xbe`, kept outside the repository. Its SHA-256 must be
  `48240902bf4a04eb89376217c529531032d3370749bcce91a194878cc49db74a`.

Windows (Direct3D 12 and Vulkan):

- Visual Studio 2022 or later with the C++ workload, the Windows SDK, CMake and Ninja
- [LLVM 21.1.8](https://github.com/llvm/llvm-project/releases/tag/llvmorg-21.1.8) with Clang and LLD

Linux (Vulkan):

- CMake 3.25 or later, Clang 18 or later, LLD and Ninja
- On Ubuntu or Debian:

```sh
sudo apt install clang lld cmake ninja-build git python3 pkg-config libx11-dev libxext-dev libxrandr-dev \
    libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev libxkbcommon-dev libwayland-dev \
    wayland-protocols libdecor-0-dev libasound2-dev libpulse-dev libpipewire-0.3-dev libdbus-1-dev \
    libudev-dev libusb-1.0-0-dev
```

## Get the source

```sh
git clone https://github.com/OAleex/OpenShao.git
cd OpenShao
git clone https://github.com/TGP482/XboxXBERecomp.git third_party/XboxXBERecomp
git -C third_party/XboxXBERecomp checkout 40e9c35451305f09ca9d86f677e4b9c2c37ff26b
```

## Build

Windows:

```powershell
python tools/build_recomp.py --xbe "C:/Games/ShaolinMonks/default.xbe" --sdk third_party/XboxXBERecomp --llvm "C:/Program Files/LLVM" --test
```

Linux:

```sh
python3 tools/build_recomp.py --xbe ~/Games/ShaolinMonks/default.xbe --sdk third_party/XboxXBERecomp --test
```

The script downloads the pinned dependencies, compiles, runs the tests and writes the package and `checksums.txt` to `dist/`.
Vulkan headers and glslang are downloaded automatically. A separate Vulkan SDK is not required.
Logs are in `build/`.

| Target | Executable | Package |
| --- | --- | --- |
| Windows | `build/release/OpenShao.exe` | `dist/OpenShao-<version>-Windows-x64.zip` |
| Linux | `build/linux-release/OpenShao` | `dist/OpenShao-<version>-Linux-x64.tar.gz` |

The Linux build prints the minimum glibc version of the executable. Check it before you publish a package.

### Options

| Option | Effect |
| --- | --- |
| `--test` | Run the tests and package only when they pass |
| `--configure-only` | Configure without compiling or packaging |
| `--jobs <n>` | Number of parallel compile jobs, 1 to 8 |
| `--build-dir <path>` | Use another build directory |
| `--disc <path>` | Add startup tests with a real game image (Windows) |
| `--linux` | Also build the Linux package in WSL (Windows) |
| `--wsl-distro <name>` | WSL distribution for `--linux` |

### Linux package from Windows

1. Run `wsl --install -d Ubuntu-24.04` and restart if asked.
2. Open Ubuntu, create the user and install the Linux packages above.
3. Add `--linux` to the Windows build command.

The WSL build output goes to `~/.cache/openshao/linux-release`. The package still goes to `dist/`.

## Release

### Repository names

Publish the application in `OAleex/OpenShao` and the mod catalog in `OAleex/OpenShao-Mods`.
The updater accepts application downloads from `OAleex/OpenShao/releases/download/`.
The mod browser reads `OpenShao-Mods/master/mods.json`. Keep that branch available unless the catalog URL changes in a later build.

After renaming a repository on GitHub, update its local `origin` URL.

### Version and packages

Use `major.minor.patch` for the application version and `v<version>` for its release tag.
Keep these version fields synchronized before building:

| File | Version fields |
| --- | --- |
| `CMakeLists.txt` | `project(OpenShao VERSION ...)` |
| `resources/app.rc` | `FILEVERSION`, `PRODUCTVERSION`, `FileVersion`, `ProductVersion` |
| `src/mods/version.h` | `RecompVersion` |
| `tools/package_release.py` | Default value of `--version` |

The build script uses the packager's default version. Changing only the CMake version does not update the package names or mod compatibility version.

For 0.4.0, use:

| Release field | Value |
| --- | --- |
| Repository | `OAleex/OpenShao` |
| Tag | `v0.4.0` |
| Title | `OpenShao 0.4.0` |
| Attached files | `OpenShao-0.4.0-Windows-x64.zip`, `OpenShao-0.4.0-Linux-x64.tar.gz`, `checksums.txt` |

1. Set all version fields.
2. Build with `--test` and include both platforms.
3. Review and commit the exact source used for the builds.
4. Push that source to the application repository.
5. Create a draft release with the version tag pointing to that commit.
6. Attach both packages and the generated `checksums.txt`.
7. Publish the stable release with **Set as latest release** selected and **This is a pre-release** cleared.

The launcher updates from the latest release and only installs packages whose SHA-256 matches.
Do not rename the packages.
Use a higher version for every published update. Replacing assets under the same version does not offer an update to an installation already at that version.
Use a patch increment for fixes, such as `0.3.1`, and a minor increment for a feature release, such as `0.4.0`.
Mark preview builds as prereleases. Drafts and prereleases are not offered through the stable latest-release endpoint.

Each package holds the executable, `readme.txt` and `licenses/`. The ZIP puts them at the archive root. The `.tar.gz` puts them in one `OpenShao` folder. Game files, mods, logs, saves and settings are never included.
Publish the source and the dependency notices with every binary release.

### Mod releases

Mods use their own versions. The builder prints the required tag as `<id>-v<version>` and creates `<id>-<version>.tar.xz`.
Publish the package in `OpenShao-Mods` before publishing the updated `mods.json` on `master`.
The catalog's `recomp` field is the minimum supported application version, not the mod's version.
Follow the [mod publishing instructions](MODS.md#publish-and-update).

## Source layout

| Directory | Contents |
| --- | --- |
| `src/` | Host integration, launcher and compatibility code |
| `src/mods/` | Mod catalog, downloads and installation |
| `src/linux/` | SDL platform services and the Vulkan renderer |
| `cmake/` | Build modules |
| `config/` | Executable profile and ABI metadata |
| `resources/` | Icon, fonts, menu text and the packaged `readme.txt` |
| `tests/` | Automated checks, and manual scripts that accept `--help` |
| `tools/` | Build, SDK adaptation and packaging scripts |
| `licenses/` | Dependency licenses and notices |

More developer guides: [mods](MODS.md), [language packs](LANGUAGE_PACKS.md), [menu text](MENU_TEXT.md) and [graphics diagnostics](GRAPHICS_DIAGNOSTICS.md).
