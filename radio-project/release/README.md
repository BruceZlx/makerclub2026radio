# Release 目录

当前状态：**BLOCKED / 不可发布**。

- 没有真实编译产物，不包含任何 `.bin`。
- `manifest.json` 由 `scripts/audit_delivery.py` 生成，记录四类事实、文件 SHA256 和阻塞项。
- 只有在 S3 真实编译成功、上传验证通过、T01–T22 全部通过后，才允许收录固件并更新清单。
- 不得用主机测试、HTML/SVG 预览或 mock 生成冒充可刷写固件。
