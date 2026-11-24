from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

import generate_city_data  # noqa: E402
from radio_config import load_and_validate  # noqa: E402

FIXTURE = ROOT / "tests" / "fixtures" / "cities.json"
FORMAL = ROOT / "config" / "cities.json"


class GenerateCityDataTests(unittest.TestCase):
    def test_render_fixture_table(self) -> None:
        config, errors = load_and_validate(FIXTURE, require_complete=False)
        self.assertEqual(errors, [])
        rendered = generate_city_data.render(config)
        self.assertIn("kCityCount = 8;", rendered)
        self.assertIn('kConfigId[] = "', rendered)
        self.assertIn('"FIX_A"', rendered)
        self.assertIn("kComplete = true;", rendered)

    def test_generator_writes_fixture_header(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / "city_data.h"
            result = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(ROOT / "scripts" / "generate_city_data.py"),
                    "--input",
                    str(FIXTURE),
                    "--output",
                    str(output),
                ],
                cwd=ROOT,
                text=True,
                capture_output=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            text = output.read_text(encoding="utf-8")
            self.assertIn("kCityCount = 8;", text)
            self.assertIn("FIX_A", text)

    def test_generator_rejects_incomplete_formal_config(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / "city_data.h"
            result = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(ROOT / "scripts" / "generate_city_data.py"),
                    "--input",
                    str(FORMAL),
                    "--output",
                    str(output),
                ],
                cwd=ROOT,
                text=True,
                capture_output=True,
            )
            self.assertEqual(result.returncode, 2)
            self.assertIn("不完整", result.stderr)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
