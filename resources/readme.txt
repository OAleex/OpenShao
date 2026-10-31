OpenShao
A native PC port of Mortal Kombat: Shaolin Monks.

Fixes, improvements and restored cut content, with respect for the original game.
The game is not included. You need your own USA/Europe Xbox copy.


REQUIREMENTS

- Windows 10 1903 or later, or Linux with glibc 2.38 or later
- x64 CPU with SSE4.2 and POPCNT
- Direct3D 12 GPU, or Vulkan 1.2 with dynamic rendering and timeline semaphores


GETTING STARTED

1. Open OpenShao.
2. Choose your game: an ISO, XISO, ZIP, extracted folder or default.xbe.
3. Select Play.

Keep the game image or folder where it is. OpenShao reads it in place.
On Windows, choose Direct3D 12 or Vulkan under Graphics API on the launcher's Play page.
Direct3D 12 is the default. Linux uses Vulkan.


UPDATES

The launcher checks for a new version when it opens. Select Update to install it.
The update preserves saves, settings and mods.
Select Release notes on the Play page to open the installed version's notes on GitHub.
To turn off the check, open Files and clear Updates.

To update manually, download the package for your system and extract it to a new folder:
Windows: OpenShao-<version>-Windows-x64.zip
Linux:   OpenShao-<version>-Linux-x64.tar.gz
https://github.com/OAleex/OpenShao/releases/latest


SETTINGS AND CONTROLS

Game settings are in Konfigure Recompilation, in the main menu and in the SELECT pause menu.
Use it to change display, graphics, controls and audio.
Select text and voice languages from the main menu.
Main-menu settings use Save or Discard. Pause settings apply immediately.

Audio Output offers Mono, Stereo and Surround. Stereo is the default.
For discrete PCM 5.1, configure and test your system's speakers before selecting Surround.
Devices without six-channel output use a stereo downmix.

V-Sync targets the monitor's refresh rate. With it disabled, choose 60, 120, 144, 165 or 240 FPS.
The achieved frame rate depends on hardware and the scene. Menus run at up to 60 FPS.

The main menu's Enhancements contains Restore Lost Dialogue, Improved Difficulty and Cinematic Bars.
In the same page, Cheats enables a cheat menu that opens over gameplay with the Cheat Menu Key, F8 by default. It is off by default.
Use the arrow keys to move, Enter to switch a cheat and Backspace to go back. The menu's Settings screen moves it on screen.
Full Health at Statue fills the health bar when a save statue is used. It is off by default.
The pause menu lists Continue, Options, Save Game, Konfigure Recompilation, Exit Game and Exit to Desktop. Exit Game returns to the main menu. Save Game opens the game's save menu anywhere.

Load Game and Save Game have ten pages of nine slots. Use the triggers to turn pages.
Press X to delete a selected save after confirmation.

Mods and language packs are in the launcher's Mods page.
Optional Discord Rich Presence is in Files > Discord. It requires the Discord desktop app and activity sharing.

In Ko-op, use keyboard and mouse for player 1 and one controller for player 2, or use two controllers.
If player 1's controller disconnects, the keyboard and mouse control player 1.
Connected controllers keep their player slots. A replacement fills an empty slot.
Any connected controller can navigate Konfigure Recompilation and remap controls.

In Versus, Start skips character introductions. Release it before pressing Start again to pause.

Select Game Mode > Survival Mode for nine levels in single player or Ko-op.
Survival Mode starts locked. Find its secret in Story Mode, then save your game.
The unlock then applies to every save.
Choose single player or Ko-op, select an unlocked Versus fighter, and then choose difficulty.
Inside Level 1, read the framed rules and press A to begin.

F9   show the version and frame rate
F10  release or capture the mouse

If the game does not start after a display change, open the launcher.
Select Files > Video > Restore.


FOLDERS

Open Files in the launcher to open or move your saves and data.
OpenShao stores diagnostic ZIP files in the data folder under reports.


KNOWN ISSUES

Some graphical issues remain. Report them with steps to reproduce the problem and a screenshot.
Mortal Kombat II and The Suffering demo do not run.
Some effects can cause a brief pause on first use while shaders compile.


HELP

Project page: https://github.com/OAleex/OpenShao
Open Support in the launcher and select Generate report.
Select Report a bug to open GitHub.
Review the diagnostic ZIP before attaching it. Describe how to reproduce the problem.

Reports include recent logs, selected settings, mod versions and a crash dump when available.
Exported text logs filter personal paths, account names and credentials.
Remove crash dumps and any private data before posting the archive publicly.
Reports exclude saves, game files and screenshots. Add a screenshot separately for visual bugs.

Keep raw rendering captures local. OpenShao does not upload reports automatically.

After a crash, the error message shows the report's location. Select OK to open its folder.

Contributing: https://github.com/OAleex/OpenShao#contributing
OpenShao accepts AI assistance. Contributors must understand, review and test their work.
AI policy: https://github.com/OAleex/OpenShao#ai-usage

Code license: GPL-2.0-or-later. See licenses/LICENSE.txt and the other notices in licenses/.
The license does not cover the original game or its assets.
