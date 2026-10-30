"""Validate the distributable menu labels and their native glyph encoding."""
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
LANGUAGES = {"en", "fr", "de", "es", "it", "pt-BR"}
REQUIRED = {
    "settings.cinematic_bars", "settings.cinematic_bars_hint", "value.display_refresh",
    "main.single_player", "main.koop", "main.versus", "main.load_game", "main.options", "main.kontent",
    "main.exit_desktop", "main.exit_confirm", "main.exit_error", "debug.menu",
    "debug.unlock_kontent", "debug.kontent_unlocked", "settings.bloom_effect",
    "recomp.enhancements", "settings.scorpion_unfiltered", "settings.restore_dialogue", "settings.restore_dialogue_hint", "settings.listen_hint", "settings.preview_error",
    "recomp.title", "recomp.display", "recomp.graphics", "recomp.controls", "recomp.audio", "settings.language",
    "options.title", "options.brightness", "options.audio", "options.gameplay", "audio.title",
    "audio.music", "audio.effects", "audio.speech", "gameplay.title", "gameplay.rounds", "gameplay.time",
    "gameplay.camera", "kontent.title", "footer.back", "footer.enter", "footer.cancel",
    "footer.default", "footer.accept",
    "character_select.title", "level_select.title", "load.title",
    "source.none", "source.leftx-", "source.righty+", "source.touchpad",
    "key.0", "key.32", "key.37", "key.38", "key.39", "key.40", "key.46",
}


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"Duplicate menu JSON key: {key}")
        result[key] = value
    return result


