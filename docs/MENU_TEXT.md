# Menu text

Edit `resources/menu_text.json` to change a caption. Rebuild the executable after the edit.
CMake embeds the UTF-8 catalog in both targets. Packages do not need a separate catalog file.

Menus use the original decorative font, button icons and animation resources from
the player's game files. The source catalog contains text and identifiers. It does
not include font textures or menu artwork. Follow the [contribution rules](../CONTRIBUTING.md)
and [AI usage policy](../AI_POLICY.md) when editing it.

## Catalog

Each language uses the same semantic keys. The supported locales are `en`, `fr`, `de`, `es`, `it` and `pt-BR`.
Unknown locales use English. Unknown graphic identifiers retain their original rendering.

The `graphics` table associates native resource identifiers with caption keys.
These identifiers select text sprites. Background art, ornaments, selection effects and gameplay text keep their existing paths.

The `system.*` keys supply the twelve native system-table entries and the executable's wait caption.
Controller reconnection and corrupt-save prompts retain the original localized wording and button tokens where applicable.
Storage and game-file errors describe PC files and folders. The storage prompt opens the configured saves folder instead of requesting the Xbox dashboard.

The native loader copies the serialized JSON text into its own allocations. Source resources and installed packs remain unchanged.
System messages allow up to 255 encoded bytes. Menu captions retain their 95-byte limit.
SELECT settings row labels use at most 24 native-encoded bytes to fit the original pause frame.

Retail captions follow the corresponding localized menu artwork.
Portuguese captions follow the installed community translation, with standard Unicode accents in the catalog.
The Portuguese language pack remains required for its glyphs and other translated game content.
New `recomp.*`, `pause.*`, `settings.*`, `binding.*`, `mouse.*`, `value.*`, `source.*`, `language.*` and `key.*` captions are port translations.
They label the functional Konfigure Recompilation screen and its input capture, confirmation and persistence feedback.

## Rendering and settings

Titles use native font metrics and kerning. Long captions shrink to fit, and the
underline follows the translated title width. Added entries follow the original
menu transforms, spacing, opacity and transition clips. Returning to a settings page
restores its selected row and binding page.

Text Language and Voice Language are independent. Changing either saves the draft
and restarts directly into the main menu after releasing the old process's data and
save locks. Unchanged language choices apply without a restart.
Display confirmation precedes the restart. Preparation or persistence failure keeps the current session open.

Exit to Desktop uses the native confirmation scene and defaults to No. Canceling
restores the Exit row. Confirming requests normal shutdown.

## Gameplay pause menus

SELECT retains the original Continue, Options and Exit Game actions. It also offers **Konfigure Recompilation** and **Exit to Desktop**.
The pause settings use the same embedded JSON catalog and original menu font as the main-menu settings.
Their categories follow the main menu's order: **Display**, **Graphics**, **Controls** and **Audio**.

Controls offers controller and primary/alternate keyboard remapping, mouse settings, dead zone and vibration.
Graphics follows the main menu's order: render scale, anisotropic filtering, V-Sync, Bloom Effect, Show FPS and frame rate.
With V-Sync enabled, frame rate shows the display target and retains the stored software cap for later use.
Display offers display mode, resolution and aspect ratio. Audio offers master volume.

Pause settings apply and save immediately.
They do not use the main-menu draft's Save Changes and Discard and Return actions.
Controller and keyboard capture reject conflicting bindings. Mouse buttons and wheel mappings cycle through available actions.
Display mode, resolution and aspect ratio preview changes for 15 seconds and revert unless confirmed.
Language and Enhancements are only accessible from the main menu.

The pause Exit to Desktop prompt defaults to No, warns about unsaved progress and only requests shutdown after confirming Yes.
Leaving the pause settings returns to the original pause menu without discarding previously saved changes.

## Checks

Run these through CTest using the [build instructions](BUILDING.md):

| Checks | Coverage |
| --- | --- |
| `menu_text_catalog`, `recomp_menu_text`, `recomp_menu_renderer`, `recomp_aspect_hooks` | Catalog, encoding, layout, opacity and aspect ratios |
| `recomp_settings_model`, `recomp_input_capture`, `recomp_controller` | Draft settings, input capture and player assignments |
| `recomp_pause_recomp_model` (Windows) | Pause categories, persistence, remapping and display confirmation |
| `recomp_restart_boot`, `recomp_restart_process` | Direct main-menu restart and process handoff |

The opt-in native regression requires the supported extracted game and a title-save fixture.
It copies the fixture and starts an owned hidden window with muted audio and virtual input.
It checks that the source executable and source saves stay unchanged.
Run it from the repository root on Windows:

```powershell
python tests/recomp_settings_runtime_test.py --exe build/release/OpenShao.exe --game "path/to/extracted-game" --save-fixture "path/to/4d570029" --scenario navigation
```

Run the script with `--help` for its scenarios and options.

| Scenario | Purpose |
| --- | --- |
| `display` | Check display confirmation and rollback |
| `remapping` | Check bindings |
| `restart`, `voices` | Check language changes |
| `exit-desktop` | Check shutdown |
| `pause-recomp`, `pause-recomp-load` | Check pause settings |

Use `--language` and `--aspect` for localized layout checks. Portuguese, `restart`
and `voices` require `--mods` with an installed language pack. `reload` needs
`--settings` with a saved configuration.

Captures and reports default to ignored `work/recomp-runtime`. Keep captured game
resources local.
