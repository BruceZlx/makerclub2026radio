# 继续开发说明

版本：v1.2 实施记录  
日期：2026-11-24

## 1 接手顺序

1. 阅读本文件、`docs/delivery-report.md`、`docs/missing-inputs.md` 与 `docs/hardware-test-matrix.md`。
2. 确认基线提交 `34dc4e022780cb75b48db99211a10f1e8ae60a66` 可回退，demo 未修改。
3. 接收完整播放器源码、依赖副本与校验值，先做 M0 短时冒烟，不从 demo 重写播放器。
4. 接收完整 8 城表、16 条真实源、8 份离线音频与来源记录。
5. 确认 S3 模组、Flash/PSRAM、分区、USB、屏幕与全部 IO，再填写 `config/hardware_pins.local.h`。
6. 安装 Arduino-ESP32 3.3.12 与 ESP32-audioI2S 4.0.1，保存校验值并执行 S3 编译。
7. 按 T01–T22 逐项实测，最后再运行 `audit_delivery.py --strict`。

## 2 最小下一步

| 顺序 | 任务 | 依赖 | 完成标志 |
| --- | --- | --- | --- |
| 1 | 接收并核对播放器 | partner 源码/依赖 | M0 冒烟日志、SHA256、PSRAM 与 I2S 设置 |
| 2 | 补齐正式配置 | 内容组 8 城表/16 源 | `cities.json` complete=true，生成头文件，严格校验通过 |
| 3 | 补齐离线资源 | 8 份 MP3 与来源 | `assets/audio/manifest.json` 8 条记录，循环测试通过 |
| 4 | 确认硬件 | 硬件组 | 模组/Flash/PSRAM/分区/USB/屏幕/IO 全部确认 |
| 5 | 接入真实播放器 | 播放器、IO | AudioAdapter 真实后端编译并出声 |
| 6 | 实屏与双板 | 屏幕、串口 | 20–30 fps、内存稳定、双板重启恢复 |
| 7 | 完整验收 | 实机、网络、电源 | T01–T22、2 小时/200 次、独立构建上传 |

## 3 禁止事项

- 不修改 demo、tmp 历史材料、外部备份或 Git 基线。
- 不创建分支、不提交、不推送、不修改 Git 索引。
- 不猜测城市、台址、GPIO、Flash/PSRAM、分区或屏幕参数。
- 不用音调、mock、静音帧或预览冒充 8 份内容资源或实机验收。
- 不把主机通过、S3 编译通过或打包成功写成全部 P0 完成。
- 不在缺项时调整 15 s/10 s/32 s/20 s/100 ms/250 ms 验收指标。

## 4 交接记录模板

```text
计划版本与审阅结论：v1.2；R2 默认 AFTER_INITIAL_LOAD，保留 LAST_USER_INPUT；R3 协议 v2；均为本次用户选择，不追认为 partner 既有批准。
当前工程位置和源码版本：/Users/bruce/Documents/club/radio-project；基线 34dc4e0；工作区未提交。
本次完成的里程碑和文件：P1–P6 逻辑、P8 主机部分；见 docs/delivery-report.md。
实际构建命令及依赖版本：未执行 S3 编译；arduino-cli 未安装。
已通过的测试与证据位置：C1–C4；evidence/。
尚未实机验证的内容：T01–T22、真实播放、PSRAM、MAX98357、SD、UART、屏幕。
已知问题和触发条件：缺完整播放器、8 城表/16 源、8 音频、板屏与 IO。
IO 确认状态：全部未确认；hardware_pins.local.h 未创建。
下一步最小任务及依赖：接收播放器与正式数据，完成 M0/M6。
```
