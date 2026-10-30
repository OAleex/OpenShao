# OpenShao Agent Guide

This file defines instructions for coding agents in this repository.
Read `README.md` for user features and `docs/BUILDING.md` for build instructions.

## Project scope

- Use **OpenShao** as the project name.
- Use `OpenShao` as the executable base name. Add `.exe` when the file format requires it.
- The target is the supported USA/Europe Xbox release of **Mortal Kombat: Shaolin Monks**.
- The project uses XboxXBERecomp, C++20, SDL and Dear ImGui.
- Both targets share one Dear ImGui launcher in `src/launcher.cpp`, drawn with `src/launcher_art.cpp`.
- Windows uses XAudio2 and Win32 windows. Its launcher uses Direct3D 12; the game offers Direct3D 12 and Vulkan.
- Linux uses SDL platform services and Vulkan for the launcher and game.
- Keep the launcher's own visual identity. Do not use the game's logos, artwork, characters, typefaces or trade dress in the launcher, icon or documentation image.
- Without a usable game, the launcher opens on its game page. `--setup` and runs without a launcher window use the system dialogs.
- Updates come from the project's GitHub releases. Verify each package with its SHA-256 before installing it.
- Keep the migration of default folders from releases before 0.3.0. It recognizes them by their contents. Do not add the project's previous names to code or documentation.
- Select platform services at compile time. Select the Windows game renderer at startup. Preserve each target's behavior when changing another target.
- Keep the SDL platform services and Vulkan renderer in `src/linux/`.
- Keep code, identifiers, comments, messages, scripts, tests, and documentation in English.
- Use neutral descriptions in user documentation.
- Name an operating system only when a requirement, procedure, path or license needs the distinction.
- Use short, direct instructions. Keep one action in each procedural sentence.
- Avoid illustrative field values and descriptions of information already visible in the launcher.
- Keep package formats, update instructions and project links in `resources/readme.txt`.
- Preserve intentional localization and character-encoding tests.
- Write comments that explain constraints or non-obvious behavior. Avoid comments that repeat the code.
- Document implemented features. Do not describe full campaign compatibility as verified without gameplay evidence.
- The maintainer reports a completed campaign playthrough. Most graphical issues are fixed; some can still appear.
- Mention tools and dependencies in project documentation. Avoid comparisons or credits to other game recompilation projects.

## Git and collaboration

- Preserve the owner's staging area and unrelated changes.
- Use `git --no-optional-locks` for read-only status and diff commands.
- Do not stage, unstage, commit, tag, push, or publish without an explicit request for that operation.
- Do not reset, restore, clean, stash, switch branches, or change remotes without an explicit request.
- A request to fix, finish, build, or clean the project does not authorize changes to the Git index.
- Do not create remote issues, pull requests, or messages without an explicit request.
- Keep local investigation notes and generated reports outside maintained documentation.
- Report completed changes, checks, and remaining limitations accurately.

## Files and dependencies

| Path | Maintained content |
| --- | --- |
| `src/` | Host integration, launcher, input, audio, graphics, storage, and compatibility code |
| `config/` | Supported executable profile and ABI metadata |
| `resources/` | Application resources and player instructions |
| `cmake/` | Shared modules and target build configuration |
| `tests/` | Regression tests and manual diagnostic scripts |
| `tools/` | Build, SDK adaptation, and release scripts |
| `docs/` | Instructions and the approved README image |
| `licenses/` | Dependency licenses and notices |

- Keep the SDK at the revision recorded in `UPSTREAM.md` and the build configuration.
- Put permanent SDK adaptations in `tools/prepare_recomp_runtime.py` or maintained integration code.
- Do not change the SDK checkout or generated translation as the permanent fix.
- Treat `build/`, `work/`, `dist/`, and downloaded dependencies as local output.
- Keep machine-specific paths in ignored local wrappers. Quote paths in scripts.
- The ignored `build.bat` is a local convenience. Public build instructions use `tools/build_recomp.py`.

## Game files, user data, and licenses

