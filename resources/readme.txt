OpenShao
A native PC port of Mortal Kombat: Shaolin Monks.

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
Saves, settings and mods are kept.
To turn off the check, open Files and clear Updates.

To update manually, download the package for your system and extract it to a new folder:
Windows: OpenShao-<version>-Windows-x64.zip
Linux:   OpenShao-<version>-Linux-x64.tar.gz
https://github.com/OAleex/OpenShao/releases/latest


SETTINGS AND CONTROLS

Game settings are in Konfigure Recompilation, in the main menu and in the SELECT pause menu.
With V-Sync enabled, Graphics targets the current monitor's refresh rate. With V-Sync disabled, choose 60 or 120 FPS. Simulation stays at 60 Hz.
In the main menu's Enhancements, Cinematic Bars adds animated black bands to in-game cutscenes. It is off by default.
Solo campaign health, combo counters, multipliers and praise follow the upper-left screen corner. Tutorial panels, text and confirmation prompts follow the bottom edge without stretching the artwork.
The achieved frame rate depends on performance and the display's refresh rate when V-Sync is enabled.
The pause menu lists Continue, Options, Konfigure Recompilation, Exit Game and Exit to Desktop. Exit Game returns to the main menu.
Mods and language packs are in the launcher's Mods page.

In Ko-op, use keyboard and mouse for player 1 and one controller for player 2, or use two controllers.
If player 1's controller disconnects, the keyboard and mouse take over player 1.
Connected controllers keep their player slots. A replacement fills an empty slot.
Any connected controller can navigate Konfigure Recompilation and remap controls.
In Versus, Start skips character introductions. Release it before pressing Start again to pause.

F9   show the version and frame rate
F10  release or capture the mouse

If the game does not start after a display change, open the launcher and select Files > Video > Restore.


FOLDERS

Open Files in the launcher to open or move your saves and data.
Diagnostic ZIP files are stored in the data folder under reports.


KNOWN ISSUES

Most graphical issues have been fixed, but some can still appear.
Any problem is worth reporting. Open an issue with a screenshot: https://github.com/OAleex/OpenShao/issues
Mortal Kombat II and The Suffering demo do not run.
Supported asset pipelines are prepared while an area loads, including on the first run.
Common menu, particle and screen-effect pipelines are prepared when their shaders load.
Other effects can still pause briefly on first use. Observed pipelines are reused in later sessions.


HELP

Project page: https://github.com/OAleex/OpenShao
Open Support in the launcher and select Generate report.
Select Report a bug to open GitHub.
Attach the diagnostic ZIP and describe how to reproduce the problem.

Reports include up to three session logs, selected settings and installed mod versions.
Manual reports also include the latest completed crash dump when available.
Automatic crash reports include the dump captured for that crash when one can be created.
New session logs and exported text logs filter account and computer names, personal paths, network addresses and credentials.
ZIP reports exclude saves, game files and screenshots. Nothing is uploaded automatically.

After an unexpected launcher or game exit, a separate process creates the crash ZIP immediately.
The error message shows the ZIP location. Select OK to open its folder.
Reopening the launcher is not required to create the crash report.

Development uses AI assistance. The maintainer reviews every change.

Code license: GPL-2.0-or-later. See licenses/LICENSE.txt and the other notices in licenses/.
The license does not cover the original game or its assets.
