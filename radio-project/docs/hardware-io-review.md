# IO 清单与接入审阅（2026-11-25）

本次输入来自用户给出的双 ESP32-S3 接线清单。`ESP32S3_1` 对应板 A，负责音频、旋钮、联网与选城；`ESP32S3_2` 对应板 B，负责地图与屏幕。两板各有独立 TF 卡与原生 USB。

## 审阅结论

已收到的外设分配在各自板内无重复 GPIO，USB D−/D+ 的 GPIO19/20 分配正确。可以录入工程，但尚不满足完整硬件构建条件：

| 项目 | 结论与处理 |
| --- | --- |
| 双板 UART | 清单没有两板各自的 TX/RX；现有 `RadioLink` / `DisplayLink` 仍依赖板间传输。保留未定义，不推断接线；实接时 A TX→B RX、B TX→A RX，并共地 |
| 板 B GPIO35/36/37 | 分别用于屏幕 D/B/B2。八线 Flash 或 PSRAM 会占用 GPIO33–37，必须依据实际模组核对。如果被占用，就要修改硬件接线及配置，不能靠软件关闭 PSRAM 解除内部连接 |
| 板 B GPIO45 | 用作屏幕行地址 A，同时是 VDD_SPI 启动配置脚。检查屏幕/电平转换电路在复位采样期间的负载和电平；不由软件写输出代替复位电气核对 |
| BOOT / EN | 按用户清单记录按钮下拉到地、上拉 3V3、100 nF 对地。BOOT=GPIO0；EN/CHIP_PU 是专用复位输入，不是 GPIO，不创建虚构的 `RADIO_PIN_RESET` |
| EN 的 100 nF | 只代表已接收的电路参数。上拉阻值未提供，不能确认上电复位时序。乐鑫通常建议 EN 采用 10 kΩ + 1 μF，并根据电源上升过程调整；保留用户接线，待实物测量确认 |
| 屏幕接口与型号 | HUB75 信号集合已接收；面板分辨率、扫描方式、色深、物理针序、电平转换与显示库仍未确定，不能据此宣称屏幕驱动已完成 |
| 两份 TF 卡 | GPIO10–13 分别属于两颗 MCU，不是同一资源冲突。板 B 的 SD 信号现已纳入必填检查；固件挂载与屏幕资源加载尚未实现 |

GPIO 范围、存储器复用与 USB 依据：[乐鑫 ESP32-S3 GPIO 文档](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html)。GPIO45 电压控制、USB D−/D+ 及 EN 时序依据：[乐鑫硬件设计检查表](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html)。

## 已接收接线

以下头文件是可提交的共享接线记录；未收到的 UART 不填写。它们取代 v1.2 历史计划中“物理 IO 全部留空”的当前状态描述，原 Markdown/Word 计划保留为历史基线。

### ESP32S3_1 / 板 A

配置：[hardware_pins_a.h](../config/hardware_pins_a.h)。

| 外设 | 信号 | GPIO / 接点 | 固件宏 |
| --- | --- | --- | --- |
| MAX98357 | I2S_BCLK | 4 | `RADIO_PIN_AUDIO_BCLK` |
| MAX98357 | I2S_LRCLK | 5 | `RADIO_PIN_AUDIO_LRCLK` |
| MAX98357 | I2S_DAT → 模块 DIN | 6 | `RADIO_PIN_AUDIO_DOUT` |
| EC11 | A | 7 | `RADIO_PIN_ENCODER_A` |
| EC11 | B | 8 | `RADIO_PIN_ENCODER_B` |
| EC11 | SW | 9 | `RADIO_PIN_ENCODER_SW` |
| TF1 | CS | 10 | `RADIO_PIN_SD_CS` |
| TF1 | MOSI | 11 | `RADIO_PIN_SD_MOSI` |
| TF1 | SCLK | 12 | `RADIO_PIN_SD_SCK` |
| TF1 | MISO | 13 | `RADIO_PIN_SD_MISO` |
| USB1 | DM | 19 | `RADIO_PIN_USB_DM` |
| USB1 | DP | 20 | `RADIO_PIN_USB_DP` |
| BOOT | 按键 | 0 | `RADIO_PIN_BOOT` |
| RESET | 按键 | EN/CHIP_PU | 专用输入，无 GPIO 宏 |
| 板间 UART | TX / RX | 未提供 | `RADIO_PIN_LINK_TX` / `RADIO_PIN_LINK_RX` 未定义 |

### ESP32S3_2 / 板 B

配置：[hardware_pins_b.h](../config/hardware_pins_b.h)。

