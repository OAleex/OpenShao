# Cheat menu

The cheat menu is not part of the original game. It applies to the supported USA/Europe release and writes to the running game, not to the executable or the game files.
Save Game and Full Health at Statue are documented in [Save Game and Full Health at Statue](SAVE_AND_STATUE.md).

## Enable it

1. Open **Konfigure Recompilation** in the main menu.
2. Open **Cheats**.
3. Turn on **Cheats**.
4. Choose a **Cheat Menu Key**, F1 to F12. The default is F8.
5. Go back and select **Save Changes**.

The same **Cheats** category is in **Konfigure Recompilation** of the pause menu (Cheats and Menu Key rows), and changes there are saved immediately.
The cheats are not part of Enhancements, which stays for improvements to the original experience.

F9 shows the FPS display and F10 releases the mouse. Avoid both as the Cheat Menu Key.

## Open and use it

Press the Cheat Menu Key during gameplay. The menu floats over the game and does not pause it.

| Key | Action |
| --- | --- |
| Up / Down | Move the selection |
| Left / Right | Change a value |
| Enter | Switch a cheat or open a submenu |
| Backspace | Go back, or close the menu from the first screen |

While the menu is open it takes every keyboard key, so the game does not receive them. Controllers keep working.
Closing the menu does not turn the cheats off. A cheat stays on until you switch it off.
Cheats reset when the game restarts.

The menu is drawn in the language selected for the game. Missing translations use English.

## Screens

| Screen | Entries |
| --- | --- |
| Cheat Menu | Infinite Health, Player 1, Player 2, Settings |
| Player 1 and Player 2 | Infinite XP, Infinite Energy, Infinite Fatality |
| Settings | Position X, Position Y, Reset Position |

## Cheats

| Cheat | Effect | Notes |
| --- | --- | --- |
| Infinite Health | Keeps health at its maximum. | Health is shared by both players, so one switch covers both. |
| Infinite XP | Sets the player's XP to 999999 whenever it is lower. | One switch for each player. |
| Infinite Energy | Keeps the player's special bar full. | One switch for each player. |
| Infinite Fatality | Keeps the player's Fatality, Multality and Brutality meter full. | The three bars share one meter, so all three are full. The meter is refilled continuously, so Brutality does not drain it. |

Cheats change progress values in the running game. If you save while a cheat is on, the save can keep the changed values. Keep a backup of your saves before using them.

## Move the menu

Open **Settings** in the cheat menu.

- **Position X** and **Position Y** place the menu. Left and Right change the value in steps of five.
- The values are percentages of the free space on screen. Zero is the left or top edge and 100 is the right or bottom edge.
- **Reset Position** returns the menu to the upper right corner.
- The menu moves while you adjust it.

The position is saved when it changes and is used again the next time the menu opens, including in a new game session.

## Settings reference

The cheat options are saved in the `[enhancements]` table of the settings file.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `cheats_enabled` | boolean | `false` | Enables the cheat menu. |
| `cheat_menu_key` | integer | `119` | Virtual-key code of the Cheat Menu Key. |
| `cheat_menu_x` | integer | `100` | Horizontal menu position, 0 to 100 percent. |
| `cheat_menu_y` | integer | `5` | Vertical menu position, 0 to 100 percent. |

Values outside the range are clamped.

## Implementation notes

The code is in `src/cheat_menu.cpp` and `src/cheat_menu.h`. The overlay is drawn with Dear ImGui and takes its keys from the platform input code.
The overlay uses the launcher typeface, baked at several sizes before the renderer builds its font atlas. Text is drawn from the nearest larger size, so it stays sharp at different resolutions.

The cheats write to fixed guest addresses of the supported game.

| Value | Location |
| --- | --- |
| Health | Player component read from the player entity at offset 0x200, fields at offsets 0x368 and 0x36C. The entities start at 0x5D1AB8 and are 0x3238 bytes apart. |
| XP | 0x81798240 for player 1 and 0x81798290 for player 2. |
| Energy | Float table at 0x4E65A8 for player 1 and 0x4E65CC for player 2. A full bar is 100. |
| Fatality meter | 0x81798254 for player 1 and 0x817982A4 for player 2. A full meter is 1200, three bars of 400. |

The XP and Fatality values sit in each player's data block, which is 0x50 bytes apart for the two players.

The cheats use hooks in the recompiled code and do not patch the game files.

Automated checks cover the settings pages and the text catalog. The overlay itself has been tested by hand on Linux and Windows.
