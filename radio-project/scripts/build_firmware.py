#!/usr/bin/env python3
"""Hardware build gate and Arduino CLI wrapper.

This script deliberately fails before invoking the compiler when the formal
city table, per-board IO map, Wi-Fi secret, sketch entry point, or Arduino CLI
is missing. It uses the received wiring and never invents missing GPIO values.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from radio_config import load_and_validate  # noqa: E402
from hardware_config import load_pins, pins_path  # noqa: E402


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
    parser.add_argument("--pins", type=Path, help="完整的单板 IO 头文件；默认优先使用该板 .local.h")
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

    # Use exactly the same per-board validator as the release audit.
    selected_pins = (args.pins or pins_path(args.board, ROOT)).resolve()
    _, pin_errors = load_pins(args.board, selected_pins)
    errors.extend(f"{selected_pins.name}: {error}" for error in pin_errors)

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
        "--build-property",
        f'compiler.cpp.extra_flags=-DRADIO_BOARD_{args.board.upper()}=1 -include "{selected_pins}"',
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
