from __future__ import annotations

import copy
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

from radio_config import config_id, load_and_validate, validate_config  # noqa: E402

FIXTURE = ROOT / "tests" / "fixtures" / "cities.json"
FORMAL = ROOT / "config" / "cities.json"


class RadioConfigTests(unittest.TestCase):
    def setUp(self) -> None:
        self.fixture, self.errors = load_and_validate(FIXTURE, require_complete=False)

    def test_fixture_is_complete_and_valid(self) -> None:
        self.assertEqual(self.errors, [])
        self.assertTrue(self.fixture["complete"])
        self.assertEqual(len(self.fixture["cities"]), 8)
        self.assertEqual(config_id(self.fixture), config_id(self.fixture))

    def test_formal_config_is_incomplete(self) -> None:
        _, errors = load_and_validate(FORMAL, require_complete=True)
        self.assertTrue(any("正式配置不完整" in error for error in errors))
        _, relaxed = load_and_validate(FORMAL, require_complete=False)
        self.assertEqual(relaxed, [])
        self.assertFalse(load_and_validate(FORMAL, require_complete=False)[0]["complete"])

    def test_duplicate_id_is_rejected(self) -> None:
        config = copy.deepcopy(self.fixture)
        config["cities"][1]["id"] = config["cities"][0]["id"]
        errors = validate_config(config, require_complete=True)
        self.assertTrue(any("id 重复" in error for error in errors))

    def test_invalid_url_is_rejected(self) -> None:
        config = copy.deepcopy(self.fixture)
        config["cities"][0]["primaryUrl"] = "ftp://example.invalid/a.mp3"
        errors = validate_config(config, require_complete=True)
        self.assertTrue(any("http(s)" in error for error in errors))

    def test_fixed_offset_timezone_is_rejected(self) -> None:
        config = copy.deepcopy(self.fixture)
        config["cities"][0]["timezoneRule"] = "8"
        errors = validate_config(config, require_complete=True)
        self.assertTrue(any("时区规则" in error for error in errors))

    def test_duplicate_display_order_is_rejected(self) -> None:
        config = copy.deepcopy(self.fixture)
        config["cities"][1]["displayOrder"] = config["cities"][0]["displayOrder"]
        errors = validate_config(config, require_complete=True)
        self.assertTrue(any("displayOrder 重复" in error for error in errors))

    def test_config_id_changes_with_city_data(self) -> None:
        changed = copy.deepcopy(self.fixture)
        changed["cities"][0]["name"] = "示例城市甲改名"
        self.assertNotEqual(config_id(self.fixture), config_id(changed))


if __name__ == "__main__":
    unittest.main()
