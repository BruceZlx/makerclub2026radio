# 板间协议 v2

版本：v1.2 实施记录  
日期：2026-11-24

## 1 物理与分帧

- 逻辑串口：115200、8N1。
- ASCII、冒号分隔、LF 结束；接收端兼容 CRLF。
- 单帧上限 160 字节（含换行）；超长帧丢弃并计数。
- 分包与粘包由有界缓冲重组；非法字段、越界编号、旧会话全部丢弃并计数。

## 2 帧格式

```text
HELLO:2:<board>:<boot>:<config>\n
ST:2:<session>:<request>:<city>:<view>:<anim>:<audio>:<source>\n
DONE:2:<session>:<request>:<city>\n
ACK:2:<session>:<request>:DONE\n
```

| 字段 | 约定 |
| --- | --- |
| board | `A` 或 `B` |
| boot | 本板开机标识，4 位十六进制；板 A 的 boot 同时作为 session |
| session | 板 A 当前 boot；板 B 原样回传 |
| request | 板 A 递增无符号 32 位请求编号 |
| city | 稳定城市编号 0–7，不随显示顺序重编号 |
| config | 公共城市/显示配置校验标识，8 位十六进制 |
| view | `STANDBY` / `MAP`；`INFO` 仅保留枚举，本轮不发送 |
| anim | `NONE` / `RUNNING` / `DONE` |
| audio | `IDLE` / `CONNECTING` / `BUFFERING` / `READY` / `PLAYING` / `ERROR` |
| source | `NONE` / `MAIN` / `BACKUP` / `LOCAL` |

## 3 握手与会话隔离

1. 双方开机发送 HELLO；未握手时每 1000 ms 重发。
2. 板 A 收到板 B HELLO 立即回复 HELLO，保证板 B 重启后能完成握手。
3. `config` 不一致时置 `configMismatch`，不按错误编号展示另一座城市。
4. 板 B 只在握手确认板 A 会话后接纳该 session 的 ST；旧 session 的 ST 丢弃。
5. 板 A 重启后生成新 session；旧 session 的 ST 和 DONE 不能影响新请求。
6. request 接近回绕时重新生成 session 并握手，不跨 session 比较普通整数大小。

## 4 状态快照与动画

- 板 A 在状态变化时发送 ST，并每 1000 ms 重发全状态快照。
- 新 request 取消旧动画；同一 request 的重复 ST 只更新状态，不重播弧线。
- 若快照 `anim=DONE`，板 B 直接恢复静态地图并返回 DONE；若 `anim=RUNNING`，从头播放当前弧线。
- 来源变化不重播动画。

## 5 DONE、ACK 与重发

- 板 B 动画完成后发送 `DONE:2:<session>:<request>:<city>`。
- 板 A 校验 session/request/city 后发送 `ACK:2:<session>:<request>:DONE`。
- 重复 DONE 仍回复 ACK，但只执行一次状态转移。
- 板 B 每 250 ms 重发 DONE，最多 8 次；收到匹配 ACK 后停止。
- 板 A 3500 ms 未收到有效 DONE 时重发快照并标记显示降级，允许已就绪音频出声，避免永久静音。

## 6 v1 迁移说明

v1 仅保留迁移对照，不与 v2 混用：

```text
A -> B  ST:<city>:<state>\n
B -> A  DONE:<city>\n
A -> B  BTN:<city>\n
```

v1 没有 session/request，无法识别“城市 1 → 城市 2 → 城市 1”的旧 DONE，也没有重启恢复和配置校验。若继续使用 v1，必须明确接受这些限制，不能宣称通过 T06、T14、T16、T17。

## 7 主机测试覆盖

- 分包、粘包、CRLF、超长帧、非法字段、错误枚举、越界 city。
- HELLO 握手、配置不一致、板 A/板 B 重启恢复。
- 同 request 重复快照不重播；旧 request 丢弃。
- DONE 重发 250 ms × 8、ACK 幂等、旧 session 拒绝。
- `audioName(Playing)` 与 `PLAYING` 往返一致。
