# 显示与地图逻辑说明

版本：v1.2 实施记录  
日期：2026-11-24

## 1 逻辑坐标

- 逻辑画布：1000 × 500。
- 经度映射横坐标，纬度映射纵坐标；上海为固定起点。
- `MapRenderer::project()`、`crossesDateline()`、`buildArc()`、`pointAt()` 为纯逻辑，不依赖 Arduino。
- 跨日期变更线使用经度回绕与断点标记，弧线不穿过整张地图。

## 2 预览产物

`python3 -B scripts/simulate_display.py --self-test --fixture tests/fixtures/cities.json --output evidence/display-preview`

生成：

- `evidence/display-preview/index.html`：可在电脑直接打开的自包含预览；
- `evidence/display-preview/map.svg`：逻辑地图、上海起点、城市落点、弧线与光点；
- `evidence/display-preview/standby.svg`：待机动画预览；
- `evidence/display-preview/preview.json`：配置标识、落点与跨日期变更线标记。

预览使用 `tests/fixtures/cities.json` 的合成数据，并明确标注 `FIXTURE PREVIEW / 非正式8城落点验收`。真实 8 城表未交付，不能宣称正式落点验收通过。

## 3 实屏接入条件

实屏驱动、扫描方式、色深、尺寸与显示库未确认，当前不接入真实刷新驱动。确认后必须记录：

- 屏幕型号、分辨率、扫描方式、色深、库版本与依赖校验值；
- 缓存方式与内存占用（帧缓冲、DMA、字库、PSRAM）；
- 刷新帧率与阻塞情况，目标 20–30 fps；
- 城市落点、弧线、光点、待机动画和来源/错误状态的实屏证据。

实屏验收不能由 HTML/SVG 预览代替。IO 未填写时硬件构建必须明确失败，不采用默认 GPIO。

## 4 主机覆盖

- 上海投影与 8 城落点在逻辑范围内；
- 跨日期变更线弧线产生断点；
- 同 request 重复快照不重播；
- 新 request 取消旧动画；
- 显示超时 3500 ms 降级并允许已就绪音频出声。
