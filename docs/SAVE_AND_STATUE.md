# Save Game and Full Health at Statue

These two features are not part of the original game and are not cheats. They apply to the supported USA/Europe release.

| Feature | Where | Default |
| --- | --- | --- |
| Save Game | Pause menu | Always listed |
| Full Health at Statue | Main menu's Enhancements | Off |

## Save Game

**Save Game** is in the pause menu, between Options and Konfigure Recompilation.

1. Open the pause menu.
2. Select **Save Game**.

The pause menu closes and the game's own save menu opens. It is the same menu the save statues use, so progress can be saved away from a statue.
OpenShao does not change what the game stores or where the game places you after loading. It is a manual save, not an autosave.

## Full Health at Statue

This option is in the main menu's **Enhancements** and is off by default.
When it is off, the save statues heal as in the original game.

When it is on, using a save statue fills the health bar completely. Damage works normally afterwards.

- The option reacts only to the statue being used.
- For one second after the statue is used, health is held at a high value for both players. The game then returns health to its normal maximum.
- It does not need the cheat menu and works with Cheats turned off.
- Health is shared in Ko-op, so one use covers both players.

The setting is `statue_full_heal` (boolean, default `false`) in the `[enhancements]` table of the settings file.

## Implementation notes

The statue is detected by a hook on the game's player reaction function. Reaction 0xF is the statue.
Save Game calls the game's save menu routine after the pause menu closes. It sets the acting player to player 1 first.
Both features use hooks in the recompiled code, so they follow the original game logic and do not patch the game files.
The pause layout and the settings pages are covered by automated tests.
