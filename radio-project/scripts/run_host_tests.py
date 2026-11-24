#!/usr/bin/env python3
"""Build and run the Arduino-independent C++ logic tests.

The production cities.json is intentionally incomplete.  Host tests therefore
generate a temporary header from the explicitly synthetic fixture and never
write generated files into the repository.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FIXTURE = ROOT / "tests" / "fixtures" / "cities.json"
GENERATOR = ROOT / "scripts" / "generate_city_data.py"


def compiler_command() -> list[str]:
    override = os.environ.get("CXX", "").strip()
    candidates = [override] if override else []
    candidates.extend(["c++", "g++", "clang++"])
    for candidate in candidates:
        resolved = shutil.which(candidate)
        if resolved:
            return [resolved]
    raise SystemExit("ERROR: no C++ compiler found (set CXX or install g++/clang++)")


def source_files() -> list[Path]:
    files = [
        ROOT / "tests" / "logic" / "test_main.cpp",
    ]
    for directory in (
        ROOT / "firmware" / "shared",
        ROOT / "firmware" / "radio_a",
        ROOT / "firmware" / "display_b",
    ):
        files.extend(sorted(directory.glob("*.cpp")))
    return files


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        default=str(ROOT / "evidence" / "host-tests.log"),
        help="raw compiler/test log (default: evidence/host-tests.log)",
    )
    parser.add_argument(
        "--no-evidence", action="store_true", help="do not write an evidence log"
    )
    args = parser.parse_args()

    if not FIXTURE.is_file():
        print(f"ERROR: missing fixture {FIXTURE}", file=sys.stderr)
        return 2

    cxx = compiler_command()
    with tempfile.TemporaryDirectory(prefix="radio-host-tests-") as temp:
        temp_dir = Path(temp)
        generated = temp_dir / "city_data.h"
        generate = subprocess.run(
            [
                sys.executable,
                "-B",
                str(GENERATOR),
                "--input",
                str(FIXTURE),
                "--output",
                str(generated),
            ],
            cwd=ROOT,
            text=True,
            capture_output=True,
        )
        if generate.returncode != 0:
            print(generate.stdout, end="")
            print(generate.stderr, end="", file=sys.stderr)
            return generate.returncode

        binary = temp_dir / "radio-host-tests"
        command = [
            *cxx,
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-O2",
            "-g",
            f"-I{temp_dir}",
            f"-I{ROOT / 'firmware' / 'shared'}",
            f"-I{ROOT / 'firmware' / 'radio_a'}",
            f"-I{ROOT / 'firmware' / 'display_b'}",
            *[str(path) for path in source_files()],
            "-o",
            str(binary),
        ]
        compile_run = subprocess.run(
            command, cwd=ROOT, text=True, capture_output=True
        )
        transcript = [
            "radio-project host logic tests",
            f"compiler: {cxx[0]}",
            f"command: {' '.join(command)}",
            f"compile_exit: {compile_run.returncode}",
        ]
        if compile_run.stdout:
            transcript.append(compile_run.stdout.rstrip())
        if compile_run.stderr:
            transcript.append(compile_run.stderr.rstrip())

        run_exit: int | None = None
        if compile_run.returncode == 0:
            run = subprocess.run(
                [str(binary)], cwd=ROOT, text=True, capture_output=True
            )
            run_exit = run.returncode
            transcript.append(f"test_exit: {run.returncode}")
            if run.stdout:
                transcript.append(run.stdout.rstrip())
            if run.stderr:
                transcript.append(run.stderr.rstrip())

        text = "\n".join(transcript) + "\n"
        print(text, end="")
        if not args.no_evidence:
            output = Path(args.output)
            if not output.is_absolute():
                output = ROOT / output
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(text, encoding="utf-8")
            print(f"evidence: {output}")

        if compile_run.returncode != 0:
            return compile_run.returncode
        if run_exit is None:
            return 1
        return int(run_exit)


if __name__ == "__main__":
    raise SystemExit(main())
