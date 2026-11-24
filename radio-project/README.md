# 短波穿越电台 · 双 ESP32-S3 软件

版本：v1.2 实施记录  
日期：2026-11-24  
基线提交：`34dc4e022780cb75b48db99211a10f1e8ae60a66`

## 当前状态

| 类别 | 状态 |
| --- | --- |
| 逻辑代码 | P1–P6 已完成主机可验证部分 |
| 主机测试 | C1/C2/C3/C4 通过 |
| S3 编译 | 未执行；无 `arduino-cli`、无依赖副本、无完整 FQBN |
| 实机验收 | T01–T22 全部 `BLOCKED` |
| 严格发布 | 失败；P0 整体未完成 |

本目录只包含逻辑、接口、配置、测试、预览和文档。没有真实播放器、正式 8 城表/16 个真实源、8 份离线音频、具体板屏与 IO，也没有任何 `.bin`。不得把主机通过、预览生成或打包成功写成全部 P0 完成。

## 目录

```text
firmware/shared/       协议、类型、身份、输入、地图、控制器、音源管理
firmware/radio_a/      AudioAdapter、RadioLink、Wi-Fi、SD 目录
firmware/display_b/    DisplayController、DisplayLink
config/                cities.json、环境锁定、IO/凭据模板
assets/                音频/字体/地图资源目录（当前为空）
scripts/               生成、校验、主机测试、预览、文档检查、发布审核
tests/logic/           Arduino 无关 C++ 测试
tests/config/          标准库 unittest
tests/fixtures/        合成 8 城 fixture（仅测试/预览）
docs/                  材料、缺失项、接入、协议、显示、矩阵、交付
evidence/              原始测试日志、预览、审核报告
release/               交付清单；无固件二进制
```

## 主机验证

```bash
cd radio-project
python3 -B scripts/run_host_tests.py
python3 -B -m unittest discover -s tests/config -p 'test_*.py' -v
python3 -B scripts/simulate_display.py --self-test \
  --fixture tests/fixtures/cities.json --output evidence/display-preview
python3 -B scripts/check_documents.py \
  --markdown ../output/短波穿越电台软件开发计划.md \
  --docx ../output/短波穿越电台软件开发计划.docx
python3 -B scripts/audit_delivery.py --strict \
  --output evidence/acceptance-report.json
```

最后一条预期返回非零：当前缺项必须阻止严格发布。

## 配置与构建门

- `config/cities.json` 是唯一编辑源；正式配置当前故意 `complete: false`。
- `python3 -B scripts/generate_city_data.py` 在正式配置完整后才生成固件城市表。
- `python3 -B scripts/build_firmware.py --board a --dry-run` 会在缺配置/IO/CLI/sketch 时明确失败。
- 禁止默认 GPIO；禁止沿用 WROVER 引脚限制；禁止在未确认 S3 模组时填写 Flash/PSRAM/分区。

## 文档

- `docs/materials.md`：M0 材料与里程碑状态
- `docs/missing-inputs.md`：缺失输入与解除条件
- `docs/audio-integration.md`：音频所有权、就绪与测量
- `docs/protocol-v2.md`：协议 v2 与 v1 迁移
- `docs/display-integration.md`：逻辑地图与实屏条件
- `docs/hardware-test-matrix.md`：T01–T22
- `docs/build-and-upload.md`：构建门与上传说明
- `docs/delivery-report.md`：四类事实与阻塞
- `docs/continued-development.md`：接手顺序

## 回退

基线提交 `34dc4e0` 保存了原 v1.1 计划和诊断 `demo`。当前未修改 demo，未创建分支，未提交，未推送。
