#!/usr/bin/env python3
"""Hardware build gate and Arduino CLI wrapper.

This script deliberately fails before invoking the compiler when the formal
city table, local IO map, Wi-Fi secret, sketch entry point, or Arduino CLI is
missing.  It never supplies default GPIO values.
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from radio_config import load_and_validate  # noqa: E402

BOARD_A_PINS = [
    "RADIO_PIN_AUDIO_BCLK",
    "RADIO_PIN_AUDIO_LRCLK",
    "RADIO_PIN_AUDIO_DOUT",
    "RADIO_PIN_ENCODER_A",
    "RADIO_PIN_ENCODER_B",
    "RADIO_PIN_ENCODER_SW",
    "RADIO_PIN_LINK_TX",
    "RADIO_PIN_LINK_RX",
    "RADIO_PIN_SD_SCK",
    "RADIO_PIN_SD_MISO",
    "RADIO_PIN_SD_MOSI",
    "RADIO_PIN_SD_CS",
]
BOARD_B_PINS = [
    "RADIO_PIN_LINK_TX",
    "RADIO_PIN_LINK_RX",
    "RADIO_PIN_PANEL_R1",
    "RADIO_PIN_PANEL_G1",
    "RADIO_PIN_PANEL_B1",
    "RADIO_PIN_PANEL_R2",
    "RADIO_PIN_PANEL_G2",
    "RADIO_PIN_PANEL_B2",
    "RADIO_PIN_PANEL_CLK",
    "RADIO_PIN_PANEL_LAT",
    "RADIO_PIN_PANEL_OE",
    "RADIO_PIN_PANEL_A",
    "RADIO_PIN_PANEL_B",
    "RADIO_PIN_PANEL_C",
    "RADIO_PIN_PANEL_D",
    "RADIO_PIN_PANEL_E",
]


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--board", choices=["a", "b"], required=True)
    parser.add_argument(
        "--fqbn",
        default="esp32:esp32:esp32s3",
        help="base FQBN only; full S3 parameters must come from the real module",
    )
    parser.add_argument("--sketch", default="")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    errors: list[str] = []

    # Formal city data must be complete before a hardware build.
    try:
        _, config_errors = load_and_validate(
            ROOT / "config" / "cities.json", require_complete=True
        )
    except Exception as exc:
        config_errors = [str(exc)]
    if config_errors:
        errors.append("正式 cities.json 不完整：" + "；".join(config_errors[:3]))

    # Local hardware configuration must exist and define every required macro.
    pins_path = ROOT / "config" / "hardware_pins.local.h"
    pins_text = pins_path.read_text(encoding="utf-8") if pins_path.is_file() else ""
    required = BOARD_A_PINS if args.board == "a" else BOARD_B_PINS
    missing = [name for name in required if not re.search(rf"#define\s+{name}\b", pins_text)]
    invalid: list[str] = []
    for name in required:
        match = re.search(rf"#define\s+{name}\s+(\S+)", pins_text)
        if not match:
            continue
        value = match.group(1)
        if not value.isdigit() or not 0 <= int(value) <= 48:
            invalid.append(f"{name}={value}")
    gate_macro = (
        "RADIO_HARDWARE_CONFIGURED"
        if args.board == "a"
        else "RADIO_DISPLAY_CONFIGURED"
    )
    if not re.search(rf"#define\s+{gate_macro}\s+1\b", pins_text):
        missing.append(gate_macro)
    if missing:
        errors.append(
            "config/hardware_pins.local.h 缺少或未启用："
            + ", ".join(missing)
            + "；禁止使用默认 GPIO。"
        )
    if invalid:
        errors.append(
            "config/hardware_pins.local.h 含非数字或超出 0..48 的 GPIO："
            + ", ".join(invalid)
        )

    if args.board == "a" and not (ROOT / "config" / "secrets.local.h").is_file():
        errors.append("config/secrets.local.h 缺失；板 A 需要 Wi-Fi 配置。")

    arduino_cli = shutil.which("arduino-cli")
    if not arduino_cli:
        errors.append("arduino-cli 未安装；不能执行 S3 真实编译。")

    sketch = Path(args.sketch) if args.sketch else (
        ROOT / "firmware" / "radio_a" / "radio_a.ino"
        if args.board == "a"
        else ROOT / "firmware" / "display_b" / "display_b.ino"
    )
    if not sketch.is_file():
        errors.append(
            f"Arduino sketch 入口未集成：{sketch}；"
            "P7 硬件集成前不会生成固件。"
        )
    if args.fqbn == "esp32:esp32:esp32s3" and not args.dry_run:
        errors.append(
            "基础 FQBN 不能用于真实构建；必须提供含模组、Flash、PSRAM、分区和 USB 参数的完整 FQBN。"
        )

    command = [
        arduino_cli or "arduino-cli",
        "compile",
        "--fqbn",
        args.fqbn,
        "--export-binaries",
        str(sketch.parent),
    ]
    if errors:
        for error in errors:
            fail(error)
        return 2
    if args.dry_run:
        print("dry-run: all hardware gates passed")
        print("command:", " ".join(command))
        print("note: base FQBN only; confirm the real S3 module, Flash, PSRAM and partition.")
        return 0

    with tempfile.TemporaryDirectory(prefix="radio-build-") as temp:
        command.extend(["--build-path", temp])
        result = subprocess.run(command, cwd=ROOT)
        return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
