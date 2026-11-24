#!/usr/bin/env python3
"""Strict release audit.

The audit reports facts separately:
  code complete / host passed / S3 compiled / hardware passed.
It never treats host tests, packaging, or missing inputs as P0 completion.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from radio_config import load_and_validate  # noqa: E402

BASELINE_COMMIT = "34dc4e022780cb75b48db99211a10f1e8ae60a66"
REQUIRED_DOCS = [
    "docs/materials.md",
    "docs/missing-inputs.md",
    "docs/player-integration-status.md",
    "docs/audio-integration.md",
    "docs/protocol-v2.md",
    "docs/display-integration.md",
    "docs/hardware-test-matrix.md",
    "docs/build-and-upload.md",
    "docs/delivery-report.md",
    "docs/continued-development.md",
]
REQUIRED_CODE = [
    "firmware/shared/app_controller.cpp",
    "firmware/shared/app_controller.h",
    "firmware/shared/source_manager.cpp",
    "firmware/shared/source_manager.h",
    "firmware/shared/protocol_v2.cpp",
    "firmware/shared/protocol_v2.h",
    "firmware/shared/input_service.cpp",
    "firmware/shared/input_service.h",
    "firmware/shared/city_catalog.cpp",
    "firmware/shared/city_catalog.h",
    "firmware/shared/map_renderer.cpp",
    "firmware/shared/map_renderer.h",
    "firmware/radio_a/audio_adapter.cpp",
    "firmware/radio_a/audio_adapter.h",
    "firmware/radio_a/radio_link.cpp",
    "firmware/radio_a/radio_link.h",
    "firmware/radio_a/sd_audio_catalog.cpp",
    "firmware/radio_a/sd_audio_catalog.h",
    "firmware/radio_a/wifi_service.cpp",
    "firmware/radio_a/wifi_service.h",
    "firmware/display_b/display_controller.cpp",
    "firmware/display_b/display_controller.h",
    "firmware/display_b/display_link.cpp",
    "firmware/display_b/display_link.h",
]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8") if path.is_file() else ""


def add_check(checks: list[dict[str, object]], cid: str, name: str, passed: bool,
              evidence: str = "", detail: str = "") -> None:
    checks.append(
        {
            "id": cid,
            "name": name,
            "passed": bool(passed),
            "evidence": evidence,
            "detail": detail,
        }
    )


def parse_t_matrix(path: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    if not path.is_file():
        return rows
    for line in path.read_text(encoding="utf-8").splitlines():
        match = re.match(r"\|\s*(T\d{2})\s*\|(.+)\|\s*$", line)
        if not match:
            continue
        cells = [cell.strip() for cell in match.group(2).split("|")]
        if len(cells) < 6:
            continue
        rows.append(
            {
                "id": match.group(1),
                "scenario": cells[0],
                "result": cells[-1],
            }
        )
    return rows


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--strict", action="store_true")
    parser.add_argument(
        "--output",
        default=str(ROOT / "evidence" / "acceptance-report.json"),
    )
    parser.add_argument(
        "--manifest",
        default=str(ROOT / "release" / "manifest.json"),
    )
    args = parser.parse_args()

    checks: list[dict[str, object]] = []
    blockers: list[str] = []
    missing_inputs: list[str] = []

    # 1. Formal city configuration.
    formal_path = ROOT / "config" / "cities.json"
    formal_config: dict = {}
    formal_errors: list[str] = []
    try:
        formal_config, formal_errors = load_and_validate(
            formal_path, require_complete=True
        )
    except Exception as exc:
        formal_errors = [str(exc)]
    formal_ok = not formal_errors and len(formal_config.get("cities", [])) == 8
    add_check(
        checks,
        "A2-config",
        "正式 8 城配置完整",
        formal_ok,
        "config/cities.json",
        "; ".join(formal_errors[:3]),
    )
    if not formal_ok:
        blockers.append("B01 正式 8 城表不完整，严格发布校验失败。")
        missing_inputs.append("完整 8 城表、稳定 id/code、显示顺序与坐标。")

    # 2. Real stream sources and offline audio.
    audio_dir = ROOT / "assets" / "audio"
    audio_files = sorted(audio_dir.glob("*.mp3")) if audio_dir.is_dir() else []
    audio_manifest_path = audio_dir / "manifest.json"
    audio_manifest_ok = False
    audio_entries = 0
    if audio_manifest_path.is_file():
        try:
            manifest = json.loads(audio_manifest_path.read_text(encoding="utf-8"))
            entries = manifest.get("files", [])
            audio_entries = len(entries) if isinstance(entries, list) else 0
            audio_manifest_ok = audio_entries >= 8
        except Exception:
            audio_manifest_ok = False
    audio_ok = len(audio_files) >= 8 and audio_manifest_ok
    add_check(
        checks,
        "A2-audio",
        "8 份离线音频及来源清单",
        audio_ok,
        "assets/audio/",
        f"mp3={len(audio_files)}, manifest_entries={audio_entries}",
    )
    if not audio_ok:
        blockers.append("B03 缺 8 份合法离线音频及逐源来源记录。")
        missing_inputs.append("8 份 MP3 离线音频、时长、码率、来源与使用说明。")

    source_records_path = ROOT / "evidence" / "source-records.json"
    source_records_ok = False
    source_record_count = 0
    if source_records_path.is_file():
        try:
            records = json.loads(source_records_path.read_text(encoding="utf-8")).get(
                "records", []
            )
            source_record_count = len(records) if isinstance(records, list) else 0
            required_fields = {
                "date",
                "network",
                "board",
                "softwareVersion",
                "finalUrl",
                "codec",
                "firstSoundMs",
                "durationMin",
                "result",
            }
            source_records_ok = source_record_count >= 16 and all(
                isinstance(record, dict) and required_fields.issubset(record)
                for record in records
            )
        except Exception:
            source_records_ok = False
    sources_ok = formal_ok and source_records_ok
    add_check(
        checks,
        "A2-sources",
        "16 条真实在线源及逐源记录",
        sources_ok,
        "evidence/source-records.json",
        "正式配置不完整" if not formal_ok else f"records={source_record_count}/16",
    )
    if not sources_ok:
        blockers.append("B02 缺 16 条真实在线源及逐源实测记录。")
        missing_inputs.append("16 条真实主备 URL、台名、编码、首播与持续播放记录。")

    # 3. Existing player / hardware inputs.
    player_status = read_text(ROOT / "docs" / "player-integration-status.md")
    player_ok = (
        (ROOT / "docs" / "player-integration-status.md").is_file()
        and "NOT RECEIVED" not in player_status
        and "已接收" in player_status
    )
    add_check(
        checks,
        "A1-player",
        "既有播放器源码已接收并可回退",
        player_ok,
        "docs/player-integration-status.md",
        "未接收实际播放器工程",
    )
    if not player_ok:
        blockers.append("B04 既有完整播放器源码、依赖和构建脚本未接收。")
        missing_inputs.append("partner 既有完整播放器工程、ESP32-audioI2S 4.0.1 副本与校验值。")

    pins_path = ROOT / "config" / "hardware_pins.local.h"
    pins_text = read_text(pins_path)
    pins_ok = "RADIO_HARDWARE_CONFIGURED 1" in pins_text and (
        "RADIO_DISPLAY_CONFIGURED 1" in pins_text
    )
    add_check(
        checks,
        "A1-io",
        "真实 IO 已填写且无默认 GPIO",
        pins_ok,
        "config/hardware_pins.local.h",
        "未填写" if not pins_path.is_file() else "配置不完整",
    )
    if not pins_ok:
        blockers.append("B05 硬件 IO 未确认；硬件构建必须明确失败，不能采用默认 GPIO。")
        missing_inputs.append("板 A/板 B 实际模组、Flash/PSRAM、分区、屏幕与全部 IO 分配。")

    secrets_ok = (ROOT / "config" / "secrets.local.h").is_file()
    add_check(
        checks,
        "A1-secrets",
        "本地 Wi-Fi 配置存在",
        secrets_ok,
        "config/secrets.local.h",
        "未创建（不读取内容）",
    )
    if not secrets_ok:
        blockers.append("B06 本地 Wi-Fi 配置缺失；不影响主机逻辑，但阻止实机联调。")

    # 4. Environment and build.
    arduino_cli = shutil.which("arduino-cli")
    environment_text = read_text(ROOT / "config" / "environment.lock.md")
    environment_ok = all(
        needle in environment_text
        for needle in ["ESP32-S3", "3.3.12", "4.0.1", "esp32:esp32:esp32s3"]
    )
    add_check(
        checks,
        "A8-env",
        "环境锁定记录完整",
        environment_ok,
        "config/environment.lock.md",
        "complete" if environment_ok else "缺少 S3/依赖版本记录",
    )
    add_check(
        checks,
        "A8-cli",
        "arduino-cli 可用",
        bool(arduino_cli),
        "PATH",
        arduino_cli or "未安装",
    )
    if not arduino_cli:
        blockers.append("B07 本机未安装 arduino-cli，未执行 S3 真实编译。")
        missing_inputs.append("Arduino CLI、Arduino-ESP32 3.3.12、ESP32-audioI2S 4.0.1 依赖及校验值。")

    binaries = sorted((ROOT / "release").glob("*.bin")) if (ROOT / "release").is_dir() else []
    add_check(
        checks,
        "A8-bin",
        "真实编译产物 .bin 存在",
        bool(binaries),
        "release/",
        "未编译，未生成 .bin",
    )
    if not binaries:
        blockers.append("B08 未生成任何 .bin；不得宣称存在可刷写固件。")

    # 5. Host verification evidence.
    host_log = read_text(ROOT / "evidence" / "host-tests.log")
    host_ok = bool(re.search(r"\b\d+ checks, 0 failures\b", host_log))
    add_check(
        checks,
        "C1",
        "主机 C++ 逻辑测试",
        host_ok,
        "evidence/host-tests.log",
        "0 failures" if host_ok else "缺少通过记录",
    )
    config_log = read_text(ROOT / "evidence" / "config-tests.log")
    config_ok = "\nOK" in config_log or config_log.rstrip().endswith("OK")
    add_check(
        checks,
        "C2",
        "配置标准库 unittest",
        config_ok,
        "evidence/config-tests.log",
        "OK" if config_ok else "缺少通过记录",
    )
    preview_ok = (
        (ROOT / "evidence" / "display-preview" / "index.html").is_file()
        and "FIXTURE PREVIEW" in read_text(ROOT / "evidence" / "display-preview" / "index.html")
    )
    add_check(
        checks,
        "C3",
        "显示模拟预览",
        preview_ok,
        "evidence/display-preview/index.html",
        "FIXTURE PREVIEW" if preview_ok else "缺少预览",
    )
    doc_report_path = ROOT / "evidence" / "document-check.json"
    doc_ok = False
    if doc_report_path.is_file():
        try:
            doc_ok = json.loads(doc_report_path.read_text(encoding="utf-8")).get("status") == "pass"
        except Exception:
            doc_ok = False
    add_check(
        checks,
        "C4",
        "Markdown/Word v1.2 一致性",
        doc_ok,
        "evidence/document-check.json",
        "pass" if doc_ok else "fail",
    )

    # 6. T01-T22 matrix.
    t_rows = parse_t_matrix(ROOT / "docs" / "hardware-test-matrix.md")
    t_pass = bool(t_rows) and all(row["result"].upper() == "PASS" for row in t_rows)
    add_check(
        checks,
        "A9",
        "T01-T22 全部实测通过",
        t_pass,
        "docs/hardware-test-matrix.md",
        f"{len(t_rows)} rows",
    )
    if not t_pass:
        blockers.append("B09 T01-T22 未全部实机实测通过；P0 整体保持未完成。")
        missing_inputs.append("目标板、真实网络/音频/屏幕/IO 条件下的 T01-T22 实测证据。")

    # 7. Delivery documentation.
    missing_docs = [doc for doc in REQUIRED_DOCS if not (ROOT / doc).is_file()]
    docs_ok = not missing_docs
    add_check(
        checks,
        "A10",
        "交付文档齐全",
        docs_ok,
        "docs/",
        ", ".join(missing_docs) or "complete",
    )
    if not docs_ok:
        blockers.append("B10 交付文档缺失：" + "、".join(missing_docs))

    logic_code_ok = (
        all((ROOT / path).is_file() for path in REQUIRED_CODE)
        and host_ok
        and config_ok
    )
    add_check(
        checks,
        "A10-code",
        "逻辑源码与主机测试齐全",
        logic_code_ok,
        "firmware/",
        "complete" if logic_code_ok else "missing",
    )
    s3_compiled = bool(arduino_cli and binaries)
    hardware_passed = t_pass
    code_complete = bool(logic_code_ok and player_ok and pins_ok)
    release_ready = all(bool(item["passed"]) for item in checks)

    if not s3_compiled:
        blockers.append("B11 S3 真实目标板编译/上传未执行。")
    if not hardware_passed:
        blockers.append("B12 2 小时活动模式/200 次混合操作、独立构建上传回归未在实机执行。")

    # 8. Hashes and manifest.
    hashed_paths = [
        "README.md",
        "config/cities.json",
        "config/environment.lock.md",
        "config/hardware_pins.example.h",
        "docs/audio-integration.md",
        "docs/build-and-upload.md",
        "docs/continued-development.md",
        "docs/delivery-report.md",
        "docs/display-integration.md",
        "docs/hardware-test-matrix.md",
        "docs/materials.md",
        "docs/missing-inputs.md",
        "docs/protocol-v2.md",
        "docs/player-integration-status.md",
        "evidence/README.md",
        "release/README.md",
        "scripts/build_firmware.py",
        "scripts/check_config.py",
        "scripts/generate_city_data.py",
        "scripts/radio_config.py",
        "scripts/run_host_tests.py",
        "scripts/simulate_display.py",
        "scripts/check_documents.py",
        "scripts/audit_delivery.py",
        "tests/config/test_fixture_isolation.py",
        "tests/config/test_generate_city_data.py",
        "tests/config/test_radio_config.py",
        "tests/config/test_timing_and_protocol.py",
        "tests/logic/test_main.cpp",
        "tests/fixtures/cities.json",
        "../output/短波穿越电台软件开发计划.md",
        "../output/短波穿越电台软件开发计划.docx",
    ] + REQUIRED_CODE
    hashes = {
        path: sha256(ROOT / path)
        for path in sorted(set(hashed_paths))
        if (ROOT / path).is_file()
    }

    generated_at = datetime.now(timezone.utc).isoformat()
    report = {
        "schema_version": 1,
        "generated_at": generated_at,
        "status": "ready" if release_ready else "blocked",
        "release_ready": release_ready,
        "facts": {
            "logic_code_complete": logic_code_ok,
            "code_complete": code_complete,
            "host_passed": bool(host_ok and config_ok and preview_ok and doc_ok),
            "s3_compiled": s3_compiled,
            "hardware_passed": hardware_passed,
        },
        "counts": {
            "formal_cities": len(formal_config.get("cities", [])),
            "audio_files": len(audio_files),
            "audio_manifest_entries": audio_entries,
            "t_matrix_rows": len(t_rows),
            "binaries": len(binaries),
            "blockers": len(blockers),
        },
        "checks": checks,
        "blockers": blockers,
        "missing_inputs": missing_inputs,
        "t01_t22": t_rows,
        "sha256": hashes,
        "notes": [
            "主机测试通过不等于 S3 编译通过，也不等于实机通过。",
            "未生成任何 .bin；release/ 不包含可刷写固件。",
            "fixture 仅用于主机测试和预览，未进入正式配置。",
        ],
    }
    output = Path(args.output)
    if not output.is_absolute():
        output = ROOT / output
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )

    manifest = {
        "schema_version": 1,
        "release_status": "blocked" if not release_ready else "ready",
        "release_ready": release_ready,
        "generated_at": generated_at,
        "baseline_commit": BASELINE_COMMIT,
        "binaries": [path.name for path in binaries],
        "facts": report["facts"],
        "files": [
            {"path": path, "sha256": digest} for path, digest in hashes.items()
        ],
        "blockers": blockers,
        "notes": report["notes"],
    }
    manifest_path = Path(args.manifest)
    if not manifest_path.is_absolute():
        manifest_path = ROOT / manifest_path
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )

    print(f"audit: {report['status']}")
    print(
        f"facts: logic_code={logic_code_ok} code={code_complete} "
        f"host={host_ok and config_ok} s3={s3_compiled} "
        f"hardware={hardware_passed}"
    )
    for blocker in blockers:
        print(f"BLOCKED: {blocker}")
    print(f"report: {output}")
    print(f"manifest: {manifest_path}")
    if args.strict and not release_ready:
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
