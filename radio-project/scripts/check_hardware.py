#!/usr/bin/env python3
"""Review received wiring separately from hardware readiness."""

from __future__ import annotations

import argparse
from pathlib import Path

from hardware_config import load_pins, pins_path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--board", choices=["a", "b"], required=True)
    parser.add_argument("--pins", type=Path, help="full per-board literal header")
    parser.add_argument("--require-complete", action="store_true")
    args = parser.parse_args()
    path = args.pins or pins_path(args.board)
    values, errors = load_pins(args.board, path, require_complete=args.require_complete)
    print(f"board {args.board.upper()}: {path}")
    print(f"received GPIO assignments: {sum(name.startswith('RADIO_PIN_') for name in values)}")
    for error in errors:
        print(f"ERROR: {error}")
    if not errors:
        print("PASS: " + ("hardware IO gates passed" if args.require_complete else
                          "received wiring is internally consistent; module/UART/electrical readiness not asserted"))
    return 2 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
