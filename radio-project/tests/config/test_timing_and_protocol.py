from __future__ import annotations

import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class TimingAndProtocolConfigTests(unittest.TestCase):
    def test_timing_constants_match_plan(self) -> None:
        text = (ROOT / "firmware" / "shared" / "radio_types.h").read_text(
            encoding="utf-8"
        )
        expected = {
            "mainStartTimeoutMs": "15000",
            "backupStartTimeoutMs": "15000",
            "localStartTimeoutMs": "2000",
            "totalStartTimeoutMs": "32000",
            "streamStallTimeoutMs": "10000",
            "idleTimeoutMs": "20000",
            "pcmReadyWindowMs": "200",
            "arcDurationMs": "1500",
            "displayDoneTimeoutMs": "3500",
            "snapshotIntervalMs": "1000",
            "doneRetryIntervalMs": "250",
        }
        for name, value in expected.items():
            with self.subTest(name=name):
                self.assertRegex(text, rf"{name}\s*=\s*{value};")
        self.assertIn("doneMaxRetries = 8;", text)

    def test_default_standby_policy_is_after_initial_load(self) -> None:
        text = (ROOT / "firmware" / "shared" / "app_controller.h").read_text(
            encoding="utf-8"
        )
        self.assertIn("standbyPolicy = StandbyPolicy::AfterInitialLoad", text)

    def test_protocol_limits_match_plan(self) -> None:
        text = (ROOT / "firmware" / "shared" / "protocol_v2.h").read_text(
            encoding="utf-8"
        )
        self.assertIn("kVersion = 2", text)
        self.assertIn("kMaxFrameBytes = 160", text)
        self.assertIn("kBaud = 115200", text)

    def test_formal_city_config_remains_incomplete(self) -> None:
        config = json.loads((ROOT / "config" / "cities.json").read_text(encoding="utf-8"))
        self.assertFalse(config["complete"])
        self.assertEqual(config["cities"], [])


if __name__ == "__main__":
    unittest.main()
