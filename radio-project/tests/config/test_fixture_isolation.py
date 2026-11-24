from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

from radio_config import load_and_validate  # noqa: E402

FIXTURE = ROOT / "tests" / "fixtures" / "cities.json"
FORMAL = ROOT / "config" / "cities.json"
PRODUCTION_HEADER = ROOT / "firmware" / "shared" / "generated" / "city_data.h"


class FixtureIsolationTests(unittest.TestCase):
    def test_fixture_is_explicitly_synthetic(self) -> None:
        fixture, errors = load_and_validate(FIXTURE, require_complete=False)
        self.assertEqual(errors, [])
        self.assertTrue(fixture.get("fixture"))
        self.assertIn("不是正式城市表", fixture["reason"])
        for city in fixture["cities"]:
            self.assertTrue(city["name"].startswith("示例"))
            self.assertTrue(city["primaryUrl"].startswith("https://example.invalid/fixture/"))
            self.assertTrue(city["backupUrl"].startswith("https://example.invalid/fixture/"))
            self.assertTrue(city["offlinePath"].startswith("/audio/FIX_"))

    def test_formal_config_has_no_cities(self) -> None:
        formal, errors = load_and_validate(FORMAL, require_complete=False)
        self.assertEqual(errors, [])
        self.assertFalse(formal["complete"])
        self.assertEqual(formal["cities"], [])
        self.assertIn("8", formal["reason"])

    def test_fixture_does_not_leak_into_production_header(self) -> None:
        text = PRODUCTION_HEADER.read_text(encoding="utf-8")
        self.assertNotIn("FIX_A", text)
        self.assertNotIn("example.invalid", text)
        self.assertNotIn("示例城市", text)


if __name__ == "__main__":
    unittest.main()
