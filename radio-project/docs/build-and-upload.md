# 构建与上传说明

版本：v1.2 实施记录  
日期：2026-11-24

## 1 当前状态

`arduino-cli` 未安装，Arduino-ESP32 3.3.12 与 ESP32-audioI2S 4.0.1 依赖副本未接收，S3 具体模组/Flash/PSRAM/分区/USB 未确认，因此 **未执行任何 S3 编译，未生成任何 `.bin`**。

## 2 构建门

```bash
cd radio-project
python3 -B scripts/build_firmware.py --board a --dry-run
python3 -B scripts/build_firmware.py --board b --dry-run
```

脚本在调用编译器前检查：

1. `config/cities.json` 必须完整 8 城并通过严格校验；
2. `config/hardware_pins.local.h` 必须存在并定义该板全部 IO 宏；
3. 板 A 必须存在 `config/secrets.local.h`；
4. `arduino-cli` 必须可用；
5. 对应 Arduino sketch 入口必须已集成。

任一条件不满足即退出码 2，并明确指出缺失项；不提供默认 GPIO，不静默回退到 WROVER 引脚限制。

## 3 环境锁定

| 项目 | 目标 |
| --- | --- |
| 板 A / 板 B | ESP32-S3 |
| Arduino-ESP32 | 3.3.12 |
| ESP32-audioI2S | 4.0.1 |
| 基础 FQBN | `esp32:esp32:esp32s3` |
| 完整 FQBN | 由真实模组、Flash、PSRAM、分区、USB 决定 |
| 依赖校验 | 收到副本后记录 `library.properties` 与压缩包 SHA256 |

## 4 真实构建命令（条件满足后）

```bash
arduino-cli core install esp32:esp32@3.3.12
arduino-cli lib install "ESP32-audioI2S@4.0.1"   # 库名以实际依赖副本为准
python3 -B scripts/build_firmware.py --board a --fqbn <完整S3-FQBN>
python3 -B scripts/build_firmware.py --board b --fqbn <完整S3-FQBN>
```

`--fqbn` 不得只写基础板型；必须包含真实模组的 Flash、PSRAM、分区和 USB 参数。构建日志、产物大小、PSRAM 启动报告和 SHA256 必须保存到 `evidence/`。

## 5 上传与回退

```bash
arduino-cli upload --port <PORT> --fqbn <完整S3-FQBN> firmware/radio_a
arduino-cli upload --port <PORT> --fqbn <完整S3-FQBN> firmware/display_b
```

- 上传前记录端口、板型、FQBN、构建版本与 SHA256。
- 保留基线提交 `34dc4e022780cb75b48db99211a10f1e8ae60a66` 的 demo 与 v1.1 计划，可回退。
- 回退固件必须使用已验证的旧产物；当前没有可刷写固件，不能伪造回退包。
- 独立构建上传回归尚未执行；需另一名成员按本说明独立完成。
