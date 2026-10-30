# Menu text

The main menu, Options, Audio, Gameplay and Kontent captions use the game's decorative font.
Character Select, Level Select and Load Game headings use the same style.
Submenu headings use gold lettering. Menu items retain the original animation opacity and selection effect.
The renderer reads glyphs from the player's game files at runtime.
No font textures or menu artwork are included in the source catalog.
Konfigure Recompilation uses the original runtime A/B icon resources for its footer and display confirmation.
Localized action captions remain in JSON, separately from the icons.
Adjustment hints use the same icon-font direction pair as the original Kontent gallery.
The Language page has separate Text Language and Voice Language rows with language autonyms from the catalog.
Voice Language offers independent English, French, German, Spanish, Italian and Brazilian Portuguese choices. Native brightness and gameplay controls remain in Options.
Portuguese boolean values use Sim and Não; the other localized boolean terms retain their existing translations.

Edit `resources/menu_text.json` to change a caption, then rebuild the executable.
CMake embeds the UTF-8 catalog in both targets. Packages do not need a separate catalog file.

Each language uses the same semantic keys. The supported locales are `en`, `fr`, `de`, `es`, `it` and `pt-BR`.
Unknown locales fall back to English. Unknown graphic identifiers retain their original rendering.
The `graphics` table associates native resource identifiers with caption keys.
These identifiers select text sprites; background art, ornaments, selection effects and gameplay text keep their existing paths.

The `system.*` keys supply the twelve native system-table entries and the executable's wait caption.
Controller reconnection and corrupt-save prompts retain the original localized wording and button tokens where applicable.
Storage and game-file errors describe PC files and folders. The storage prompt opens the configured saves folder instead of requesting the Xbox dashboard.
The native loader copies the serialized JSON text into its own allocations; source resources and installed packs remain unchanged.
System messages allow up to 255 encoded bytes. Menu captions retain their 95-byte limit.
SELECT settings row labels are clipped to 24 native-encoded bytes to fit the original pause frame.

Retail captions follow the corresponding localized menu artwork.
Portuguese captions follow the installed community translation, with standard Unicode accents in the catalog.
The Portuguese language pack remains required for its glyphs and other translated game content.
New `recomp.*`, `pause.*`, `settings.*`, `binding.*`, `mouse.*`, `value.*`, `source.*`, `language.*` and `key.*` captions are port translations.
They label the functional Konfigure Recompilation screen and its input capture, confirmation and persistence feedback.

Titles are measured with the native font, including kerning, before layout.
Long captions shrink to fit their columns. The underline follows the translated title width.
Its line starts at the original viewport edge across aspect ratios, while its arrow cap retains its original proportions.
The display confirmation clamps the question mark's atlas bounds to avoid sampling neighboring glyph pixels.
The added main-menu entries follow the native timeline position, line spacing and opacity during entry, exit and return.
Exit to Desktop appears below the separate Enhancements entry in the main menu and uses the native Options scene for its localized confirmation.
No is selected initially. No or B restores the Exit row; Yes requests the normal window shutdown.
Internal settings pages play the original Options-to-Audio and Audio-to-Options clips.
Their captions follow the original title and row transforms and opacity; no host-time page animation is added.
Saving a different text or voice language prepares a replacement process, saves the complete draft and closes the previous session.
The replacement waits for the old process to release the data and save locks before loading the saved settings.
It initializes the native profile and save-space state before exposing the main menu, including when the profile does not exist yet.
All six text languages and every voice selection use this path. It skips startup movies and uses the retail return-to-main frontend route once.
Saving the active text and voice choices applies settings without a restart. Display confirmation precedes any language restart.
Preparation or persistence failure keeps the current session open and cancels the prepared process.
Each main-menu settings page retains its last selected row. Returning restores the parent selection, including paginated bindings.
The ornament pointer and selection sparkle share the same row origin after native return clips.

## Gameplay pause menus

SELECT retains the original Continue, Options and Exit Game actions. It also offers **Konfigure Recompilation** and **Exit to Desktop**.
The pause settings use the same embedded JSON catalog and original menu font as the main-menu settings.
Their categories follow the main menu's order: **Display**, **Graphics**, **Controls** and **Audio**.

