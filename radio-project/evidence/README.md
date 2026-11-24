# Evidence

| 文件 | 内容 |
| --- | --- |
| `host-tests.log` | C1 原始编译与 C++ 主机测试输出 |
| `config-tests.log` | C2 标准库 unittest 原始输出 |
| `display-preview/` | C3 生成的 HTML/SVG/JSON 预览 |
| `document-check.json` | C4 Markdown/Word v1.2 一致性结果 |
| `acceptance-report.json` | C5 严格发布审核报告（当前 blocked） |
| `strict-audit.log` | C5 严格审核原始输出（退出码 2，预期阻塞） |
| `build-gate.log` | 板 A/板 B 构建门 dry-run 原始输出（退出码 2，预期阻塞） |
| `document-render/` | Word 计划渲染检查用 PDF/页面图 |
| `source-records.json` | 待补：16 条真实源逐源实测记录（当前不存在） |

主机证据不代表 S3 编译或实机验收。当前没有 `.bin`、没有 T01–T22 实测日志。
