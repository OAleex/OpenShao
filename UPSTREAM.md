# Tools and dependencies

This document records dependency versions, licenses and implementation references.
For installation and launcher settings, use the [player guide](README.md).

## XboxXBERecomp

The port uses [XboxXBERecomp](https://github.com/TGP482/XboxXBERecomp), copyright 2026 TGP482, under the MIT license.
The supported SDK revision is `40e9c35451305f09ca9d86f677e4b9c2c37ff26b`.

The build script writes adapted SDK files in the build directory.
It preserves the SDK checkout.
The adaptations cover graphics, audio, movies, input, storage, mods, diagnostics and startup.
The controller report mapping derives from this SDK.

The executable includes SDL, Dear ImGui, toml++, miniz, XZ Embedded and Lua.
The launcher uses Dear ImGui, rendered with Direct3D 12 on Windows and Vulkan on Linux.
The Windows game renderer can use Direct3D 12 or Vulkan; Linux uses Vulkan.
SDL supplies controller input and platform services.
File selection uses the Windows file dialog, SDL dialogs, `zenity` or `kdialog`.
Release updates use the GitHub releases API and the SHA-256 digests published with each release.

## Hanken Grotesk

The launcher embeds the [Hanken Grotesk](https://github.com/marcologous/hanken-grotesk) typeface under the SIL Open Font License 1.1.
The build uses static Regular, Medium and SemiBold instances of the variable font from the [Google Fonts repository](https://github.com/google/fonts/tree/main/ofl/hankengrotesk).
See the [font license](licenses/HankenGrotesk-OFL.txt).

Dependency notices cover the embedded ImGui font, stb code, SDL's HIDAPI and YUV conversion code.
The recompiler tools use Zydis and Zycore.
The game executable excludes those recompiler libraries.
LLVM and [Ghidra](https://github.com/NationalSecurityAgency/ghidra) are development tools.
Ghidra supplied analysis of the original XBE.
Release packages exclude these development tools.

See the [SDK license](licenses/XboxXBERecomp-MIT.txt) and [dependency notices](licenses/THIRD_PARTY.txt).

## miniz

ZIP imports and language resource decompression use [miniz 3.1.0](https://github.com/richgel999/miniz) under the MIT license.
The build pins revision `174573d60290f447c13a2b1b3405de2b96e27d6c`.
It checks the source download's SHA-256 hash.
See the [miniz license](licenses/miniz-MIT.txt).

The build applies a local fix for ZIP64 entries with extended file offsets.
The adaptation preserves the pinned download. Extraction still checks file bounds and CRC values.

## Vulkan headers and glslang

The Vulkan renderer uses [Khronos Vulkan headers](https://github.com/KhronosGroup/Vulkan-Headers) and [glslang](https://github.com/KhronosGroup/glslang).
glslang compiles HLSL shaders to SPIR-V for Vulkan, including during pipeline preparation.

| Dependency | Version | Pinned revision |
| --- | --- | --- |
| Vulkan-Headers | `v1.4.321` | `2cd90f9d20df57eac214c148f3aed885372ddcfe` |
| glslang | `15.4.0` | `8a85691a0740d390761a1008b4696f57facd02c4` |

Both desktop builds include these dependencies.
Windows opens the system Vulkan loader when Vulkan is selected. Linux opens it through SDL.
The Vulkan renderer follows the SDK's targets, texture cache, pipeline keys and constants.
Its BC texture decoder follows the Khronos Data Format Specification.

Parts of glslang use the Apache License 2.0.
Builds that combine glslang with this project's code use GNU GPL version 3 or later.
The project's source code remains GPL-2.0-or-later.
See the [glslang license](licenses/glslang-LICENSE.txt) and [Vulkan-Headers license](licenses/Vulkan-Headers-MIT.txt).

## Mod packages

The installer uses [XZ Embedded](https://github.com/tukaani-project/xz-embedded) under the 0BSD license.
The build pins tag `v2024-12-30`, revision `ae63ae3a36ed01724674e8f3d750dc47bf125410`.
It checks the source download's SHA-256 hash.
The decoder supports CRC64 and limits the LZMA2 dictionary to 64 MiB.
See the [XZ Embedded license](licenses/xz-embedded-0BSD.txt).

The USTAR reader, JSON reader, SHA-256 implementation and VCDIFF decoder use project code in `src/mods/`.
The VCDIFF decoder follows [RFC 3284](https://www.rfc-editor.org/rfc/rfc3284).
It also supports xdelta3 application headers and Adler-32 checks for each window.
The OpenShao-Mods release builder uses xdelta3 with `-S none`.
The game applies patches without an xdelta3 executable.

## Routine scripts

Routine mods use [Lua 5.4.9](https://www.lua.org/) under the MIT license.
The build checks the source archive's SHA-256 hash.
The runtime exposes checked guest memory and registers to scripts.
Scripts have memory and instruction limits. Host files, processes and networking are unavailable.
See the [Lua license](licenses/Lua-MIT.txt).

## Downloads

HTTPS downloads use WinHTTP or the system's `libcurl.so.4`.
The runtime opens libcurl dynamically.
The download code checks certificates and restricts redirects to HTTPS.
Release packages do not include libcurl.

## References

Public xemu, Cxbx-Reloaded, nxdk and Microsoft documentation informed behavior and ABI checks.
These references do not imply endorsement of this port.
The source repository excludes game images, assets, generated translations, analysis databases and proprietary SDK files.
