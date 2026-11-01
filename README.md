# OpenShao

<p align="center">
  <img src="docs/images/openshao.png" width="240" alt="OpenShao logo">
</p>

<p align="center">
  <a href="https://github.com/OAleex/OpenShao/releases/latest"><img src="https://img.shields.io/badge/Download-Latest-2563eb?style=for-the-badge&amp;logo=github&amp;logoColor=white" alt="Download the latest release"></a>
  <a href="https://github.com/OAleex/OpenShao/releases/latest"><img src="https://img.shields.io/github/v/release/OAleex/OpenShao?style=for-the-badge&amp;logo=github&amp;label=Latest%20release&amp;cacheSeconds=300" alt="Latest stable release"></a>
  <a href="https://github.com/OAleex/OpenShao/releases"><img src="https://img.shields.io/github/downloads/OAleex/OpenShao/total?style=for-the-badge&amp;logo=github&amp;label=Release%20downloads" alt="Total GitHub release downloads"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-GPL--2.0--or--later-blue?style=for-the-badge" alt="GPL-2.0-or-later license"></a>
</p>

OpenShao is a native PC port of **Mortal Kombat: Shaolin Monks**, made by statically recompiling the original Xbox game.

The goal is to fix bugs, improve the experience and restore cut content while preserving
the game's identity, atmosphere and gameplay. Restorations use evidence from the original
game and its development.