Controls offers controller and primary/alternate keyboard remapping, mouse settings, dead zone and vibration.
Graphics follows the main menu's order: render scale, anisotropic filtering, V-Sync, Bloom Effect, Show FPS and frame rate.
With V-Sync enabled, frame rate shows the display target and retains the stored software cap for later use.
Display offers display mode, resolution and aspect ratio. Audio offers master volume.
Pause settings apply and save immediately; they do not use the main-menu draft's Save Changes and Discard and Return actions.
Controller and keyboard capture reject conflicting bindings. Mouse buttons and wheel mappings cycle through available actions.
Display mode, resolution and aspect ratio preview changes for 15 seconds and revert unless confirmed.
Language and Enhancements are only accessible from the main menu.

The pause Exit to Desktop prompt defaults to No, warns about unsaved progress and only requests shutdown after confirming Yes.
Leaving the pause settings returns to the original pause menu without discarding previously saved changes.

Run `menu_text_catalog`, `recomp_menu_text`, `recomp_menu_renderer` and `recomp_aspect_hooks` through CTest
to check catalog coverage, encoding, layout, opacity and aspect integration.
Run `recomp_settings_model`, `recomp_input_capture` and `recomp_controller` for draft editing,
raw input capture and preservation of player assignments during live configuration.
On Windows, run `recomp_pause_recomp_model` for pause categories, immediate persistence, remapping and screen confirmation. The native runtime scenarios below also exercise pause navigation and exit behavior.

The opt-in native regression requires the supported extracted game and a title-save fixture.
It copies the fixture, starts an owned hidden window with muted audio and virtual input,
and checks that the source executable and source saves stay unchanged.
Run it from the repository root on Windows:

```powershell
python tests/recomp_settings_runtime_test.py --exe build/release/OpenShao.exe --game "path/to/extracted-game" --save-fixture "path/to/4d570029" --scenario navigation
```

Choose `persistence`, `reload`, `write-failure`, `display`, `remapping`, `prompts`, `transition`, `polish`, `presentation`, `input-handoff`, `details`, `headings`, `restart`, `voices` or `native-options`
to check the other interaction paths. `reload` requires `--settings` with a previously saved
configuration. `display` checks confirmation and the 15-second rollback.
`prompts` captures the native button hints and checks keyboard navigation through a category.
`transition` captures main-menu entry, exit and return alongside the added row's selected state.
`polish` visits every port settings page with virtual controller input.
`presentation` uses keyboard navigation to capture horizontal direction hints and all six language autonyms.
It verifies the original submenu clips and the native font's horizontal arrow glyph mapping.
`details` captures restored parent selections, retained binding pagination and refused controller conflicts.
`headings` captures port settings, Options and its submenus, Load Game, Kontent and Character Select for the selected aspect ratio.
`input-handoff` checks keyboard release and a controller connected after the custom menu opens.
`restart` requires `--mods` and cycles through all six languages with Save Changes, following each owned replacement process.
It checks direct main-menu entry without player input, paths, all other saved settings, copied title saves and unchanged-language persistence.
`voices` keeps the selected text language while saving every voice choice through separate process restarts.
Use `--voice-language` for an initial voice choice in other native scenarios.
Use `--restart-count` to run fewer consecutive changes when continuing an interrupted local validation.
Use `--resume-main-menu` to check the restart startup route without synthetic opening-screen input.
Run `recomp_restart_boot` and `recomp_restart_process` through CTest for the one-shot route and process handoff.
Use `--language` and `--aspect` for a localized aspect combination.
Use `exit-desktop` to check both cancellation paths, selection restoration and confirmed process shutdown.
It also checks preservation of the legacy disabled effect preference during normal shutdown.
Use `pause-recomp` to exercise all five pause settings categories and `pause-recomp-load` for the pause menu after loading a saved game.
Portuguese also requires `--mods` with an installed language pack.
Captures and reports default to ignored `work/recomp-runtime`; do not redistribute the captured artwork.