- Users supply their own original game files. Do not change those files during import or testing.
- Keep original disc images, XBE files and extracted game assets outside public source and all release packages.
- Keep generated translations outside public source and base release packages. Build mod archives through the OpenShao-Mods release tool.
- Keep saves, settings, logs, captures, crash dumps, analysis databases, and local toolchains outside public source.
- The approved documentation image is `docs/images/openshao.png`. Preserve it unless the owner requests a replacement.
- Preserve the project's GPL-2.0-or-later terms and all required third-party notices.
- The project license does not license the original game's code, assets, names, or artwork.
- Use `paths::Root()` for application data and `paths::SaveRoot()` for the configured saves folder.
- Preserve the system defaults listed in the [README](README.md#where-are-my-saves-settings-and-logs).
- Keep `paths.toml` in the default data folder, even after the data folder moves.
- Keep installed mods under `paths::Mods()` (`paths::Root()/mods`), with enabled state in `installed.toml`.
- Apply pending folder moves in `paths::Initialize` before logs open.
- Check copied files before removing source files.
- Preserve prior locations when move preparation fails.
- Update imported ZIP game paths when data moves.
- Preserve the priority of `--data-dir`, `--save-dir` and `--portable-saves`.
- Diagnostic and read-only path queries must not move files.
- Prepare and check complete mod installations before replacing installed files.
- Recover interrupted transactions before changing installed mods.
- Preserve existing saves and settings during updates. Never delete user data as routine cleanup.
- Inspect the exact target before cleanup. Delete only artifacts within the authorized scope.

## Runtime behavior

- Preserve automatic input assignment. Do not restore manual keyboard-player or multiplayer-mode configuration variants.
- In solo play, the keyboard, mouse, and first controller share player 1.
- Ko-op assigns the keyboard and mouse to player 1 with one controller assigned to player 2.
- With two controllers at Ko-op entry, the controllers control players 1 and 2.
- Preserve controller connection, disconnection, reconnection, and focus handling.
- Mouse input shares the keyboard's player slot. F10 and focus loss release pointer capture.
- Capture the pointer only while the interactive game window has focus. Keep it inside the selected monitor.
- Preserve visible title-bar controls in windowed mode.
- Show game selection before the tabs when the remembered game is unavailable. Preserve settings-only `--configure` behavior.
- Keep ISO and XISO reading in the runtime. Do not add extract-xiso or a separate installer dependency.
- Preserve original timing, camera restrictions, save behavior, and visual intent when changing host integration.
- A higher host FPS limit alone does not prove accurate gameplay timing at that rate.
- Support hardware claims with capability checks and reproducible evidence.

## Validation

- Run automated checks in the background with isolated configuration, saves, and logs.
- Use the existing private-desktop infrastructure for native dialog tests.
- Automated tests must not steal focus, move or confine the cursor, produce audio, or poll physical controllers.
- Use synthetic fixtures for file operations. Real-disc regressions may read the external game with isolated writable data.
- Launch a visible game only when the owner requests it.
- Add a focused regression check when a behavior change needs one.
- For documentation, inspect links, filenames, commands, and consistency. Avoid unrelated rebuilds.
- Run the affected CTest checks after runtime changes. Run broader checks when integration risks justify them.
- Report automated checks separately from physical-device, visual, or full-campaign validation.

Use the commands and target paths in [Building](docs/BUILDING.md).
Successful builds create a release package and `checksums.txt` automatically.
Configuration-only runs skip packaging.
GitHub supplies source archives for release tags.
Each package contains the executable, `readme.txt` and `licenses/`, including the project license text.
The `.tar.gz` puts those files in one `OpenShao` folder.
Keep `README.md`, `UPSTREAM.md`, `AGENTS.md`, and `docs/` in the source repository, outside the packages.
Maintain the packaged player instructions in `resources/readme.txt`. Preserve the explicit package file list.
Do not publish packages as part of local validation.

## AI-assisted changes

AI assistance does not replace maintainer review or testing.
Check proposed changes against the source, supported game behavior, and relevant documentation.
Do not invent test results, hardware coverage, compatibility claims, or upstream attribution.
Keep the README's AI disclosure accurate as the workflow changes.
