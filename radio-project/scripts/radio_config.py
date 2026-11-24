"""Shared configuration loading, validation and config-id generation.

Only the Python standard library is used so the same checks run on developer
machines and CI without installing anything.
"""

from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any
from urllib.parse import urlparse

SCHEMA_VERSION = 1
CITY_COUNT = 8
COPY_LIMIT = 40
CODE_RE = re.compile(r"^[A-Z0-9_]{1,12}$")
FIXED_OFFSET_RE = re.compile(r"^[+-]?\d+(?::\d{2})?$")


class ConfigError(ValueError):
    """Raised for malformed configuration files."""


def _require_dict(value: Any, name: str) -> dict:
    if not isinstance(value, dict):
        raise ConfigError(f"{name} 必须是对象")
    return value


def _require_number(value: Any, name: str, minimum: float, maximum: float) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ConfigError(f"{name} 必须是数字")
    number = float(value)
    if number < minimum or number > maximum:
        raise ConfigError(f"{name} 超出范围 {minimum}..{maximum}")
    return number


def _require_text(value: Any, name: str, limit: int = 200) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ConfigError(f"{name} 不能为空")
    text = value.strip()
    if len(text) > limit:
        raise ConfigError(f"{name} 超过 {limit} 字符")
    return text


def _validate_url(value: Any, name: str) -> str:
    text = _require_text(value, name)
    if any(character.isspace() for character in text):
        raise ConfigError(f"{name} 不能包含空白")
    parsed = urlparse(text)
    if parsed.scheme not in ("http", "https") or not parsed.netloc:
        raise ConfigError(f"{name} 必须是可访问的 http(s) 直链")
    return text


def _validate_city(city: Any, index: int) -> list[str]:
    errors: list[str] = []
    prefix = f"cities[{index}]"
    if not isinstance(city, dict):
        return [f"{prefix} 必须是对象"]

    try:
        city_id = city["id"]
        if isinstance(city_id, bool) or not isinstance(city_id, int):
            raise ConfigError("id 必须是整数")
        if city_id < 0 or city_id >= CITY_COUNT:
            raise ConfigError(f"id 必须在 0..{CITY_COUNT - 1}")
    except KeyError:
        errors.append(f"{prefix}.id 缺失")
    except ConfigError as exc:
        errors.append(f"{prefix}.id {exc}")

    try:
        code = _require_text(city.get("code"), f"{prefix}.code", 12)
        if not CODE_RE.fullmatch(code):
            raise ConfigError("只能包含大写字母、数字和下划线")
    except ConfigError as exc:
        errors.append(str(exc))

    try:
        _require_text(city.get("name"), f"{prefix}.name", 40)
    except ConfigError as exc:
        errors.append(str(exc))

    try:
        _require_number(city.get("longitude"), f"{prefix}.longitude", -180.0, 180.0)
        _require_number(city.get("latitude"), f"{prefix}.latitude", -90.0, 90.0)
    except ConfigError as exc:
        errors.append(str(exc))

    for field in ("primaryUrl", "backupUrl"):
        try:
            _validate_url(city.get(field), f"{prefix}.{field}")
        except ConfigError as exc:
            errors.append(str(exc))

    for field in ("primaryStation", "backupStation", "offlineLabel"):
        try:
            _require_text(city.get(field), f"{prefix}.{field}", 80)
        except ConfigError as exc:
            errors.append(str(exc))

    try:
        offline_path = _require_text(city.get("offlinePath"), f"{prefix}.offlinePath", 120)
        if not offline_path.startswith("/") or not offline_path.endswith(".mp3"):
            raise ConfigError("必须是 / 开头、.mp3 结尾的绝对路径")
        if ".." in offline_path or any(character.isspace() for character in offline_path):
            raise ConfigError("路径不能包含 .. 或空白")
    except ConfigError as exc:
        errors.append(str(exc))

    try:
        timezone_rule = _require_text(city.get("timezoneRule"), f"{prefix}.timezoneRule", 64)
        if FIXED_OFFSET_RE.fullmatch(timezone_rule):
            raise ConfigError("不能只写固定整数时差，必须是经验证的时区规则标识")
    except ConfigError as exc:
        errors.append(str(exc))

    try:
        copy = _require_text(city.get("copy"), f"{prefix}.copy", COPY_LIMIT)
        if len(copy) > COPY_LIMIT:
            raise ConfigError(f"超过 {COPY_LIMIT} 字")
    except ConfigError as exc:
        errors.append(str(exc))

    try:
        order = city.get("displayOrder")
        if isinstance(order, bool) or not isinstance(order, int):
            raise ConfigError("displayOrder 必须是整数")
        if order < 0 or order >= CITY_COUNT:
            raise ConfigError(f"displayOrder 必须在 0..{CITY_COUNT - 1}")
    except ConfigError as exc:
        errors.append(str(exc))

    profile = city.get("streamProfile")
    if profile is not None:
        if not isinstance(profile, dict):
            errors.append(f"{prefix}.streamProfile 必须是对象")
        else:
            codec = profile.get("codec")
            if codec not in ("MP3", "AAC", "OGG"):
                errors.append(f"{prefix}.streamProfile.codec 必须是 MP3/AAC/OGG")
            bitrate = profile.get("bitrateKbps")
            if isinstance(bitrate, bool) or not isinstance(bitrate, int) or bitrate <= 0:
                errors.append(f"{prefix}.streamProfile.bitrateKbps 必须是正整数")
            if profile.get("auth") not in ("none", "basic", "token"):
                errors.append(f"{prefix}.streamProfile.auth 必须是 none/basic/token")

    return errors


