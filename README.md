# 短波穿越电台

基于双 ESP32-S3 的城市电台交互装置：转动同一个可按压旋钮选择城市，地图显示从上海飞向目标城市的弧线，音频与动画共同就绪后播放该城市的互联网电台。在线来源按主用、备用、本地 SD 音频依次降级，设备在无有效操作后进入待机。

**当前状态：主机可验证的逻辑、工具和模拟预览已交付；真实播放器接入、S3 固件构建、双板实机联调及整体验收仍为 `BLOCKED`。目前没有可直接上传的 Arduino sketch，也没有可刷写的 `.bin`。**

进度快照日期：**2026-11-24**。本文依据当前源码、测试日志和交付报告编写，对应已提交的实现快照 `45dc1b0`。开发计划文档为 **v1.2**；执行工作流计划为 **v2**，两者独立编号。工作流 v2 只补充了一份固件说明文件的修改范围，未变更功能或验收标准。

快速导航：[当前进度](#当前进度) · [已有实现](#已有实现) · [主机验证与预览](#主机验证与预览) · [配置与硬件构建](#配置与硬件构建) · [阻塞项与接续顺序](#阻塞项与接续顺序) · [已知限制与待审查事项](#已知限制与待审查事项) · [文档与证据索引](#文档与证据索引)

## 项目概述

### 目标体验与分工

| 部分 | 职责 | 当前落地情况 |
| --- | --- | --- |
| 板 A：ESP32-S3 | 旋钮输入、城市与状态决策、Wi-Fi、音频来源、SD、出声控制和待机 | 逻辑模块、接口及故障模拟已有；真实音频与硬件运行待集成 |
| 板 B：ESP32-S3 | 地图、城市光点、飞行弧线、待机动画、来源和错误状态 | 逻辑渲染与电脑预览已有；实屏驱动待确定 |
| 两板链路 | 通过状态快照同步当前请求，确认动画完成，处理重发与重启恢复 | v2 编解码及模拟链路已测试；实际 UART 未联调 |
| 音频输出 | 解码得到 PCM，经 I2S 送入 MAX98357 模块 | 输出路线已确定，实际库接口、模组配置和 IO 尚待验证 |

用户选城后，板 A 的音频准备与板 B 的动画并行开始。只有当前请求的音频就绪，且动画完成或已明确进入显示失联降级，才允许出声。旋转始终具有最高事件处理优先级，并使旧请求失效。

### 本轮范围

本轮包括 8 城选择、主备与离线策略、待机、板间同步、地图动画、异常恢复、配置生成、测试及交接说明。旋钮按压仅产生一次 `ENCODER_PRESS` 并刷新活动时间；待机时短按不唤醒，活动时短按不切源、不翻页。

城市内容页、`INFO` 页面绘制、当地时间、手机应用、云后台、OTA、自动搜台和额外实体音量控制均不在本轮范围。`INFO` 只保留枚举扩展边界。项目名称中的“短波”是作品命名，当前音源通过 Wi-Fi 获取互联网电台，不涉及射频接收或调谐。

## 当前进度

### 四类完成状态

| 维度 | 当前结论 | 依据与边界 |
| --- | --- | --- |
| 代码实现 | **部分完成** | 输入、状态机、切源策略、协议、显示逻辑、主机工具已实现；真实播放后端、实屏驱动和 Arduino 入口尚未完成 |
| 主机验证 | **C1–C4 通过** | C++ 665 项检查、Python 17 项测试、预览自测、文档 31 项检查通过 |
| ESP32-S3 构建 | **未执行** | 缺实际播放器、完整板型参数、硬件配置和锁定工具链；无 `.bin` |
| 实机验收 | **未执行** | T01–T22 全部标记 `BLOCKED`，无真实音频、SD、UART、屏幕及稳定性通过证据 |
| 严格发布审核 | **未通过** | C5 退出码为 2，`release_ready` 为 `false`，P0 整体未完成 |
| 独立代码审查 | **待完成** | 当前记录来自实施与自查，工作流尚未产出后台 GPT 审查通过结果 |

### 对照原计划 M0–M7

| 里程碑 | 已完成部分 | 尚未完成部分 |
| --- | --- | --- |
| M0 接收并封存单城工程 | 原诊断 demo、计划及 Git 基线已保存；材料和环境缺项已登记 | 接收 partner 的完整播放器、依赖及构建脚本，执行短时构建上传冒烟、PSRAM 和调用耗时核对 |
| M1 复用播放链路 | `AudioAdapter` 接口、单所有者调度、代次隔离、就绪检测和故障模拟 | 接入原有可出声工程，验证真实解码、静音、取消、I2S 和 SD 播放 |
| M2 输入和城市选择 | 正交解码、可校准卡点、消抖、首尾循环、请求编号、城市配置校验与生成 | 真实旋钮方向和卡点标定，正反各 50 格及快速换城实测 |
| M3 主备离线和待机 | 模拟时钟验证切源、首次加载、两种待机策略、超时、优先级与计时回绕 | 真实在线源和 SD 故障注入，输入静音响应及底层调用阻塞测量 |
| M4 协议和显示模拟 | v2 解析、握手、幂等、重发、旧事件拒绝和双板重启模拟 | 对齐会话标识宽度与开发计划，完成真实 UART 联调 |
| M5 实屏地图和动画 | 地图投影、弧线、光点、待机和 HTML/SVG 预览 | 确定屏幕与库，接入实屏驱动，验证正式城市落点、帧率、刷新阻塞和内存 |
| M6 双板联调和资源导入 | 统一同步规则及测试接口已有 | 正式 8 城、16 条在线源、8 份离线音频的导入与逐源验证，两板实际联动 |
| M7 稳定性和交付 | 主机测试、配置工具、交付说明、证据目录和发布检查已有 | 2 小时实机运行、200 次混合操作、独立构建上传、固件产物和最终验收 |

**主机测试中的“两小时运行”通过模拟时钟推进场景，不代表目标设备已连续运行两小时。** partner 确认的既有单城出声成果与本仓库集成状态分别记录：该成果已完成，但完整源码尚未收到并接入本仓库。

## 目录结构

```text
.
├── README.md                         仓库入口、进度与接手说明
├── demo                              保留的 Wi-Fi / HTTP 诊断程序
├── output/
│   ├── 短波穿越电台软件开发计划.md    v1.2 开发计划与实施记录
│   └── 短波穿越电台软件开发计划.docx  同版本 Word 文档
└── radio-project/
    ├── README.md                     子工程简要入口
    ├── firmware/
    │   ├── shared/                   类型、计时、身份、输入、城市、协议和控制器
    │   │   └── generated/            由正式城市配置生成的公共头文件
    │   ├── radio_a/                  音频适配、Wi-Fi、SD 和板 A 链路
    │   └── display_b/                显示控制器和板 B 链路
    ├── config/                       正式城市表、环境记录、IO 和 Wi-Fi 模板
    ├── assets/
    │   ├── audio/                    离线音频接收目录，目前仅有说明
    │   ├── fonts/                    字体资源接收目录，目前仅有说明
    │   └── maps/                     地图资源说明，逻辑地图由代码绘制
    ├── scripts/                      配置生成、主机测试、预览、文档和发布检查
    ├── tests/
    │   ├── logic/                    Arduino 无关的 C++ 测试
    │   ├── config/                   Python 标准库 unittest
    │   └── fixtures/                 仅供测试和预览的合成 8 城数据
    ├── docs/                         接入、协议、缺项、验收和交付文档
    ├── evidence/                     原始测试日志、预览及审核报告
    └── release/                      发布说明与交付清单，目前无固件二进制
```

根目录的 `demo` 仅执行 Wi-Fi、HTTP 请求和最多 256 字节的响应读取，不含音频解码播放链路。历史文档生成临时文件位于被忽略的 `tmp/`，不属于运行工程。

## 已有实现

| 模块 | 位置 | 已实现的逻辑与当前限制 |
| --- | --- | --- |
| `InputService` | [输入模块](radio-project/firmware/shared/input_service.h) | 正交解码、卡点边沿数校准、按压消抖；真实电气行为待测 |
| `Identity` / `CityCatalog` | [身份](radio-project/firmware/shared/identity.h)、[城市目录](radio-project/firmware/shared/city_catalog.h) | session/request/attempt 隔离、稳定编号、显示顺序及字段校验；正式城市表未收到 |
| `AppController` / `SourceManager` | [控制器](radio-project/firmware/shared/app_controller.h)、[音源管理](radio-project/firmware/shared/source_manager.h) | 旋转优先、主备离线单向降级、同步出声、旧事件拒绝、待机和回绕处理 |
| `AudioAdapter` | [音频适配](radio-project/firmware/radio_a/audio_adapter.h) | 串行控制、旧回调隔离、PCM 进度、停止后的静默期；真实库后端仍为草案 |
| `WifiService` / `SdAudioCatalog` | [网络状态](radio-project/firmware/radio_a/wifi_service.h)、[SD 目录](radio-project/firmware/radio_a/sd_audio_catalog.h) | 状态处理、重连调度和文件检查接口；不代表真实联网、挂载与播放已验证 |
| 协议与双板链路 | [协议](radio-project/firmware/shared/protocol_v2.h)、[板 A](radio-project/firmware/radio_a/radio_link.h)、[板 B](radio-project/firmware/display_b/display_link.h) | 有界分帧、HELLO/ST/DONE/ACK、配置核对、重发与重启模拟 |
| `DisplayController` / `MapRenderer` | [显示控制器](radio-project/firmware/display_b/display_controller.h)、[地图逻辑](radio-project/firmware/shared/map_renderer.h) | 上海起点、投影、跨日期变更线、可取消弧线、待机和状态显示；无实屏刷新驱动 |
| 配置与交付脚本 | [脚本目录](radio-project/scripts/) | 城市数据生成、主机测试、预览、文档检查、构建前置检查和严格发布审核 |

### 已编码并通过主机测试的行为

- 旋转先处理，旧请求的 PCM、失败、EOF 和动画通知不能改变新请求；快速选择以最新城市为准。
- 主用明确失败立即转备用，备用失败转本地；确认断网时直接本地，跳过在线首播预算。
- 本地文件结束后循环；本地致命错误进入错误状态，避免无限重试。
- 通过有效 PCM 进度判定就绪与卡流，自然静音不以音量大小判失败。
- 等待动画时继续消费音频；主用、备用和本地共用同步出声条件，切源不重播动画。
- 重复快照不重启动画，旧会话和请求被拒绝；显示失联后有明确降级，重启后恢复状态。
- 自动切源、网络和显示事件不延长已开始播放后的待机期限。
- 本轮自查已补充待机锚点回绕、本地硬失败、旋钮校准及城市字段验证的回归用例。

### 时间参数

参数集中在 [`TimingConfig`](radio-project/firmware/shared/radio_types.h)，数值均为当前工程初值。

| 参数 | 初值 | 含义 |
| --- | --- | --- |
| `mainStartTimeoutMs` / `backupStartTimeoutMs` | 各 15,000 ms | 主用、备用独立首播预算；明确失败提前切换 |
| `localStartTimeoutMs` | 2,000 ms | 本地文件启动至就绪的预算 |
| `totalStartTimeoutMs` | 32,000 ms | 一次选城的首次总加载上限，不因切源重置 |
| `streamStallTimeoutMs` | 10,000 ms | 播放中无 PCM 进度的卡流判断窗口 |
| `idleTimeoutMs` | 20,000 ms | 正常活动状态的无操作待机窗口 |
| `pcmReadyWindowMs` | 200 ms | 持续采样进度达到音频就绪的窗口 |
| `arcDurationMs` | 1,500 ms | 逻辑弧线时长 |
| `displayDoneTimeoutMs` | 3,500 ms | 动画完成通知超时后的显示降级阈值 |
| `snapshotIntervalMs` | 1,000 ms | 状态快照重发间隔 |
| `doneRetryIntervalMs` / `doneMaxRetries` | 250 ms / 8 次 | DONE 重发间隔及次数 |

默认待机策略为 `AFTER_INITIAL_LOAD`：本次选城首次加载期间暂缓待机，仍受 32 秒总预算约束；首次出声或最终失败后获得完整 20 秒活动窗口。兼容策略 `LAST_USER_INPUT` 从最后一次有效操作起计时，可能在两级首播预算耗尽前取消加载。两种策略已有独立主机测试。

输入到静音的 **100 ms** 响应目标、超时 **250 ms** 以内的实测误差，以及实屏 **20–30 fps** 目标均待目标设备验证。

## 主机验证与预览

### 环境要求

- Python **3.10 或更高版本**；现有主机校验脚本使用 Python 标准库。
- 支持 **C++17** 的 `c++`、`g++` 或 `clang++`。测试脚本也支持通过 `CXX` 指定可执行文件。
- Git，用于查看版本和比较改动。主机测试不要求 Arduino CLI、联网电台或实际开发板。

先从仓库根目录进入子工程，以下主机验证、配置和构建命令均在 **`radio-project/` 目录内**执行：

```bash
cd radio-project
```

测试脚本会在临时目录生成合成城市头文件并编译测试程序，正式配置和生产城市头文件不会被 fixture 覆盖。

### C1：C++ 逻辑测试

```bash
python3 -B scripts/run_host_tests.py
```

当前结果为 `665 checks, 0 failures`。默认保存到 [host-tests.log](radio-project/evidence/host-tests.log)；只复验、不更新证据文件时可增加 `--no-evidence`。

### C2：配置和生成器测试

```bash
python3 -B -m unittest discover \
  -s tests/config -p 'test_*.py' -v
```

当前结果为 `17 tests, OK`，已有记录见 [config-tests.log](radio-project/evidence/config-tests.log)。单独运行上述 unittest 命令只打印结果，不自动更新该历史日志。

### C3：地图和待机预览

```bash
python3 -B scripts/simulate_display.py --self-test \
  --fixture tests/fixtures/cities.json \
  --output evidence/display-preview
```

当前自测通过。生成的 [index.html](radio-project/evidence/display-preview/index.html) 可直接在电脑浏览器打开；另有 [地图 SVG](radio-project/evidence/display-preview/map.svg)、[待机 SVG](radio-project/evidence/display-preview/standby.svg) 和 [预览数据](radio-project/evidence/display-preview/preview.json)。所有落点均来自合成 fixture，页面明确标注 `FIXTURE PREVIEW`，不代表正式 8 城验收。

### C4：计划文档一致性检查

```bash
python3 -B scripts/check_documents.py \
  --markdown ../output/短波穿越电台软件开发计划.md \
  --docx ../output/短波穿越电台软件开发计划.docx
```

当前脚本的 31 项检查通过，结果见 [document-check.json](radio-project/evidence/document-check.json)。已有 [21 页 PDF 渲染](radio-project/evidence/document-render/plan-v1.2.pdf)，页面抽查图保存在同目录。脚本仅覆盖已编码的检查项，不保证计划全文与代码逐字段一致；已发现的协议差异见下文。

### C5：严格发布审核

```bash
python3 -B scripts/audit_delivery.py --strict \
  --output evidence/acceptance-report.json \
  --manifest release/manifest.json
```

**当前预期退出码为 2**，报告为 `status: blocked`、`release_ready: false`。原因包括正式资源、播放器、硬件、工具链和实测证据缺失。该退出码表示发布前置条件未满足，不能作为发布通过处理。

审核会更新 [验收报告](radio-project/evidence/acceptance-report.json) 和 [交付清单](radio-project/release/manifest.json)。当前清单的固件二进制列表为空；清单文件本身不是固件发布包。

## 配置与硬件构建

### 正式城市数据与测试数据

| 文件 | 用途 | 当前状态 |
| --- | --- | --- |
| [config/cities.json](radio-project/config/cities.json) | 正式城市配置的唯一编辑源 | `complete: false`，`cities` 为空，等待内容组数据 |
| [tests/fixtures/cities.json](radio-project/tests/fixtures/cities.json) | 合成 8 城，仅用于测试与预览 | 已通过结构及字段校验，不是实际台址清单 |
| [generated/city_data.h](radio-project/firmware/shared/generated/city_data.h) | 正式固件使用的生成结果 | 当前为不完整配置、0 城，不包含 fixture |

正式数据需要稳定 `id` / `code`、显示顺序、城市名、坐标、主备 URL 和台名、离线路径与标签、时区规则及预留文案。编号与显示顺序分别管理，详细校验见 [配置校验实现](radio-project/scripts/radio_config.py)。时区和文案字段为配置保留项，本轮不实现当地时间或内容页。

检查正式数据：

```bash
python3 -B scripts/check_config.py \
  --input config/cities.json --require-complete
```

当前预期退出码为 2。收到完整真实数据并使严格校验通过后，才生成正式头文件：

```bash
python3 -B scripts/generate_city_data.py \
  --input config/cities.json \
  --output firmware/shared/generated/city_data.h \
  --print-config-id
```

目前直接生成会拒绝不完整配置。不要用 fixture 覆盖正式配置、伪填 8 个城市或通过放宽检查消除发布阻塞。

### 本地 IO、网络与资源

- [hardware_pins.example.h](radio-project/config/hardware_pins.example.h) 只列配置项名称，实际 GPIO 继续留空；硬件确认后建立 `radio-project/config/hardware_pins.local.h`，补齐所需宏和对应板的启用标记。
- [secrets.example.h](radio-project/config/secrets.example.h) 只列 Wi-Fi 配置模板；板 A 的真实配置放入 `radio-project/config/secrets.local.h`。
- 两个 `.local.h` 文件已被子工程 `.gitignore` 排除。凭据只保存在开发机，不写入共享源码、协议报文或测试日志。
- 离线资源放入 [assets/audio/](radio-project/assets/audio/)，按城市代码对应 SD 路径，并补齐来源和资源清单。计划初值为 MP3、44.1 kHz、128 kbps；当前没有实际 MP3 文件。
- 字体、屏幕与地图资源按 [显示接入说明](radio-project/docs/display-integration.md) 接收；当前没有实屏字库或已确认的刷新库。

### 硬件环境与构建前置检查

目标环境记录在 [environment.lock.md](radio-project/config/environment.lock.md)：两板 ESP32-S3、Arduino-ESP32 3.3.12、ESP32-audioI2S 4.0.1。这些是待接收与实际构建核验的版本，不代表依赖已经安装并验证。

基础 FQBN 为 `esp32:esp32:esp32s3`。真实构建还需按实际模组确认 Flash、PSRAM 模式、分区、USB 及屏幕库；WROVER 的引脚和分区记录不能直接充当 S3 配置。

```bash
python3 -B scripts/build_firmware.py --board a --dry-run
python3 -B scripts/build_firmware.py --board b --dry-run
```

当前两板预期都退出 2。脚本会列出正式配置、IO 宏、板 A Wi-Fi、Arduino CLI 以及 sketch 入口的缺项；即使使用 `--dry-run`，也需要通过这些前置检查。

`radio_a.ino` / `display_b.ino` 尚未集成，真实后端与实屏驱动尚未完成；只安装工具链或复制配置模板还不能生成固件。资料和代码接入齐备后，再按 [构建与上传说明](radio-project/docs/build-and-upload.md) 提供完整 FQBN、执行构建、上传并留存产物与 SHA256。

## 阻塞项与接续顺序

| 优先顺序 | 需要补齐的输入或工作 | 完成标志 |
| --- | --- | --- |
| 1 | partner 的完整单城播放器、构建脚本、库副本及校验值 | 复用原工程完成 M0 短时冒烟，记录 PSRAM、I2S 设置与 API 耗时 |
| 2 | 正式 8 城配置、16 条主备地址；巴黎和开罗候选源重新验证 | 配置严格校验通过，每条源有网络、板型、版本、编码及播放记录 |
| 3 | 8 份离线 MP3、来源说明、文件路径和资源清单 | 主用每城至少 20 分钟、备用至少 5 分钟、本地至少两次完整循环 |
| 4 | 两板具体 S3 模组、Flash/PSRAM、分区、USB、屏幕及 IO | 硬件参数与配置全部确认，不使用默认引脚 |
| 5 | 对齐协议会话宽度，完成独立代码审查；接入真实音频、实屏驱动与 Arduino 入口 | 文档、代码、两板协议与依赖接口一致，关键阻塞和取消边界经过验证 |
| 6 | 安装并锁定工具链，完成 S3 编译和上传 | 有真实编译日志、依赖校验、产物大小、`.bin` 和 SHA256 |
| 7 | 执行双板与最终装配的硬件验收 | T01–T22 通过，完成 2 小时活动模式、200 次混合操作及独立构建上传回归 |
| 8 | 补齐证据并重新执行严格发布审核 | 报告真实反映所有前置条件已满足，才能确认 P0 完成并发布 |

单源测试还需记录首播时间、最终 URL、编码和现场网络条件，并在展示场地复测。SD、MAX98357、UART、屏幕、供电、真实静音响应和看门狗行为均需目标设备验证。完整依赖及解除条件见 [缺失输入清单](radio-project/docs/missing-inputs.md)，操作和结果登记见 [硬件验收矩阵](radio-project/docs/hardware-test-matrix.md)。

## 已知限制与待审查事项

1. **协议会话标识宽度与计划存在差异。** [开发计划第 7 节](output/短波穿越电台软件开发计划.md) 要求 `boot` 为 16 位十六进制文本；当前 [协议说明](radio-project/docs/protocol-v2.md) 与 [C++ 编解码实现](radio-project/firmware/shared/protocol_v2.cpp) 使用 4 位十六进制、`uint16_t` 会话标识。正式双板联调前需要统一计划、两板实现及相关测试。现有主机测试通过不表示该差异已解决。
2. **真实音频后端仍为草案。** [audio_backend_esp32.cpp](radio-project/firmware/radio_a/audio_backend_esp32.cpp) 中的 Arduino 代码位于 `ARDUINO` / `ESP32` 条件编译分支。主机测试不会编译或运行该分支，采样钩子签名、库接口、静音行为、错误/结束回调及最长阻塞仍待核对。
3. **正式内容与显示硬件尚未到位。** 正式城市表为 0 城，音频资源为 0 份；预览中的 8 城是合成数据，逻辑画布不能替代实屏落点、分辨率、色深、扫描方式和刷新验证。
4. **测试覆盖与验收范围不同。** C1–C4 是当前主机检查与文档检查，不能替代独立代码审查、S3 编译或 T01–T22 实机测试。C5 依据当前文件和证据进行审核，最终仍需核对原始日志、真实设备与逐源播放记录。
5. **部分报告保留生成时的 Git 描述。** 子目录实施报告中“工作区未提交”等内容属于当时的记录；实现代码与更新计划目前已保存在 `45dc1b0`，后续状态以 `git log` 和 `git status` 为准。

## 文档与证据索引

| 需要了解的内容 | 入口 |
| --- | --- |
| 产品范围、规则、时间参数及验收要求 | [Markdown 开发计划](output/短波穿越电台软件开发计划.md)、[Word 版本](output/短波穿越电台软件开发计划.docx) |
| 已接收材料及 M0–M7 状态 | [材料与里程碑](radio-project/docs/materials.md) |
| 完成内容、验证结果和限制 | [交付报告](radio-project/docs/delivery-report.md) |
| 缺项、影响及解除条件 | [缺失输入](radio-project/docs/missing-inputs.md) |
| 音频所有权、PCM、代次与阻塞测量 | [音频接入](radio-project/docs/audio-integration.md)、[播放器接收状态](radio-project/docs/player-integration-status.md) |
| 报文、握手、重发与 v1 迁移 | [协议 v2](radio-project/docs/protocol-v2.md) |
| 地图逻辑、预览及实屏前提 | [显示接入](radio-project/docs/display-integration.md) |
| 工具链、构建前置检查和上传 | [构建与上传](radio-project/docs/build-and-upload.md) |
| T01–T22 操作与结果表 | [硬件验收矩阵](radio-project/docs/hardware-test-matrix.md) |
| 接手顺序及交接模板 | [继续开发](radio-project/docs/continued-development.md) |
| C++／配置原始日志 | [host-tests.log](radio-project/evidence/host-tests.log)、[config-tests.log](radio-project/evidence/config-tests.log) |
| 文档与发布检查结果 | [文档检查](radio-project/evidence/document-check.json)、[验收报告](radio-project/evidence/acceptance-report.json)、[发布清单](radio-project/release/manifest.json) |

## 版本与回退

| 提交 | 保存内容 |
| --- | --- |
| `34dc4e0` | 原始诊断 demo 与 v1.1 开发计划的本地基线 |
| `45dc1b0` | 当前主机逻辑工程、测试、预览、交付文档及 v1.2 开发计划 |

可通过以下命令核对版本与改动：

```bash
git log --oneline -5
git status --short
git diff --stat
```

前期全量文件副本和 Git bundle 保存在仓库外，未随项目分发。需要恢复时先核对本地副本与目标提交；基线仅包含当时的 demo 和计划，当前没有已验证的旧固件可用作硬件回退包。
