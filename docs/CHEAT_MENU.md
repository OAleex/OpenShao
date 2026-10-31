# Cheat menu, Save Game and save statue health

This page documents three features that are not part of the original game:

| Feature | Where | Default |
| --- | --- | --- |
| Cheat menu | In-game overlay, enabled in the main menu's Enhancements | Off |
| Save Game | Pause menu | Always listed |
| Full Health at Statue | Main menu's Enhancements | Off |

They apply to the supported USA/Europe release. The cheats write to the running game, not to the executable or the game files.

## Cheat menu

### Enable it

1. Open **Enhancements** in the main menu.
2. Turn on **Cheats**.
3. Choose a **Cheat Menu Key**, F1 to F12. The default is F8.
4. Select **Save Changes**.

F9 shows the FPS display and F10 releases the mouse. Avoid both as the Cheat Menu Key.

### Open and use it

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

### Screens

| Screen | Entries |
| --- | --- |
| Cheat Menu | Infinite Health, Player 1, Player 2, Settings |
| Player 1 and Player 2 | Infinite XP, Infinite Energy, Infinite Fatality |
| Settings | Position X, Position Y, Reset Position |

### Cheats

| Cheat | Effect | Notes |
| --- | --- | --- |
| Infinite Health | Keeps health at its maximum. | Health is shared by both players, so one switch covers both. |
| Infinite XP | Sets the player's XP to 999999 whenever it is lower. | One switch for each player. |
| Infinite Energy | Keeps the player's special bar full. | One switch for each player. |
| Infinite Fatality | Keeps the player's Fatality, Multality and Brutality meter full. | The three bars share one meter, so all three are full. The meter is refilled continuously, so Brutality does not drain it. |

Cheats change progress values in the running game. If you save while a cheat is on, the save can keep the changed values. Keep a backup of your saves before using them.

### Move the menu

Open **Settings** in the cheat menu.

- **Position X** and **Position Y** place the menu. Left and Right change the value in steps of five.
- The values are percentages of the free space on screen. Zero is the left or top edge and 100 is the right or bottom edge.
- **Reset Position** returns the menu to the upper right corner.
- The menu moves while you adjust it.

The position is saved when it changes and is used again the next time the menu opens, including in a new game session.

## Save Game

**Save Game** is in the pause menu, between Options and Konfigure Recompilation.

1. Open the pause menu.
2. Select **Save Game**.

The pause menu closes and the game's own save menu opens. It is the same menu the save statues use, so progress can be saved away from a statue.
OpenShao does not change what the game stores or where the game places you after loading.

## Full Health at Statue

This option is in the main menu's **Enhancements** and is off by default.
When it is off, the save statues heal as in the original game.

When it is on, using a save statue fills the health bar completely. Damage works normally afterwards.

How it works:

- The option reacts only to the statue being used.
- For one second after the statue is used, health is held at a high value for both players. The game then returns health to its normal maximum.
- It does not need the cheat menu and works with Cheats turned off.
- Health is shared in Ko-op, so one use covers both players.

## Settings reference

The options are saved in the `[enhancements]` table of the settings file.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `cheats_enabled` | boolean | `false` | Enables the cheat menu. |
| `cheat_menu_key` | integer | `119` | Virtual-key code of the Cheat Menu Key. |
| `statue_full_heal` | boolean | `false` | Fills health when a save statue is used. |
| `cheat_menu_x` | integer | `100` | Horizontal menu position, 0 to 100 percent. |
| `cheat_menu_y` | integer | `5` | Vertical menu position, 0 to 100 percent. |

Values outside the range are clamped.

## Implementation notes

The code is in `src/cheat_menu.cpp` and `src/cheat_menu.h`. The overlay is drawn with Dear ImGui and takes its keys from the platform input code.
The overlay uses the launcher typeface, baked at several sizes before the renderer builds its font atlas. Text is drawn from the nearest larger size, so it stays sharp at different resolutions.

The cheats and the statue option write to fixed guest addresses of the supported game.

| Value | Location |
| --- | --- |
| Health | Player component read from the player entity at offset 0x200, fields at offsets 0x368 and 0x36C. The entities start at 0x5D1AB8 and are 0x3238 bytes apart. |
| XP | 0x81798240 for player 1 and 0x81798290 for player 2. |
| Energy | Float table at 0x4E65A8 for player 1 and 0x4E65CC for player 2. A full bar is 100. |
| Fatality meter | 0x81798254 for player 1 and 0x817982A4 for player 2. A full meter is 1200, three bars of 400. |

The XP and Fatality values sit in each player's data block, which is 0x50 bytes apart for the two players.

The save statue is detected by a hook on the game's player reaction function. Reaction 0xF is the statue.
Save Game calls the game's save menu routine after the pause menu closes. It sets the acting player to player 1 first.
Both features use hooks in the recompiled code, so they follow the original game logic and do not patch the game files.

Automated checks cover the settings, the pause menu layout and the text catalog. The overlay itself has been tested by hand on Linux and Windows.
