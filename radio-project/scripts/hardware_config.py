"""Validate the same per-board literal C headers used by the firmware build.

These files are data: only #pragma once and integer #define directives are
supported. Reject conditional/includes/expressions rather than misreading C.
"""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SD_PINS = ["RADIO_PIN_SD_SCK", "RADIO_PIN_SD_MISO", "RADIO_PIN_SD_MOSI", "RADIO_PIN_SD_CS"]
COMMON_PINS = ["RADIO_PIN_LINK_TX", "RADIO_PIN_LINK_RX", *SD_PINS,
               "RADIO_PIN_USB_DM", "RADIO_PIN_USB_DP", "RADIO_PIN_BOOT"]
BOARD_A_PINS = COMMON_PINS + [
    "RADIO_PIN_AUDIO_BCLK", "RADIO_PIN_AUDIO_LRCLK", "RADIO_PIN_AUDIO_DOUT",
    "RADIO_PIN_ENCODER_A", "RADIO_PIN_ENCODER_B", "RADIO_PIN_ENCODER_SW",
]
BOARD_B_PINS = COMMON_PINS + [
    f"RADIO_PIN_PANEL_{signal}"
    for signal in ("R1", "G1", "B1", "R2", "G2", "B2", "CLK", "LAT", "OE",
                   "A", "B", "C", "D", "E")
]
GATES = {"a": "RADIO_HARDWARE_CONFIGURED", "b": "RADIO_DISPLAY_CONFIGURED"}
VALID_GPIO = set(range(22)) | set(range(26, 49))


def pins_path(board: str, root: Path = ROOT) -> Path:
    """A board-specific local full copy takes precedence over shared wiring."""
    local = root / "config" / f"hardware_pins_{board}.local.h"
    return local if local.is_file() else root / "config" / f"hardware_pins_{board}.h"


def parse_header(text: str) -> tuple[dict[str, int], list[str]]:
    values: dict[str, int] = {}
    errors: list[str] = []
    if re.search(r"\\\r?\n", text):
        errors.append("不支持反斜杠续行（含注释中的续行），避免 C 预处理与校验结果不同")
    # Keep line breaks for readable diagnostics; commented definitions do not count.
    text = re.sub(r"//[^\n]*|/\*.*?\*/", lambda m: " " + "\n" * m[0].count("\n"),
                  text, flags=re.S)
    seen: set[str] = set()
    for number, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if not line or re.fullmatch(r"#\s*pragma\s+once", line):
            continue
        match = re.fullmatch(r"#\s*define\s+(RADIO_[A-Z0-9_]+)\s+(0|[1-9][0-9]*)", line)
        if not match:
            errors.append(f"第 {number} 行：只允许十进制整数 #define 和 #pragma once：{line}")
            continue
        name, value = match.groups()
        if name in seen:
            errors.append(f"重复定义 {name}")
        seen.add(name)
        values[name] = int(value)
    return values, errors


def validate_pins(board: str, values: dict[str, int], *, require_complete: bool = True) -> list[str]:
    required = BOARD_A_PINS if board == "a" else BOARD_B_PINS
    allowed = set(required) | {GATES[board], "RADIO_OCTAL_MEMORY_PRESENT", "RADIO_GPIO45_STRAP_VERIFIED"}
    errors = [f"板 {board.upper()} 未知或属于另一板的宏：{name}" for name in values if name not in allowed]
    for name in (GATES[board], "RADIO_GPIO45_STRAP_VERIFIED"):
        if name in values and values[name] not in (0, 1):
            errors.append(f"{name} 只能为 0 或 1")
    if require_complete:
        missing = [name for name in required if name not in values]
        if missing:
            errors.append("缺少 IO：" + ", ".join(missing))
        if values.get(GATES[board]) != 1:
            errors.append(f"{GATES[board]} 未启用；UART、模组与电气核对完成后才能设为 1")

    occupied: dict[int, str] = {}
    for name, pin in values.items():
        if not name.startswith("RADIO_PIN_"):
            continue
        if pin not in VALID_GPIO:
            errors.append(f"{name}={pin}：不是 ESP32-S3 有效 GPIO（22..25 不存在）")
            continue
        if pin in occupied:
            errors.append(f"GPIO{pin} 冲突：{occupied[pin]} 与 {name}")
        occupied[pin] = name
        if 26 <= pin <= 32:
            errors.append(f"{name}=GPIO{pin}：Flash/PSRAM 总线引脚，不允许用于本项目外设")

    for name, fixed in {"RADIO_PIN_USB_DM": 19, "RADIO_PIN_USB_DP": 20, "RADIO_PIN_BOOT": 0}.items():
        if name in values and values[name] != fixed:
            errors.append(f"{name} 必须为 GPIO{fixed}")
        # Even an incomplete file must not give these reserved pins to another role.
        owner = occupied.get(fixed)
        if owner and owner != name:
            errors.append(f"GPIO{fixed} 已保留给 {name}，不能分配给 {owner}")

    octal = values.get("RADIO_OCTAL_MEMORY_PRESENT")
    if octal is not None and octal not in (0, 1):
        errors.append("RADIO_OCTAL_MEMORY_PRESENT 只能为 0 或 1")
    memory_pins = sorted(set(occupied) & set(range(33, 38)))
    if memory_pins and (octal == 1 or (require_complete and octal is None)):
        errors.append(
            f"GPIO{memory_pins} 与八线 Flash/PSRAM 复用；"
            + ("实际模组使用八线存储器，必须重新分配这些 IO" if octal == 1
               else "模组未确认，缺少 RADIO_OCTAL_MEMORY_PRESENT（核对实物后填写 0 或 1）")
        )
    if 45 in occupied and require_complete and values.get("RADIO_GPIO45_STRAP_VERIFIED") != 1:
        errors.append("GPIO45 是启动配置脚；缺少 RADIO_GPIO45_STRAP_VERIFIED 1（核对 VDD_SPI 与外部负载）")
    return errors


def load_pins(board: str, path: Path | None = None, *, require_complete: bool = True,
              root: Path = ROOT) -> tuple[dict[str, int], list[str]]:
    path = path or pins_path(board, root)
    if not path.is_file():
        return {}, [f"硬件配置不存在：{path}"]
    values, errors = parse_header(path.read_text(encoding="utf-8"))
    return values, errors + validate_pins(board, values, require_complete=require_complete)