def validate_config(config: dict, require_complete: bool = True) -> list[str]:
    """Return a list of human-readable validation errors."""
    errors: list[str] = []
    if not isinstance(config, dict):
        return ["根节点必须是对象"]
    if config.get("schemaVersion") != SCHEMA_VERSION:
        errors.append(f"schemaVersion 必须是 {SCHEMA_VERSION}")
    if not isinstance(config.get("complete"), bool):
        errors.append("complete 必须是布尔值")
        return errors

    complete = config["complete"]
    if complete:
        if "status" not in config:
            errors.append("complete=true 时必须提供 status")
    else:
        if not str(config.get("status", "")).strip():
            errors.append("complete=false 时必须提供 status")
        if not str(config.get("reason", "")).strip():
            errors.append("complete=false 时必须提供 reason")

    origin = config.get("origin")
    if not isinstance(origin, dict):
        errors.append("origin 必须是对象")
    else:
        try:
            _require_text(origin.get("code"), "origin.code", 12)
            _require_text(origin.get("name"), "origin.name", 40)
            _require_number(origin.get("longitude"), "origin.longitude", -180.0, 180.0)
            _require_number(origin.get("latitude"), "origin.latitude", -90.0, 90.0)
        except ConfigError as exc:
            errors.append(str(exc))

    cities = config.get("cities")
    if not isinstance(cities, list):
        errors.append("cities 必须是数组")
        cities = []
    if complete and len(cities) != CITY_COUNT:
        errors.append(f"complete=true 时必须有 {CITY_COUNT} 个城市，当前 {len(cities)}")
    if not complete and len(cities) > CITY_COUNT:
        errors.append(f"未完成配置的城市数不能超过 {CITY_COUNT}")

    ids: set[int] = set()
    codes: set[str] = set()
    orders: set[int] = set()
    for index, city in enumerate(cities):
        city_errors = _validate_city(city, index)
        errors.extend(city_errors)
        if isinstance(city, dict):
            city_id = city.get("id")
            if isinstance(city_id, int) and not isinstance(city_id, bool):
                if city_id in ids:
                    errors.append(f"cities[{index}].id 重复")
                ids.add(city_id)
            code = city.get("code")
            if isinstance(code, str):
                if code in codes:
                    errors.append(f"cities[{index}].code 重复")
                codes.add(code)
            order = city.get("displayOrder")
            if isinstance(order, int) and not isinstance(order, bool):
                if order in orders:
                    errors.append(f"cities[{index}].displayOrder 重复")
                orders.add(order)

    if complete:
        if ids != set(range(CITY_COUNT)):
            errors.append("城市 id 必须恰好覆盖 0..7")
        if orders != set(range(CITY_COUNT)):
            errors.append("displayOrder 必须恰好覆盖 0..7")
    if require_complete and not complete:
        errors.append("正式配置不完整：缺少完整8城表，严格发布校验失败")
    return errors


def load_config(path: str | Path) -> dict:
    file_path = Path(path)
    try:
        text = file_path.read_text(encoding="utf-8")
    except OSError as exc:
        raise ConfigError(f"无法读取配置文件 {file_path}: {exc}") from exc
    try:
        config = json.loads(text)
    except json.JSONDecodeError as exc:
        raise ConfigError(f"JSON 解析失败 {file_path}: {exc}") from exc
    if not isinstance(config, dict):
        raise ConfigError("根节点必须是对象")
    return config


def config_id(config: dict) -> str:
    """FNV-1a 32-bit over a canonical projection of the editable source."""
    payload = {
        "schemaVersion": config.get("schemaVersion"),
        "origin": config.get("origin"),
        "cities": config.get("cities", []),
    }
    canonical = json.dumps(
        payload, sort_keys=True, separators=(",", ":"), ensure_ascii=False
    ).encode("utf-8")
    value = 0x811C9DC5
    for byte in canonical:
        value ^= byte
        value = (value * 0x01000193) & 0xFFFFFFFF
    return f"{value:08X}"


def load_and_validate(
    path: str | Path, require_complete: bool = True
) -> tuple[dict, list[str]]:
    config = load_config(path)
    return config, validate_config(config, require_complete=require_complete)
