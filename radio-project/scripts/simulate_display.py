#!/usr/bin/env python3
"""Generate a self-contained logical map/standby preview from the test fixture.

The preview intentionally uses only ``tests/fixtures/cities.json``.  It is a
logic-coordinate animation preview, not a production city table and not a
substitute for real-panel acceptance.
"""

from __future__ import annotations

import argparse
import html
import json
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from radio_config import config_id, load_and_validate  # noqa: E402

WIDTH = 1000
HEIGHT = 500
DEFAULT_FIXTURE = ROOT / "tests" / "fixtures" / "cities.json"


def project(longitude: float, latitude: float) -> tuple[float, float]:
    return (
        (longitude + 180.0) / 360.0 * WIDTH,
        (90.0 - latitude) / 180.0 * HEIGHT,
    )


def wrap_longitude(longitude: float) -> float:
    while longitude > 180.0:
        longitude -= 360.0
    while longitude < -180.0:
        longitude += 360.0
    return longitude


def crosses_dateline(from_lon: float, to_lon: float) -> bool:
    delta = to_lon - from_lon
    return delta > 180.0 or delta < -180.0


def build_arc(
    from_lon: float, from_lat: float, to_lon: float, to_lat: float, steps: int = 48
) -> list[dict[str, float | bool]]:
    delta = to_lon - from_lon
    if delta > 180.0:
        delta -= 360.0
    if delta < -180.0:
        delta += 360.0
    adjusted_to_lon = from_lon + delta
    mid_lon = (from_lon + adjusted_to_lon) * 0.5
    mid_lat = (from_lat + to_lat) * 0.5
    control_lon = mid_lon
    control_lat = mid_lat + 8.0 + abs(delta) / 360.0 * 18.0

    points: list[dict[str, float | bool]] = []
    previous: float | None = None
    for index in range(steps + 1):
        t = index / steps
        inverse = 1.0 - t
        lon = (
            inverse * inverse * from_lon
            + 2.0 * inverse * t * control_lon
            + t * t * adjusted_to_lon
        )
        lat = (
            inverse * inverse * from_lat
            + 2.0 * inverse * t * control_lat
            + t * t * to_lat
        )
        wrapped = wrap_longitude(lon)
        break_before = previous is not None and abs(wrapped - previous) > 180.0
        x, y = project(wrapped, lat)
        points.append({"x": x, "y": y, "breakBefore": break_before})
        previous = wrapped
    return points


def path_for(points: list[dict[str, float | bool]]) -> str:
    parts: list[str] = []
    for point in points:
        command = "M" if point["breakBefore"] or not parts else "L"
        parts.append(f"{command} {point['x']:.1f} {point['y']:.1f}")
    return " ".join(parts)


def self_test(config: dict) -> list[str]:
    errors: list[str] = []
    if not config.get("complete"):
        errors.append("预览配置必须完整")
    if not config.get("fixture"):
        errors.append("预览配置必须明确标注为 fixture")
    cities = config.get("cities", [])
    if len(cities) != 8:
        errors.append(f"预览城市数应为8，实际 {len(cities)}")
    if sorted(city["displayOrder"] for city in cities) != list(range(8)):
        errors.append("displayOrder 必须恰好覆盖 0..7")
    if sorted(city["id"] for city in cities) != list(range(8)):
        errors.append("id 必须恰好覆盖 0..7")

    shanghai = project(config["origin"]["longitude"], config["origin"]["latitude"])
    if abs(shanghai[0] - 837.4) > 1.0 or abs(shanghai[1] - 163.2) > 1.0:
        errors.append(f"上海投影异常: {shanghai}")
    if not crosses_dateline(170.0, -170.0):
        errors.append("跨日期变更线判定失败")

    dateline_city = next((city for city in cities if city["code"] == "FIX_H"), None)
    if dateline_city is None:
        errors.append("缺少用于跨日期变更线预览的 FIX_H")
    else:
        arc = build_arc(
            config["origin"]["longitude"],
            config["origin"]["latitude"],
            dateline_city["longitude"],
            dateline_city["latitude"],
        )
        if not any(point["breakBefore"] for point in arc):
            errors.append("跨日期变更线弧线未产生断点")
        if not crosses_dateline(
            config["origin"]["longitude"], dateline_city["longitude"]
        ):
            errors.append("FIX_H 未跨越日期变更线")

    for city in cities:
        x, y = project(city["longitude"], city["latitude"])
        if not (0.0 <= x <= WIDTH and 0.0 <= y <= HEIGHT):
            errors.append(f"{city['code']} 落点超出逻辑地图: {(x, y)}")
    return errors


