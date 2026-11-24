# T01–T22 验收矩阵

版本：v1.2 实施记录  
日期：2026-11-24  
当前结果：**0 / 22 通过**。所有实机项均因缺完整播放器、8 城表/16 源、8 份离线音频、具体板屏与 IO 而 `BLOCKED`；主机逻辑覆盖不能替代实机验收。

| 编号 | 场景 | 前置 | 期望 | 实测 | 证据 | 结果 |
| --- | --- | --- | --- | --- | --- | --- |
| T01 | 原播放器基线 | 完整播放器、目标板、可播放源 | 连续出声至少 20 分钟，无异常重启 | 未接收播放器，未执行 | docs/player-integration-status.md | BLOCKED |
| T02 | PSRAM/硬件配置缺失 | S3 模组、Flash/PSRAM、完整 FQBN | 明确错误提示，不采用默认 IO | 构建门已实现；实机未执行 | scripts/build_firmware.py | BLOCKED |
| T03 | 旋钮正反转及抖动 | 旋钮实物、IO | 一卡点一城市，首尾循环，抖动不换城 | 主机逻辑覆盖 50 次正反转与抖动；实机未执行 | evidence/host-tests.log | BLOCKED |
| T04 | 动画先结束 | 双板、音频 | 等到当前音频就绪才出声 | 主机覆盖；实机未执行 | evidence/host-tests.log | BLOCKED |
| T05 | 音频先就绪 | 双板、音频 | 解码继续，动画结束前静音 | 主机覆盖；实机未执行 | evidence/host-tests.log | BLOCKED |
| T06 | 城市 1→2→1 | 旋钮、双板 | 旧 DONE/音频回调不能放行新请求 | 主机覆盖旧 session/request/attempt；实机未执行 | evidence/host-tests.log | BLOCKED |
| T07 | 20 次快速换城 | 旋钮、双板 | 最新城市胜出，旧音频停止，无连接堆积 | 主机覆盖 20 次排队旋转；实机未执行 | evidence/host-tests.log | BLOCKED |
| T08 | 主/备明确失败与无首帧 | 真实源 | 硬失败立即切下一源；无首帧各不超过 15 s 加调度误差 | 主机覆盖 15000/15000 ms；实机未执行 | evidence/host-tests.log | BLOCKED |
| T09 | 主备均不可用 | 真实源、SD | 转本地文件，来源显示正确 | 主机覆盖；实机未执行 | evidence/host-tests.log | BLOCKED |
| T10 | 选城无网/在线断网 | 网络、SD | 旋钮优先；确认断网直接 SD | 主机覆盖同周期旋转抢占；实机未执行 | evidence/host-tests.log | BLOCKED |
| T11 | 断流及自然静音 | 真实音频 | 持续无 PCM 切源；正常静音不误切 | 主机覆盖 10000 ms；实机未执行 | evidence/host-tests.log | BLOCKED |
| T12 | SD 缺失/缺文件/损坏 | SD 卡、音频 | 显示错误状态，无无限重试和崩溃 | 主机覆盖分类；实机未执行 | evidence/host-tests.log | BLOCKED |
| T13 | 本地文件结束 | SD 卡、音频 | 正常重新播放，不转错城市 | 主机覆盖循环；实机未执行 | evidence/host-tests.log | BLOCKED |
| T14 | DONE 丢失/重复/延迟 | 双板串口 | 重发和确认有效；只有当前请求可出声 | 主机覆盖 250 ms × 8 与 ACK；实机未执行 | evidence/host-tests.log | BLOCKED |
| T15 | 分包/粘包/超长帧/非法字段 | 串口 | 合法帧重组，非法帧拒绝，缓冲有上限 | 主机覆盖 160 字节边界；实机未执行 | evidence/host-tests.log | BLOCKED |
| T16 | 板 B 重启/显示失联 | 双板、串口 | 恢复全状态；按策略降级，无永久静音 | 主机覆盖重启与 3500 ms 降级；实机未执行 | evidence/host-tests.log | BLOCKED |
| T17 | 板 A 重启/配置不一致 | 双板、串口 | 新会话不接受旧帧；版本不一致有提示 | 主机覆盖；实机未执行 | evidence/host-tests.log | BLOCKED |
| T18 | 首次加载及播放中待机 | 旋钮、双板 | 默认 AFTER_INITIAL_LOAD；自动切源不续期；兼容策略独立 | 主机覆盖两种策略；实机未执行 | evidence/host-tests.log | BLOCKED |
| T19 | 活动模式稳定性 | 实机、电源、网络 | 至少 2 小时、200 次混合操作，无错台/死机/看门狗/内存下降 | 仅主机模拟 2 小时/200 次；实机未执行 | evidence/host-tests.log | BLOCKED |
| T20 | 同一旋钮按压与暂缓页面 | 旋钮 | 仅 SW 产生一次按压事件，不切源，不进入未实现页面 | 主机覆盖；实机未执行 | evidence/host-tests.log | BLOCKED |
| T21 | 失败/断网与旋转同时发生 | 旋钮、网络 | 先选新城，再对新城降级，旧城事件不误切 | 主机覆盖同周期优先级；实机未执行 | evidence/host-tests.log | BLOCKED |
| T22 | 无错误但主备均缓慢 | 真实源 | 各等 15 s 再切，总首播/错误不超过 32 s 加调度误差 | 主机覆盖 32000 ms；实机未执行 | evidence/host-tests.log | BLOCKED |

## 说明

- `BLOCKED` 表示缺少实机前置条件，不是通过，也不是失败。
- T03–T18、T20–T22 的逻辑分支已由主机测试覆盖，但仍需实机复验。
- T01、T02、T04、T05、T19 必须有真实播放器、目标板、音频/屏幕和供电条件。
- 任何一项未实测通过前，整体 P0 保持未完成。