| 外设 | 信号 | GPIO / 接点 | 固件宏 |
| --- | --- | --- | --- |
| TF2 | CS | 10 | `RADIO_PIN_SD_CS` |
| TF2 | MOSI | 11 | `RADIO_PIN_SD_MOSI` |
| TF2 | SCLK | 12 | `RADIO_PIN_SD_SCK` |
| TF2 | MISO | 13 | `RADIO_PIN_SD_MISO` |
| HUB75 | R1 | 42 | `RADIO_PIN_PANEL_R1` |
| HUB75 | G1 | 41 | `RADIO_PIN_PANEL_G1` |
| HUB75 | B1 | 40 | `RADIO_PIN_PANEL_B1` |
| HUB75 | R2 | 38 | `RADIO_PIN_PANEL_R2` |
| HUB75 | G2 | 39 | `RADIO_PIN_PANEL_G2` |
| HUB75 | B2 | 37 | `RADIO_PIN_PANEL_B2` |
| HUB75 | E | 21 | `RADIO_PIN_PANEL_E` |
| HUB75 | A | 45 | `RADIO_PIN_PANEL_A` |
| HUB75 | B | 36 | `RADIO_PIN_PANEL_B` |
| HUB75 | C | 48 | `RADIO_PIN_PANEL_C` |
| HUB75 | D | 35 | `RADIO_PIN_PANEL_D` |
| HUB75 | CLK | 2 | `RADIO_PIN_PANEL_CLK` |
| HUB75 | LAT | 47 | `RADIO_PIN_PANEL_LAT` |
| HUB75 | OE | 14 | `RADIO_PIN_PANEL_OE` |
| USB2 | DM | 19 | `RADIO_PIN_USB_DM` |
| USB2 | DP | 20 | `RADIO_PIN_USB_DP` |
| BOOT | 按键 | 0 | `RADIO_PIN_BOOT` |
| RESET | 按键 | EN/CHIP_PU | 专用输入，无 GPIO 宏 |
| 板间 UART | TX / RX | 未提供 | `RADIO_PIN_LINK_TX` / `RADIO_PIN_LINK_RX` 未定义 |

## 配置与校验

在 `radio-project/` 目录执行：

```bash
# 仅检查收到的分配：当前两板都应通过，不代表硬件已就绪。
python3 -B scripts/check_hardware.py --board a
python3 -B scripts/check_hardware.py --board b

# 完整 IO 门：当前两板应退出 2，列出 UART / 配置标记 / 模组核对缺项。
python3 -B scripts/check_hardware.py --board a --require-complete
python3 -B scripts/check_hardware.py --board b --require-complete
```

每块板默认选用 `config/hardware_pins_a.h` 或 `hardware_pins_b.h`。需要本机试接时，分别复制为 `hardware_pins_a.local.h` / `hardware_pins_b.local.h`，每份必须是完整的单板配置。它们已被 Git 忽略；最终确定的共享接线应更新回对应的受版本管理头文件。

不再自动使用旧的单文件 `hardware_pins.local.h`；将旧配置分别迁移到两份完整文件。校验只接受 `#pragma once`、十进制整数 `#define` 与注释；拒绝条件编译、包含、重复定义、续行、表达式与宏别名，防止检查值和编译值不一致。

补齐两板各自的 UART；核对实际模组、Flash/PSRAM 与电气连接后，才将板 A 的 `RADIO_HARDWARE_CONFIGURED` 或板 B 的 `RADIO_DISPLAY_CONFIGURED` 设为 1。使用 GPIO33–37 时必须记录 `RADIO_OCTAL_MEMORY_PRESENT`：0 表示已核实无八线存储器占用，1 表示有占用并必须重新分配冲突引脚；GPIO45 使用前须记录 `RADIO_GPIO45_STRAP_VERIFIED 1`。这些标记是核对结果的记录，不会替代测量或改变硬件。

`check_hardware.py`、`build_firmware.py` 可通过 `--pins <完整头文件>` 显式选择；发布审核使用对应的 `--pins-a` / `--pins-b`。构建和审核共用 `hardware_config.py`，两板必填项都包含各自 SD；构建以 `-include` 将实际被校验的同一文件传给 C++ 编译器，并设置 `RADIO_BOARD_A=1` 或 `RADIO_BOARD_B=1`。

## 验证与交付边界

新增回归覆盖原始接线、板别选择、板 B SD 缺项、同板重脚、USB/BOOT 固定占用、不存在的 GPIO22–25、Flash 总线占用、八线存储器冲突、GPIO45、注释伪装启用标记、含空格路径的构建参数和发布门。

本轮只完成 IO 审阅、配置落地和检查链路。真实播放器、Arduino 入口、正式 8 城与资源、完整 FQBN、显示驱动和工具链仍需补齐；未编译 S3 固件、未生成 `.bin`，T01–T22 保持 `BLOCKED`。
