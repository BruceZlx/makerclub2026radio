# Firmware 模块说明

当前目录包含 Arduino 无关的 C++ 逻辑模块与真实硬件适配草案。P7 硬件集成尚未开始，因此没有可直接编译上传的 `.ino` 入口，也没有任何 `.bin`。

## 模块

- `shared/`：类型、计时参数、身份、输入、城市、地图、协议 v2、控制器、音源管理。
- `radio_a/`：AudioAdapter 单所有者、RadioLink、Wi-Fi 状态、SD 音频目录、ESP32-audioI2S 4.0.1 适配草案。
- `display_b/`：DisplayController、DisplayLink。

## 集成前提

1. 接收并核对 partner 完整播放器源码与依赖副本。
2. 确认 ESP32-S3 模组、Flash/PSRAM、分区、USB、屏幕和全部 IO。
3. 已接收的外设接线在 `config/hardware_pins_a.h` / `hardware_pins_b.h`；补齐 UART、完成模组与电气核对后启用配置，见 `docs/hardware-io-review.md`。本机可使用对应完整 `.local.h`；板 A 另需 `config/secrets.local.h`。
4. 安装 Arduino-ESP32 3.3.12 与 ESP32-audioI2S 4.0.1，记录校验值。
5. 再创建板 A/板 B 的 Arduino 入口并执行真实构建。

在上述条件满足前，`scripts/build_firmware.py` 会明确失败，不会采用默认 GPIO。
