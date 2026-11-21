"""Exercise campaign runtime CLI and command generation without starting a game."""

import json
from pathlib import Path
import subprocess
import sys
import unittest


class CampaignRuntimeCommandsTest(unittest.TestCase):
    def run_driver(self, scenario, *options):
        script = Path(__file__).with_name("recomp_settings_runtime_test.py")
        return subprocess.run(
            [sys.executable, str(script), "--exe", "unused.exe", "--game", "unused.iso",
             "--save-fixture", "unused-saves", "--scenario", scenario,
             *options, "--campaign-print-plan"],
            capture_output=True, text=True, check=False,
        )

    def test_solo_plan_uses_original_main_row(self):
        result = self.run_driver("campaign-roster", "--campaign-check-back")
        self.assertEqual(result.returncode, 0, result.stderr)
        plan = json.loads(result.stdout)
        self.assertEqual(plan["commands"][0], ["navigate", 0])
        self.assertIn(["campaign-characters", 4, None, True], plan["commands"])
        self.assertEqual(plan["virtual_gamepads"], 1)

    def test_koop_plan_uses_original_main_row_and_exact_slots(self):
        result = self.run_driver("campaign-roster", "--campaign-fighter", "0",
                                 "--campaign-partner", "7", "--campaign-frontend-only")
        self.assertEqual(result.returncode, 0, result.stderr)
        plan = json.loads(result.stdout)
        self.assertEqual(plan["commands"], [
            ["navigate", 1], ["campaign-characters", 0, 7, False], ["quit"],
        ])
        self.assertEqual(plan["virtual_gamepads"], 2)

    def test_duplicate_plan_reaches_native_selector_without_starting_campaign(self):
        result = self.run_driver("campaign-roster", "--campaign-fighter", "6",
                                 "--campaign-partner", "6")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["commands"], [
            ["navigate", 1], ["campaign-characters", 6, 6, False], ["quit"],
        ])

    def test_out_of_range_slots_are_rejected(self):
        for option, value in (("--campaign-fighter", "-1"), ("--campaign-partner", "8")):
            with self.subTest(option=option):
                result = self.run_driver("campaign-roster", option, value)
                self.assertEqual(result.returncode, 2)
                self.assertIn(f"argument {option}: invalid choice", result.stderr)

    def test_move_menu_probe_follows_native_startup(self):
        result = self.run_driver("campaign-roster", "--campaign-fighter", "6",
                                 "--campaign-partner", "5", "--campaign-move-menus")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["commands"][-3:], [
            ["campaign-start", 6, 5], ["campaign-move-menus"], ["quit"],
        ])

    def test_campaign_options_are_rejected_in_other_scenarios(self):
        result = self.run_driver("navigation", "--campaign-fighter", "4")
        self.assertEqual(result.returncode, 2)
        self.assertIn("--campaign-fighter requires --scenario campaign-roster", result.stderr)

    def test_save_fixture_plan_uses_original_load_flow(self):
        result = self.run_driver("campaign-roster", "--campaign-fighter", "0",
                                 "--campaign-save-roundtrip", "--slot", "2")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["commands"], [
            ["navigate", 3], ["enter-menu", "0x5967C0"],
            ["campaign-save-roundtrip", 0, None], ["quit"],
        ])

    def test_save_fixture_rejects_incompatible_frontend_probe(self):
        result = self.run_driver("campaign-roster", "--campaign-save-roundtrip",
                                 "--campaign-frontend-only")
        self.assertEqual(result.returncode, 2)
        self.assertIn("--campaign-save-roundtrip uses the original Load Game fixture", result.stderr)

    def test_save_fixture_requires_explicit_totem_slot(self):
        result = self.run_driver("campaign-roster", "--campaign-save-roundtrip")
        self.assertEqual(result.returncode, 2)
        self.assertIn("--campaign-save-roundtrip requires --slot", result.stderr)

    def test_save_fixture_rejects_selector_unlock_probe(self):
        result = self.run_driver("campaign-roster", "--campaign-save-roundtrip",
                                 "--slot", "2", "--campaign-saved-unlocks")
        self.assertEqual(result.returncode, 2)
        self.assertIn("--campaign-saved-unlocks applies to character selection", result.stderr)

    def test_death_continue_diagnostic_follows_save_roundtrip(self):
        result = self.run_driver("campaign-roster", "--campaign-save-roundtrip",
                                 "--campaign-death-continue", "--slot", "2")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["commands"][-3:], [
            ["campaign-save-roundtrip", 4, None], ["campaign-death-continue"], ["quit"],
        ])

    def test_death_continue_requires_native_save_checkpoint(self):
        result = self.run_driver("campaign-roster", "--campaign-death-continue")
        self.assertEqual(result.returncode, 2)
        self.assertIn("--campaign-death-continue requires --campaign-save-roundtrip", result.stderr)


if __name__ == "__main__":
    unittest.main()