def graticule_svg() -> str:
    parts: list[str] = []
    for longitude in range(-180, 181, 30):
        x, _ = project(longitude, 0)
        parts.append(
            f'<line class="grid" x1="{x:.1f}" y1="0" x2="{x:.1f}" y2="{HEIGHT}"/>'
        )
        if longitude % 60 == 0:
            label = "0°" if longitude == 0 else f"{longitude:+d}°"
            parts.append(
                f'<text class="axis" x="{x:.1f}" y="{HEIGHT - 5:.1f}" '
                f'text-anchor="middle">{label}</text>'
            )
    for latitude in range(-90, 91, 30):
        _, y = project(0, latitude)
        parts.append(
            f'<line class="grid" x1="0" y1="{y:.1f}" x2="{WIDTH}" y2="{y:.1f}"/>'
        )
        if latitude != 0:
            parts.append(
                f'<text class="axis" x="5" y="{y - 3:.1f}">{latitude:+d}°</text>'
            )
    return "\n".join(parts)


def map_svg(config: dict, standalone: bool = False) -> str:
    origin = config["origin"]
    origin_x, origin_y = project(origin["longitude"], origin["latitude"])
    cities = sorted(config["cities"], key=lambda city: city["displayOrder"])
    body: list[str] = [
        '<rect class="map-bg" x="0" y="0" width="1000" height="500"/>',
        graticule_svg(),
        '<text class="map-title" x="24" y="34">逻辑坐标地图 · 上海起点</text>',
        '<text class="map-subtitle" x="24" y="52">FIXTURE PREVIEW / 非正式8城落点验收</text>',
    ]

    for city in cities:
        arc = build_arc(
            origin["longitude"],
            origin["latitude"],
            city["longitude"],
            city["latitude"],
        )
        body.append(
            f'<path class="arc" data-city="{html.escape(city["code"])}" '
            f'd="{path_for(arc)}"/>'
        )
        x, y = project(city["longitude"], city["latitude"])
        anchor = "end" if x > 780 else "start"
        label_x = x - 12 if anchor == "end" else x + 12
        source = "MAIN / BACKUP / OFFLINE"
        body.extend(
            [
                f'<circle class="city-pulse" cx="{x:.1f}" cy="{y:.1f}" r="10"/>',
                f'<circle class="city-dot" cx="{x:.1f}" cy="{y:.1f}" r="4"/>',
                f'<text class="city-name" x="{label_x:.1f}" y="{y - 8:.1f}" '
                f'text-anchor="{anchor}">{html.escape(city["name"])}</text>',
                f'<text class="city-meta" x="{label_x:.1f}" y="{y + 8:.1f}" '
                f'text-anchor="{anchor}">{html.escape(city["code"])} · {source}</text>',
            ]
        )
        if crosses_dateline(origin["longitude"], city["longitude"]):
            note_x = max(6.0, min(x, WIDTH - 6.0))
            note_anchor = "start" if x < 60 else ("end" if x > 940 else "middle")
            body.append(
                f'<text class="dateline-note" x="{note_x:.1f}" y="{y + 24:.1f}" '
                f'text-anchor="{note_anchor}">跨日期变更线弧线</text>'
            )

    body.extend(
        [
            f'<circle class="origin-pulse" cx="{origin_x:.1f}" cy="{origin_y:.1f}" r="12"/>',
            f'<circle class="origin-dot" cx="{origin_x:.1f}" cy="{origin_y:.1f}" r="5"/>',
            f'<text class="origin-label" x="{origin_x:.1f}" y="{origin_y + 30:.1f}" '
            f'text-anchor="middle">{html.escape(origin["name"])} · 起点</text>',
        ]
    )

    style = """
<style>
  .map-bg { fill: #07111f; }
  .grid { stroke: #1d3b55; stroke-width: 0.6; opacity: 0.65; }
  .axis { fill: #6f8aa3; font: 10px ui-monospace, Menlo, monospace; }
  .map-title { fill: #e8f4ff; font: 700 20px -apple-system, "PingFang SC", sans-serif; }
  .map-subtitle { fill: #f0b429; font: 700 11px ui-monospace, Menlo, monospace; letter-spacing: 1px; }
  .arc { fill: none; stroke: #46d3ff; stroke-width: 2.2; stroke-linecap: round;
         stroke-dasharray: 7 6; animation: dash 1.2s linear infinite;
         filter: drop-shadow(0 0 4px rgba(70,211,255,0.65)); }
  .city-dot { fill: #ffd166; stroke: #fff3c4; stroke-width: 1.2; }
  .city-pulse { fill: none; stroke: #ffd166; stroke-width: 1.4; opacity: 0;
                animation: pulse 2.4s ease-out infinite; }
  .city-name { fill: #f4f8fb; font: 700 13px -apple-system, "PingFang SC", sans-serif;
               paint-order: stroke; stroke: #07111f; stroke-width: 3px; }
  .city-meta { fill: #9fb7c9; font: 10px ui-monospace, Menlo, monospace;
               paint-order: stroke; stroke: #07111f; stroke-width: 3px; }
  .dateline-note { fill: #ff8fa3; font: 10px -apple-system, "PingFang SC", sans-serif;
                   paint-order: stroke; stroke: #07111f; stroke-width: 3px; }
  .origin-dot { fill: #5eead4; stroke: #ccfbf1; stroke-width: 1.2; }
  .origin-pulse { fill: none; stroke: #5eead4; stroke-width: 1.2; opacity: 0;
                  animation: pulse 2.4s ease-out infinite; }
  .origin-label { fill: #ccfbf1; font: 700 13px -apple-system, "PingFang SC", sans-serif;
                  paint-order: stroke; stroke: #07111f; stroke-width: 3px; }
  @keyframes dash { to { stroke-dashoffset: -26; } }
  @keyframes pulse { 0% { opacity: 0.8; r: 4; } 70% { opacity: 0; r: 20; } 100% { opacity: 0; r: 20; } }
</style>
"""
    if standalone:
        return (
            '<?xml version="1.0" encoding="UTF-8"?>\n'
            f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1000 500" '
            f'width="1000" height="500" role="img" aria-label="逻辑坐标地图预览">'
            f"{style}<g>{''.join(body)}</g></svg>\n"
        )
    return f'<svg viewBox="0 0 1000 500" role="img" aria-label="逻辑坐标地图预览">{style}<g>{"".join(body)}</g></svg>'


