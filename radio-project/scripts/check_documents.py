#!/usr/bin/env python3
"""Check that the Markdown and Word plans are the same v1.2 content."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
import zipfile
from pathlib import Path
from xml.etree import ElementTree

ROOT = Path(__file__).resolve().parent.parent
W_NS = "{http://schemas.openxmlformats.org/wordprocessingml/2006/main}"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def docx_text(path: Path) -> tuple[str, str]:
    with zipfile.ZipFile(path) as archive:
        document = archive.read("word/document.xml")
        footer = archive.read("word/footer1.xml") if "word/footer1.xml" in archive.namelist() else b""
        core = archive.read("docProps/core.xml") if "docProps/core.xml" in archive.namelist() else b""
    document_parts: list[str] = []
    for blob in (document, core):
        root = ElementTree.fromstring(blob)
        document_parts.extend(element.text or "" for element in root.iter(W_NS + "t"))
    footer_parts: list[str] = []
    if footer:
        root = ElementTree.fromstring(footer)
        footer_parts.extend(element.text or "" for element in root.iter(W_NS + "t"))
    return "\n".join(document_parts), "\n".join(footer_parts)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--markdown", required=True)
    parser.add_argument("--docx", required=True)
    parser.add_argument(
        "--output",
        default=str(ROOT / "evidence" / "document-check.json"),
    )
    args = parser.parse_args()

    markdown_path = Path(args.markdown)
    docx_path = Path(args.docx)
    checks: list[dict[str, object]] = []

    def check(name: str, passed: bool, detail: str = "") -> None:
        checks.append({"name": name, "passed": bool(passed), "detail": detail})

    if not markdown_path.is_file():
        check("markdown_exists", False, str(markdown_path))
        markdown = ""
    else:
        markdown = markdown_path.read_text(encoding="utf-8")
        check("markdown_exists", True, str(markdown_path))

    if not docx_path.is_file():
        check("docx_exists", False, str(docx_path))
        word = ""
        footer = ""
    else:
        check("docx_exists", True, str(docx_path))
        try:
            word, footer = docx_text(docx_path)
            check("docx_readable", True, "word/document.xml")
        except Exception as exc:  # invalid zip/XML
            word = ""
            footer = ""
            check("docx_readable", False, str(exc))

    markdown_needles = [
        ("markdown_version_1_2", "版本：1.2"),
        ("markdown_date", "日期：2026年11月24日"),
        ("markdown_s3_target", "ESP32-S3"),
        ("markdown_after_initial_load", "AFTER_INITIAL_LOAD"),
        ("markdown_last_user_input", "LAST_USER_INPUT"),
        ("markdown_protocol_v2", "协议 v2"),
        ("markdown_baseline_commit", "34dc4e022780cb75b48db99211a10f1e8ae60a66"),
        ("markdown_user_choice", "本次用户选择"),
        ("markdown_not_partner_approval", "不追认为 partner"),
        ("markdown_implementation_status", "15 v1.2 实施状态"),
    ]
    for name, needle in markdown_needles:
        check(name, needle in markdown, needle)
    check("markdown_no_v1_1_version", "版本：1.1" not in markdown, "版本：1.1")
    check("markdown_no_wrover_baseline", "ESP32-WROVER" not in markdown, "ESP32-WROVER")

    word_needles = [
        ("word_version_1_2", "版本：1.2"),
        ("word_s3_target", "ESP32-S3"),
        ("word_after_initial_load", "AFTER_INITIAL_LOAD"),
        ("word_last_user_input", "LAST_USER_INPUT"),
        ("word_protocol_v2", "协议 v2"),
        ("word_baseline_commit", "34dc4e022780cb75b48db99211a10f1e8ae60a66"),
        ("word_user_choice", "本次用户选择"),
        ("word_not_partner_approval", "不追认为 partner"),
        ("word_implementation_status", "15 v1.2 实施状态"),
    ]
    for name, needle in word_needles:
        check(name, needle in word, needle)
    check("word_no_v1_1_version", "版本：1.1" not in word, "版本：1.1")
    check("word_no_wrover_baseline", "ESP32-WROVER" not in word, "ESP32-WROVER")
    check("word_footer_1_2", "1.2" in footer, footer[:80])

    # The same key decisions must be present in both documents.
    for label, needle in [
        ("s3_consistent", "ESP32-S3"),
        ("r2_consistent", "AFTER_INITIAL_LOAD"),
        ("r3_consistent", "协议 v2"),
        ("baseline_consistent", "34dc4e022780cb75b48db99211a10f1e8ae60a66"),
    ]:
        check(label, needle in markdown and needle in word, needle)

    passed = all(bool(item["passed"]) for item in checks)
    report = {
        "schema_version": 1,
        "status": "pass" if passed else "fail",
        "markdown": {
            "path": str(markdown_path),
            "sha256": sha256(markdown_path) if markdown_path.is_file() else None,
        },
        "docx": {
            "path": str(docx_path),
            "sha256": sha256(docx_path) if docx_path.is_file() else None,
        },
        "checks": checks,
    }
    output = Path(args.output)
    if not output.is_absolute():
        output = ROOT / output
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )

    for item in checks:
        if not item["passed"]:
            print(f"FAIL: {item['name']} ({item['detail']})")
    print(
        f"document check: {report['status']} "
        f"({sum(1 for item in checks if item['passed'])}/{len(checks)})"
    )
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