[Contributing](CONTRIBUTING.md) · [AI usage policy](AI_POLICY.md) · [Release notes](https://github.com/OAleex/OpenShao/releases)

> [!IMPORTANT]
> OpenShao does not include the game. You need your own **USA/Europe Xbox copy**.

## Requirements

- Windows 10 1903 or later, or Linux with glibc 2.38 or later
- x64 CPU with SSE4.2 and POPCNT
- Direct3D 12 GPU, or Vulkan 1.2 with dynamic rendering and timeline semaphores
- Controller, or keyboard and mouse
- On Linux, the system `libcurl` for mod and update downloads

## Install

1. Download the [latest release](https://github.com/OAleex/OpenShao/releases/latest) for your system.
2. Extract it to a folder.
3. Open **OpenShao**.
4. Choose your game: an ISO, XISO, ZIP, extracted folder or `default.xbe`.
5. Select **Play**.

Keep the game image or folder where it is. OpenShao reads it in place.

## Update

The launcher checks for a new version when it opens. Select **Update**.
The launcher installs the new version and restarts OpenShao.
The update preserves saves, settings and mods.
Select **Release notes** on the Play page to see what changed in the installed version.

To update manually, extract the new release to a new folder.
OpenShao migrates recognized data and saves from earlier releases to the current folders.

## Features

- Widescreen and ultrawide support, with the original 4:3 as default.
- Adjustable render scale, anisotropic filtering and V-Sync, with Direct3D 12 or Vulkan on Windows and Vulkan on Linux.
- Gameplay output up to 240 FPS, with simulation at 60 Hz and menus at 60 FPS.
- Remappable controller, keyboard and mouse controls, including mouse camera support.
- Local Ko-op with two controllers or keyboard and mouse plus one controller.
- Nine-level Survival Mode for single player or Ko-op, with the shared difficulty options.
- In-game display, graphics, controls and audio settings through **Konfigure Recompilation**.
- Mono, stereo and discrete PCM 5.1 audio on compatible devices.
- Restored missing and unused dialogue using only the original recordings. Lines whose original
  trigger is lost are placed by their map, scene and script context; this is optional.
- Optional enhancements, including Easy and Monk difficulties and cinematic bars, plus ten pages of save slots.
- **Cheat menu** in Konfigure Recompilation's Cheats category, off by default. It opens over gameplay with a configurable key and offers infinite health, XP, energy and Fatality meter. Its position on screen is adjustable and saved. See [Cheat menu](docs/CHEAT_MENU.md).
- **Full Health at Statue** in the main menu's Enhancements, off by default. Using a save statue fills the health bar completely. See [Save Game and Full Health at Statue](docs/SAVE_AND_STATUE.md).
- **Pause menu** with Continue, Options, Save Game, Konfigure Recompilation, Exit Game and Exit to Desktop. Exit Game returns to the main menu. Save Game opens the game's save menu anywhere (see [docs](docs/SAVE_AND_STATUE.md)).
- Separate text and voice languages, with an optional Brazilian Portuguese fan translation and dub as a language pack mod.
- A launcher with [mods](https://github.com/OAleex/OpenShao-Mods), automatic updates and optional Discord Rich Presence.

Dialogue restoration, difficulty options and cinematic bars are in **Enhancements** in the main menu.
Select **Game Mode > Survival Mode** to choose single player or Ko-op, select from your unlocked Versus fighters, and then choose the difficulty. Confirm the framed rules inside Level 1 with **A**.

Survival Mode starts locked. Find its secret in Story Mode, then save your game. The unlock then applies to every save.

Version-specific changes are in the [release notes](https://github.com/OAleex/OpenShao/releases).

## Known issues

- Some graphical issues remain. [Report a problem](#how-do-i-report-a-problem) with steps to reproduce it and a screenshot.
- Mortal Kombat II and The Suffering demo do not run.
- Some effects can cause a brief pause on first use while shaders compile.

## FAQ

### The launcher says my game is invalid

Use the original USA/Europe Xbox release. Renamed or converted images from other releases are not supported.
The check covers the executable, archives and required movies.

### Where are my saves, settings and logs?

Open **Files** in the launcher. Each folder has an **Open** button, and **Move** places it somewhere else.

| System | Data | Saves |
| --- | --- | --- |
| Windows | `%LOCALAPPDATA%\OpenShao` | `Saved Games\OpenShao` |
| Linux | `~/.local/share/OpenShao` | `~/.local/share/OpenShao/saves` |

### The game does not start after I changed the display settings

Open the launcher. Select **Files > Video > Restore**.

### What are the keyboard and mouse controls?

| Input | Key or button |
| --- | --- |
| Move | W A S D |
| D-pad | Arrow keys |
| A / B | Space / Esc |
| X / Y | J or left click / K or middle click |
| Black / White | C / Q |
| Triggers | Shift / E |
| Start / Back | Enter / Backspace |
| Camera | Mouse |
| FPS display | F9 |
| Release or capture the mouse | F10 |

Change them in **Konfigure Recompilation > Controls**. Each action has a primary and an alternate key.

### How do I use the cheat menu?

1. Turn on **Cheats** in the main menu's **Konfigure Recompilation > Cheats**.
2. Press the Cheat Menu Key during gameplay. The default is F8.
3. Move with the arrow keys and press Enter to switch a cheat.
4. Press Backspace to go back or close the menu.

Choose the key in the same category and move the menu from the menu's Settings screen. See [Cheat menu](docs/CHEAT_MENU.md) for every option.

### How do controllers and Ko-op work?

Xbox, PlayStation and other SDL-compatible controllers work. The game shows Xbox buttons.
In single player, the keyboard, mouse and first controller all control player 1.
In Ko-op, the keyboard and mouse control player 1. One controller controls player 2.
With two controllers, each controller controls one player.

If player 1's controller disconnects during Ko-op, the keyboard and mouse control player 1.
Connected controllers keep their player slots. A replacement fills an empty slot.
Any connected controller can navigate Konfigure Recompilation and remap controls.

For a controller without a mapping, put an SDL `gamecontrollerdb.txt` in the data folder.

### How do I install mods or the Brazilian Portuguese fan translation?

Open **Mods** in the launcher. Select **Install**.
For a downloaded `.tar.xz`, use **Install from file**.
Choose the language in **Konfigure Recompilation > Language**.
Mod authors can read the [mod guide](docs/MODS.md).

### How can I improve performance?

Lower **Render scale** in **Konfigure Recompilation > Graphics**. Press F9 to see the frame rate.
V-Sync targets the monitor's refresh rate. When V-Sync is disabled, choose a cap from 60 to 240 FPS.
The achieved frame rate depends on your hardware and the scene.

### Why does OpenShao bring the open window forward instead of starting?

Only one copy can run at a time with the same data folder.

### Are there command-line options?

| Option | Effect |
| --- | --- |
| `--configure` | Open only the launcher settings |
| `--no-launcher` | Start the game with the saved settings |
| `--setup` | Choose the game again with the system dialogs |
| `--disc <image>`, `--game <folder>` | Play another copy of the game for this session |
| `--data-dir <path>` | Use another data folder for this session |
| `--save-dir <path>` | Use another saves folder for this session |
| `--portable-saves` | Keep saves in `hdd` next to the executable |
| `--paths` | Print the folders in use |
| `--support-report` | Create a filtered diagnostic ZIP without opening the launcher |

### How do I report a problem?

1. Open **Support** in the launcher.
2. Select **Generate report** to create a diagnostic ZIP.
3. Review the report before sharing it.
4. Select **Report a bug** to open the GitHub issue form.
   The form receives your OpenShao version and settings. A report with a crash dump opens the crash form.
5. Describe the steps that cause the problem.
6. Attach the reviewed report.

For graphics or audio problems, choose that form on GitHub. **Report a bug** also copies your version and settings to the clipboard.

Use the [new feature form](https://github.com/OAleex/OpenShao/issues/new?template=feature_request.yml) to request something new.
Use the [quality-of-life form](https://github.com/OAleex/OpenShao/issues/new?template=quality_of_life.yml) to suggest an improvement to an existing feature.

OpenShao saves reports in the data folder under `reports`.

Reports include recent logs, selected settings, mod versions and a crash dump when available.
Exported text logs filter personal paths, account names and credentials.
Reports exclude saves, game files and screenshots. Remove crash dumps and any private data before posting the archive.
Add a screenshot separately for graphical problems. Keep raw rendering captures local.

After a crash, the error message shows the report's location. Select **OK** to open its folder.
OpenShao does not upload reports automatically.

## Contributing

Read the [contribution guide](CONTRIBUTING.md) for project scope, PR expectations and rules
on game files and private data. Fixes, testing and restorations supported by evidence are welcome.

## Building

See [Building](docs/BUILDING.md) for requirements, commands and packaging.
Dependency versions and notices are in [Tools and dependencies](UPSTREAM.md).

## AI usage

Development uses AI assistance for analysis, code, debugging, tests and documentation.
The maintainer follows the same rules as contributors: every change is reviewed, understood
and tested in game before release, and design decisions are made by people, not by AI.
Contributors must understand, review and test their work. They must disclose how they used AI.
Read the [AI usage policy](AI_POLICY.md).

## Credits

Built with [XboxXBERecomp](https://github.com/TGP482/XboxXBERecomp). See [tools and dependencies](UPSTREAM.md), [third-party notices](licenses/THIRD_PARTY.txt) and the [Brazilian Portuguese fan translation credits](docs/TRANSLATION_CREDITS.md).

The code is licensed under [GPL-2.0-or-later](LICENSE). The license does not cover the original game or its assets.
OpenShao is not affiliated with or endorsed by the owners of Mortal Kombat or Xbox.