def standby_svg(standalone: bool = False) -> str:
    style = """
<style>
  .standby-bg { fill: #07111f; }
  .ring { fill: none; stroke: #46d3ff; stroke-width: 1.4; opacity: 0;
          animation: standby 3.2s ease-out infinite; }
  .core { fill: #5eead4; }
  .standby-text { fill: #e8f4ff; font: 700 16px -apple-system, "PingFang SC", sans-serif; }
  .standby-sub { fill: #9fb7c9; font: 11px ui-monospace, Menlo, monospace; }
  @keyframes standby { 0% { opacity: 0.8; r: 6; } 70% { opacity: 0; r: 54; } 100% { opacity: 0; r: 54; } }
</style>
"""
    body = """
<rect class="standby-bg" x="0" y="0" width="320" height="180"/>
<circle class="ring" cx="160" cy="82" r="6"/>
<circle class="ring" cx="160" cy="82" r="6" style="animation-delay:1.05s"/>
<circle class="ring" cx="160" cy="82" r="6" style="animation-delay:2.1s"/>
<circle class="core" cx="160" cy="82" r="5"/>
<text class="standby-text" x="160" y="140" text-anchor="middle">待机动画预览</text>
<text class="standby-sub" x="160" y="158" text-anchor="middle">STANDBY / 20 s idle</text>
"""
    if standalone:
        return (
            '<?xml version="1.0" encoding="UTF-8"?>\n'
            '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 320 180" '
            'width="320" height="180" role="img" aria-label="待机动画预览">'
            f"{style}{body}</svg>\n"
        )
    return f'<svg viewBox="0 0 320 180" role="img" aria-label="待机动画预览">{style}{body}</svg>'