class MenuCatalogTest(unittest.TestCase):
    def test_system_messages_and_pc_storage_actions(self):
        required = {"system.wait", "system.reconnect_port", "system.reconnect", "system.reconnect_accept",
                    "system.load_corrupt", "system.save_corrupt", "system.storage_space", "system.disc_error",
                    "system.unsaved_progress", "system.no", "system.yes", "system.navigation"}
        for language, labels in self.catalog["languages"].items():
            with self.subTest(language=language):
                self.assertTrue(required <= labels.keys())
                self.assertEqual(labels["system.reconnect_port"].count("%d"), 1)
                for key in required - {"system.wait", "system.reconnect_port", "system.reconnect", "system.no", "system.yes"}:
                    self.assertIn("=", labels[key], "System actions must preserve native controller icons")
                for key in required:
                    self.assertNotIn("Xbox", labels[key], "PC system messages must not direct players to console storage")
                    self.assertNotIn("?", labels[key], "System messages must preserve accents instead of encoding substitution characters")
        portuguese=self.catalog["languages"]["pt-BR"]
        self.assertEqual(portuguese["system.no"], "N\u00e3o")
        self.assertTrue(portuguese["system.storage_space"].startswith("N\u00e3o foi poss\u00edvel salvar."))
        self.assertTrue(portuguese["system.load_corrupt"].startswith("N\u00e3o foi poss\u00edvel"))
    def setUp(self):
        source = ROOT / "resources/menu_text.json"
        self.assertTrue(source.is_file(), "The public UTF-8 menu catalog must exist")
        self.catalog = json.loads(source.read_text(encoding="utf-8"), object_pairs_hook=unique_object)

    def test_supported_languages_and_complete_keys(self):
        self.assertEqual(self.catalog["version"], 1)
        self.assertEqual(set(self.catalog["languages"]), LANGUAGES)
        keys = set(self.catalog["languages"]["en"])
        self.assertTrue(REQUIRED <= keys)
        for language, labels in self.catalog["languages"].items():
            with self.subTest(language=language):
                self.assertEqual(set(labels), keys)

    def test_labels_are_text_and_fit_native_string_storage(self):
        for language, labels in self.catalog["languages"].items():
            for key, label in labels.items():
                with self.subTest(language=language, key=key):
                    self.assertIsInstance(label, str)
                    self.assertTrue(label.strip())
                    self.assertLessEqual(len(label), 255 if key.startswith("system.") else 95)
                    self.assertNotIn("\x00", label)
                    self.assertNotIn("\n", label)
                    self.assertNotIn("\ufffd", label, "Menu captions must not contain encoding replacement characters")
                    label.encode("cp1252")
                    self.assertTrue(all(ord(character) <= 255 for character in label),
                                    "Each caption must fit the native Latin-1 glyph slots")
                    if key.startswith("debug."):
                        self.assertLessEqual(len(label), 23, "Debug captions must fit a 24-byte terminated guest slot")

    def test_portuguese_accents_are_unicode_text(self):
        portuguese = self.catalog["languages"]["pt-BR"]
        self.assertEqual(portuguese["debug.menu"], "Menu de Depuração")
        self.assertEqual(portuguese["main.options"], "Opções")
        self.assertEqual(portuguese["main.kontent"], "Konteúdo")
        self.assertNotIn("ö", portuguese["main.options"])
        self.assertEqual(portuguese["footer.default"], "Padrão")
        self.assertEqual(portuguese["recomp.title"], "Konfigurar Recompilação")

    def test_pause_translations_preserve_accents_in_utf8_json(self):
        # A Windows text-insertion path once replaced non-ASCII source glyphs
        # with literal '?'. The native font cannot recover missing characters.
        expected = {
            "pt-BR": {
                "pause.video": "V\u00eddeo e Tela",
                "pause.resolution": "Resolu\u00e7\u00e3o",
                "pause.aspect": "Propor\u00e7\u00e3o",
                "pause.exit_no": "N\u00e3o, continuar jogando",
            },
            "fr": {"pause.resolution": "R\u00e9solution"},
            "de": {"pause.resolution": "Aufl\u00f6sung"},
            "es": {"pause.resolution": "Resoluci\u00f3n"},
            "it": {"pause.exit_yes": "S\u00ec, esci senza salvare"},
        }
        keys = {key for key in self.catalog["languages"]["en"] if key.startswith("pause.")}
        self.assertIn("pause.display_refresh", keys)
        for locale, labels in self.catalog["languages"].items():
            for key in keys:
                with self.subTest(locale=locale, key=key):
                    self.assertNotIn("?", labels[key], "Accent was replaced with '?' in source JSON")
                    self.assertNotIn("\ufffd", labels[key])
        for locale, translations in expected.items():
            for key, text in translations.items():
                self.assertEqual(self.catalog["languages"][locale][key], text)

    def test_button_hints_separate_captions_from_native_icons(self):
        for locale, labels in self.catalog["languages"].items():
            with self.subTest(locale=locale):
                for key in ("settings.back_hint", "settings.edit_hint", "settings.keep_display"):
                    self.assertNotRegex(labels[key], r"\b[AB]\s*[:/]")
                for key in ("settings.keep", "settings.revert", "settings.adjust_hint"):
                    self.assertIn(key, labels)

    def test_restart_feedback_retains_localized_accents(self):
        for language,labels in self.catalog['languages'].items():
            for key in ('settings.restart','settings.restarting','settings.restart_error'):
                with self.subTest(language=language,key=key):
                    self.assertIn(key,labels)
                    self.assertNotIn('?',labels[key],'Restart feedback must not contain damaged accent substitutions')
        self.assertIn('vozes',self.catalog['languages']['pt-BR']['settings.restart'].casefold())
        self.assertIn('red\u00e9marre',self.catalog['languages']['fr']['settings.restart'])
        self.assertIn('\u00c4nderungen',self.catalog['languages']['de']['settings.restart_error'])

    def test_graphic_aliases_resolve_to_existing_labels(self):
        seen = set()
        labels = self.catalog["languages"]["en"]
        for key, identifiers in self.catalog["graphics"].items():
            self.assertIn(key, labels)
            for identifier in identifiers:
                self.assertRegex(identifier, r"^[0-9A-F]{8}$")
                self.assertNotIn(identifier, seen)
                seen.add(identifier)


if __name__ == "__main__":
    unittest.main()
