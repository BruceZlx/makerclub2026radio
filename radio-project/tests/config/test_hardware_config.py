from __future__ import annotations

import contextlib
import io
import shlex
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

import audit_delivery  # noqa: E402
import build_firmware  # noqa: E402
from hardware_config import GATES, load_pins, parse_header, pins_path, validate_pins  # noqa: E402


def complete(board: str) -> dict[str, int]:
    values, errors = load_pins(board, ROOT / "config" / f"hardware_pins_{board}.h", require_complete=False)
    assert not errors, errors
    # Synthetic UART allocations for tests only; not actual project wiring.
    values.update(RADIO_PIN_LINK_TX=15, RADIO_PIN_LINK_RX=16)
    values[GATES[board]] = 1
    if board == "b":
        values.update(RADIO_OCTAL_MEMORY_PRESENT=0, RADIO_GPIO45_STRAP_VERIFIED=1)
    return values


def write_header(path: Path, values: dict[str, int]) -> None:
    path.write_text("#pragma once\n" + "".join(f"#define {key} {value}\n" for key, value in values.items()), encoding="utf-8")


class HardwareConfigTests(unittest.TestCase):
    def test_received_wiring_matches_user_list(self) -> None:
        expected = {
            "a": {"AUDIO_BCLK": 4, "AUDIO_LRCLK": 5, "AUDIO_DOUT": 6,
                  "ENCODER_A": 7, "ENCODER_B": 8, "ENCODER_SW": 9},
            "b": {"PANEL_R1": 42, "PANEL_G1": 41, "PANEL_B1": 40,
                  "PANEL_R2": 38, "PANEL_G2": 39, "PANEL_B2": 37,
                  "PANEL_E": 21, "PANEL_A": 45, "PANEL_B": 36,
                  "PANEL_C": 48, "PANEL_D": 35, "PANEL_CLK": 2,
                  "PANEL_LAT": 47, "PANEL_OE": 14},
        }
        for board, wiring in expected.items():
            wiring.update(SD_CS=10, SD_MOSI=11, SD_SCK=12, SD_MISO=13,
                          USB_DM=19, USB_DP=20, BOOT=0)
            values, errors = load_pins(board, ROOT / "config" / f"hardware_pins_{board}.h", require_complete=False)
            self.assertEqual(errors, [])
            self.assertEqual({key.removeprefix("RADIO_PIN_"): value for key, value in values.items()
                              if key.startswith("RADIO_PIN_")}, wiring)
            self.assertEqual(values[GATES[board]], 0)

    def test_received_headers_are_incomplete_without_invented_uart(self) -> None:
        for board in ("a", "b"):
            values, errors = load_pins(board, ROOT / "config" / f"hardware_pins_{board}.h")
            self.assertNotIn("RADIO_PIN_LINK_TX", values)
            self.assertNotIn("RADIO_PIN_LINK_RX", values)
            self.assertIn("RADIO_PIN_LINK_TX", " ".join(errors))
            self.assertNotIn("RADIO_PIN_RESET", values)

    def test_complete_boards_are_validated_independently(self) -> None:
        for board in ("a", "b"):
            self.assertEqual(validate_pins(board, complete(board)), [])

    def test_both_boards_require_all_sd_signals(self) -> None:
        for board in ("a", "b"):
            values = complete(board)
            del values["RADIO_PIN_SD_MISO"]
            self.assertIn("RADIO_PIN_SD_MISO", " ".join(validate_pins(board, values)))

    def test_duplicate_gpio_and_uart_usb_boot_conflicts(self) -> None:
        for pin in (4, 19, 20, 0):
            values = complete("a")
            values["RADIO_PIN_LINK_TX"] = pin
            self.assertIn("冲突", " ".join(validate_pins("a", values)))

    def test_nonexistent_and_flash_bus_gpio(self) -> None:
        for pin in (22, 23, 24, 25, 26, 32, 49, -1):
            values = complete("a")
            values["RADIO_PIN_LINK_TX"] = pin
            self.assertTrue(validate_pins("a", values), pin)

    def test_octal_memory_conflict_cannot_be_overridden_by_gate(self) -> None:
        values = complete("b")
        values["RADIO_OCTAL_MEMORY_PRESENT"] = 1
        self.assertIn("必须重新分配", " ".join(validate_pins("b", values)))
        del values["RADIO_OCTAL_MEMORY_PRESENT"]
        self.assertIn("模组未确认", " ".join(validate_pins("b", values)))
        for key, value in {"RADIO_PIN_PANEL_B2": 1, "RADIO_PIN_PANEL_B": 17,
                           "RADIO_PIN_PANEL_D": 18}.items():
            values[key] = value
        values["RADIO_OCTAL_MEMORY_PRESENT"] = 1
        self.assertEqual(validate_pins("b", values), [])

    def test_gpio45_requires_reset_level_verification(self) -> None:
        values = complete("b")
        del values["RADIO_GPIO45_STRAP_VERIFIED"]
        self.assertIn("GPIO45", " ".join(validate_pins("b", values)))

    def test_usb_and_boot_have_fixed_pin_numbers(self) -> None:
        for name in ("RADIO_PIN_USB_DM", "RADIO_PIN_USB_DP", "RADIO_PIN_BOOT"):
            values = complete("a")
            values[name] = 18
            self.assertIn("必须为", " ".join(validate_pins("a", values)))

    def test_comments_do_not_enable_gate_or_supply_pins(self) -> None:
        values, errors = parse_header("// #define RADIO_HARDWARE_CONFIGURED 1\n/*\n#define RADIO_PIN_LINK_TX 15\n*/\n#define RADIO_PIN_SD_CS 10 // card\n")
        self.assertEqual(errors, [])
        self.assertEqual(values, {"RADIO_PIN_SD_CS": 10})
        self.assertTrue(validate_pins("a", values))
        # A block-comment marker inside a line comment cannot hide real macros.
        values, errors = parse_header("// /*\n#define RADIO_PIN_LINK_TX 4\n// */\n")
        self.assertEqual(errors, [])
        self.assertEqual(values, {"RADIO_PIN_LINK_TX": 4})

    def test_rejects_preprocessor_ambiguity(self) -> None:
        for text in ("#if 0\n#define RADIO_HARDWARE_CONFIGURED 1\n#endif",
                     '#include "other.h"', "#define RADIO_PIN_SD_CS (10)",
                     "#define RADIO_PIN_SD_CS GPIO_NUM_10", "#define RADIO_PIN_SD_CS 010",
                     "#define RADIO_PIN_SD_CS 10\n#define RADIO_PIN_SD_CS 11",
                     "// hidden \\\n#define RADIO_PIN_LINK_TX 15\n"):
            self.assertTrue(parse_header(text)[1], text)

    def test_wrong_board_and_typo_are_rejected(self) -> None:
        self.assertTrue(validate_pins("a", complete("b")))
        values = complete("a")
        values["RADIO_PIN_AUDIO_DATA"] = 17
        self.assertIn("未知", " ".join(validate_pins("a", values)))

    def test_local_selection_is_board_specific(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "config").mkdir()
            local = root / "config" / "hardware_pins_a.local.h"
            local.touch()
            self.assertEqual(pins_path("a", root), local)
            self.assertEqual(pins_path("b", root), root / "config" / "hardware_pins_b.h")

    def test_missing_header_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            self.assertTrue(load_pins("a", Path(temp) / "missing.h")[1])

    def test_build_uses_validated_header_and_board_flag(self) -> None:
        with tempfile.TemporaryDirectory(prefix="radio pins ") as temp:
            root = Path(temp)
            (root / "config").mkdir()
            (root / "config" / "secrets.local.h").touch()
            header = root / "config" / "hardware_pins_a.local.h"
            write_header(header, complete("a"))
            sketch = root / "radio_a.ino"
            sketch.touch()
            stdout = io.StringIO()
            with patch.object(build_firmware, "ROOT", root), \
                 patch.object(build_firmware, "load_and_validate", return_value=({}, [])), \
                 patch.object(build_firmware.shutil, "which", return_value="/test/arduino-cli"), \
                 patch.object(build_firmware.subprocess, "run") as run, \
                 patch.object(sys, "argv", ["build_firmware.py", "--board", "a", "--sketch", str(sketch), "--fqbn", "test:full:s3"]), \
                 contextlib.redirect_stdout(stdout):
                run.return_value.returncode = 0
                self.assertEqual(build_firmware.main(), 0)
                command = run.call_args.args[0]
            flags = command[command.index("--build-property") + 1].split("=", 1)[1]
            self.assertEqual(shlex.split(flags), ["-DRADIO_BOARD_A=1", "-include", str(header.resolve())])

    def test_build_rejects_incomplete_pins_before_compiler(self) -> None:
        with patch.object(build_firmware.subprocess, "run") as run, \
             patch.object(sys, "argv", ["build_firmware.py", "--board", "b", "--pins", str(ROOT / "config" / "hardware_pins_b.h"), "--dry-run"]), \
             contextlib.redirect_stderr(io.StringIO()) as stderr:
            self.assertEqual(build_firmware.main(), 2)
            run.assert_not_called()
            self.assertIn("RADIO_PIN_LINK_TX", stderr.getvalue())
            self.assertIn("GPIO45", stderr.getvalue())

    def test_audit_rejects_commented_enable_flags(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "config").mkdir()
            for board in ("a", "b"):
                (root / "config" / f"hardware_pins_{board}.h").write_text(
                    "// RADIO_HARDWARE_CONFIGURED 1\n// RADIO_DISPLAY_CONFIGURED 1\n", encoding="utf-8")
            with patch.object(audit_delivery, "ROOT", root), \
                 patch.object(sys, "argv", ["audit_delivery.py", "--strict", "--output", str(root / "report.json"), "--manifest", str(root / "manifest.json")]), \
                 contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(audit_delivery.main(), 2)
            import json
            report = json.loads((root / "report.json").read_text(encoding="utf-8"))
            self.assertFalse(next(item for item in report["checks"] if item["id"] == "A1-io")["passed"])
            self.assertFalse(report["hardware_configuration"]["a"]["passed"])
            self.assertFalse(report["hardware_configuration"]["b"]["passed"])


if __name__ == "__main__":
    unittest.main()