def build_html(config: dict) -> str:
    cities = sorted(config["cities"], key=lambda city: city["displayOrder"])
    rows = []
    for city in cities:
        rows.append(
            "<tr>"
            f"<td>{city['id']}</td>"
            f"<td>{html.escape(city['code'])}</td>"
            f"<td>{html.escape(city['name'])}</td>"
            f"<td>{html.escape(city['primaryStation'])}</td>"
            f"<td>{html.escape(city['backupStation'])}</td>"
            f"<td>{html.escape(city['offlineLabel'])}</td>"
            "<td>FIXTURE</td>"
            "</tr>"
        )
    return f"""<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>短波穿越电台 · 逻辑显示预览（FIXTURE）</title>
<style>
  :root {{ color-scheme: dark; }}
  body {{ margin: 0; background: #050b14; color: #dbeaf5;
          font-family: -apple-system, "PingFang SC", "Microsoft YaHei", sans-serif; }}
  .wrap {{ max-width: 1120px; margin: 0 auto; padding: 24px; }}
  h1 {{ margin: 0 0 6px; font-size: 24px; color: #f2f8ff; }}
  .banner {{ border: 1px solid #f0b429; background: rgba(240,180,41,0.12);
             color: #ffd97a; padding: 12px 14px; border-radius: 8px; margin: 12px 0 18px; }}
  .grid {{ display: grid; grid-template-columns: minmax(0, 1fr) 320px; gap: 18px; }}
  .card {{ background: #0a1624; border: 1px solid #1d3b55; border-radius: 10px;
           padding: 12px; overflow: hidden; }}
  svg {{ width: 100%; height: auto; display: block; }}
  table {{ width: 100%; border-collapse: collapse; margin-top: 18px; font-size: 13px; }}
  th, td {{ border-bottom: 1px solid #1d3b55; padding: 7px 8px; text-align: left; }}
  th {{ color: #8fb3cc; background: #0a1624; }}
  td {{ color: #c9dbe8; }}
  .meta {{ color: #8fa9bb; font: 12px ui-monospace, Menlo, monospace; }}
  @media (max-width: 900px) {{ .grid {{ grid-template-columns: 1fr; }} }}
</style>
</head>
<body>
<div class="wrap">
  <h1>短波穿越电台 · 逻辑显示预览</h1>
  <div class="meta">config_id={config_id(config)} · fixture=true · 8 synthetic cities</div>
  <div class="banner">
    <strong>FIXTURE PREVIEW / 非正式验收：</strong>
    真实 8 城表、16 个真实源、8 份离线音频、实屏和 IO 均未交付。
    本页面只验证逻辑坐标、弧线、光点、待机动画和状态展示，不代表正式 8 城落点或实屏验收。
  </div>
  <div class="grid">
    <div class="card">{map_svg(config)}</div>
    <div class="card">{standby_svg()}</div>
  </div>
  <table>
    <thead><tr><th>id</th><th>code</th><th>城市</th><th>主台</th><th>备台</th><th>离线标签</th><th>来源状态</th></tr></thead>
    <tbody>{''.join(rows)}</tbody>
  </table>
</div>
</body>
</html>
"""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixture", default=str(DEFAULT_FIXTURE))
    parser.add_argument("--output", default=str(ROOT / "evidence" / "display-preview"))
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    try:
        config, errors = load_and_validate(Path(args.fixture), require_complete=False)
    except Exception as exc:  # ConfigError or file error
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    errors = self_test(config) + errors
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1

    output = Path(args.output)
    if not output.is_absolute():
        output = ROOT / output
    output.mkdir(parents=True, exist_ok=True)
    (output / "index.html").write_text(build_html(config), encoding="utf-8")
    (output / "map.svg").write_text(map_svg(config, standalone=True), encoding="utf-8")
    (output / "standby.svg").write_text(standby_svg(standalone=True), encoding="utf-8")
    metadata = {
        "schema_version": 1,
        "fixture": True,
        "config_id": config_id(config),
        "city_count": len(config["cities"]),
        "origin": config["origin"],
        "cities": [
            {
                "id": city["id"],
                "code": city["code"],
                "name": city["name"],
                "displayOrder": city["displayOrder"],
                "longitude": city["longitude"],
                "latitude": city["latitude"],
                "x": project(city["longitude"], city["latitude"])[0],
                "y": project(city["longitude"], city["latitude"])[1],
                "crosses_dateline": crosses_dateline(
                    config["origin"]["longitude"], city["longitude"]
                ),
            }
            for city in config["cities"]
        ],
        "warning": "FIXTURE PREVIEW ONLY - not a production 8-city acceptance.",
    }
    (output / "preview.json").write_text(
        json.dumps(metadata, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(
        f"generated {output} (fixture config_id={config_id(config)}, "
        f"{len(config['cities'])} synthetic cities)"
    )
    if args.self_test:
        print("self-test: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
