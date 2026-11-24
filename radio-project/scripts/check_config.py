#!/usr/bin/env python3
"""Validate config/cities.json or a fixture."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from radio_config import config_id, load_and_validate  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", default="config/cities.json")
    parser.add_argument("--require-complete", action="store_true")
    parser.add_argument("--print-config-id", action="store_true")
    args = parser.parse_args()

    try:
        config, errors = load_and_validate(
            args.input, require_complete=args.require_complete
        )
    except Exception as exc:  # ConfigError or file error
        print(f"ERROR: {exc}")
        return 1

    if args.print_config_id:
        print(config_id(config))
    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 2 if config.get("complete") is False else 1
    print(f"OK: {args.input} config_id={config_id(config)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
