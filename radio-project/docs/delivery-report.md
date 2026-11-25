# 交付报告

2026-11-25 增补：外设 IO 已录入两份共享头文件，新增硬件配置校验、修正板 B TF 卡必填项并统一构建/发布门。当前 Python 共 34 项回归；完整接线与本轮边界见 `docs/hardware-io-review.md`。下列 v1.2 测试数量及 Git 状态为 2026-11-24 历史记录，当前以 `evidence/` 和 Git 状态为准。

版本：v1.2 实施记录  
日期：2026-11-24  
基线提交：`34dc4e022780cb75b48db99211a10f1e8ae60a66`

## 1 四类事实

| 类别 | 状态 | 依据 |
| --- | --- | --- |
| 代码完成 | 部分 | 逻辑模块、协议、配置、主机测试、模拟预览与文档已完成；真实播放器/实屏/硬件集成未完成，整体代码不算完成 |
| 主机通过 | 是 | C1/C2/C3/C4 通过，原始结果在 `evidence/` |
| S3 编译通过 | 否 | 本机无 `arduino-cli`，无依赖副本，无完整 FQBN，未生成 `.bin` |
| 实机通过 | 否 | T01–T22 全部 `BLOCKED`，无目标板/屏幕/音频/IO 实测 |

**结论：P0 整体未完成，严格发布审核失败。**

## 2 已完成内容

- P1：基线、材料清单、环境状态、缺失输入、S3 目标与禁止默认 IO 已登记。
- P2：CityCatalog、配置 schema、城市数据生成、旋钮正交解码、可校准卡点、50 ms 消抖、首尾循环、session/request/attempt。
- P3：AudioAdapter 单所有者接口、代次隔离、PCM 200 ms 就绪、失败分类、SD 目录检查、Wi-Fi 状态、接入说明与故障模拟。
- P4：AppController/SourceManager、可控时钟、无阻塞调度、主备离线单向降级、两种待机策略、旋转最高优先级、回绕与快速选城。
- P5：协议 v2 编解码、160 字节有界解析、握手、ST/DONE/ACK、250 ms × 8 重发、会话隔离、双板重启恢复、HTML/SVG 显示模拟端。
- P6：逻辑坐标地图、上海起点、城市落点、跨日期变更线弧线、光点、待机动画、来源/错误状态、fixture 预览。
- P8（主机部分）：`run_host_tests.py`、`tests/config` unittest、`simulate_display.py`、`check_documents.py`、`audit_delivery.py`、README、交付清单、验收矩阵。

## 3 验证结果

| 检查 | 命令 | 结果 |
| --- | --- | --- |
| C1 | `python3 -B scripts/run_host_tests.py` | 665 checks, 0 failures |
| C2 | `python3 -B -m unittest discover -s tests/config -p 'test_*.py' -v` | 17 tests, OK |
| C3 | `python3 -B scripts/simulate_display.py --self-test --fixture tests/fixtures/cities.json --output evidence/display-preview` | self-test OK，fixture 8 城 |
| C4 | `python3 -B scripts/check_documents.py --markdown ... --docx ...` | 31/31 pass |
| C5 | `python3 -B scripts/audit_delivery.py --strict --output evidence/acceptance-report.json` | 退出码 2（预期：缺项阻塞） |

## 4 阻塞项

见 `evidence/acceptance-report.json` 的 `blockers` 与 `missing_inputs`。主要包括：完整播放器未接收、正式 8 城表/16 源缺失、8 份离线音频缺失、S3 模组/Flash/PSRAM/分区/USB 未确认、屏幕与显示库未确认、外设 IO 已接收但 UART 与模组/电气核对未完成、`arduino-cli` 与依赖未安装、T01–T22 未实机执行、无 `.bin`。

## 5 已知限制

- 主机测试使用合成 fixture 与 fake audio/link，不证明真实解码、I2S、SD、UART 或屏幕。
- 显示预览是逻辑坐标，不是实屏验收，也不代表正式 8 城落点。
- `Esp32AudioBackend` 是待核验草案，未编译，不能称为可播放实现。
- 未执行 2 小时实机/200 次混合操作、独立构建上传回归、真实超时误差与输入静音 ≤100 ms 实测。

## 6 回退

基线提交 `34dc4e022780cb75b48db99211a10f1e8ae60a66` 保存了原 v1.1 计划与诊断 demo。当前工作区未提交，未修改 demo，未创建分支，未推送。回退时使用该提交中的计划与 demo；不要用未验证的固件回退。

## 7 下一步

1. 接收完整播放器源码/依赖并记录 SHA256，完成 M0 冒烟。
2. 接收完整 8 城表/16 源/8 份音频，替换巴黎与开罗候选源并逐源复验。
3. 核对已接收的双板 IO，补齐 UART，确认 S3 模组/Flash/PSRAM、分区、USB、屏幕及电气条件，再启用对应的 `hardware_pins_a.h` / `hardware_pins_b.h`（或完整 `.local.h`）。
4. 安装锁定版本 Arduino 工具链，完成 S3 编译、上传与 PSRAM/MAX98357/SD/网络/双板显示验证。
5. 执行 T01–T22、2 小时/200 次与独立构建上传回归，补齐证据后再运行严格审核。
