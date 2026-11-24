# 环境锁定（v1.2）

核对日期：2026-11-24  
基线提交：`34dc4e022780cb75b48db99211a10f1e8ae60a66`

| 项目 | 目标/记录 | 实际状态 |
| --- | --- | --- |
| 板 A | ESP32-S3 | 目标确定；具体模组未确认 |
| 板 B | ESP32-S3 | 目标确定；具体模组未确认 |
| Arduino-ESP32 | 3.3.12 | 待实际构建核验；本机未安装 |
| ESP32-audioI2S | 4.0.1 | 待实际构建核验；未收到库副本 |
| 基础 FQBN | `esp32:esp32:esp32s3` | 仅基础板型；完整参数未确定 |
| Flash 容量 | 未确认 | 禁止默认 |
| PSRAM | QSPI/OPI 未确认 | 禁止默认 |
| 分区表 | 未确认 | 禁止默认 |
| USB CDC / USB OTG | 未确认 | 禁止默认 |
| 板 A 音频 | MAX98357 I2S | 模块已确认，IO 未填 |
| 板 B 屏幕 | 未确认 | 型号、扫描、库均未冻结 |
| Arduino CLI | 未安装 | `shutil.which("arduino-cli")` 无结果 |
| 依赖校验值 | 无文件可算 | 收到副本后记录 `library.properties` 与压缩包 SHA256 |

## 实际核对记录

```text
2026-11-24  arduino-cli: not found
2026-11-24  Arduino libraries: ~/Documents/Arduino/libraries 不存在
2026-11-24  Arduino15: ~/Library/Arduino15 不存在
2026-11-24  demo SHA256: ab90b0ffc6d48601e46f31190e4ed08d65141f9a65d08aed2c2d27c0f73e21e4
2026-11-24  config/cities.json SHA256: 4ba2c7a6b13ba804802542d70d025360624bbbc4c7ef6573f32f88d71d8dbbc2
2026-11-24  tests/fixtures/cities.json SHA256: 825a9bc22ac34ec0d90a8387101beac57e081ce6cc88db8531ffa86c55a2bea3
```

本文件不把交接文档中的 ESP32-WROVER、GPIO 或分区参数当作 S3 默认值。真实模组、Flash/PSRAM、分区和屏幕确认后，必须重新生成本文件并保存构建日志。IO 未填写时 `scripts/build_firmware.py` 必须明确失败。
