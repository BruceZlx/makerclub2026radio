# 音频适配说明

版本：v1.2 实施记录  
日期：2026-11-24

## 1 所有权与边界

- `AudioAdapter` 是 `AudioBackend` 的唯一所有者。所有 start、stop、mute 只经由适配器串行执行。
- `AppController` 通过 `AudioAdapterInterface` 发起 `AudioStartRequest`，不直接调用播放库。
- 采样回调只允许推进原子计数或写入有界事件；不得打印长日志、操作文件、联网或切源。
- 禁止强删任务或并发控制同一个 `Audio` 对象。

## 2 代次与旧回调隔离

每次 start 携带 `session/request/attempt/city`。适配器在 stop 后进入静默期，只有后端已停止且回调安静达到 `quiescenceMs`（默认 50 ms）后才应用下一次 start。旧 PCM、失败和 EOF 事件在 stop 后被排空并丢弃，不能给旧 PCM 贴新请求编号。

## 3 PCM 就绪与健康

- 以音量处理前的原始 PCM 采样进度作为健康依据，不使用 `isRunning()`、HTTP 200 或收到字节单独判定。
- 首个 PCM 进度开始计时，持续约 `pcmReadyWindowMs`（默认 200 ms）后才标记 `READY_MUTED`。
- 静音只做输出门控，解码与采样消费继续；自然静音节目不判失败。
- `streamStallTimeoutMs`（默认 10000 ms）内没有 PCM 进度才按断流处理。

## 4 失败分类

`FailureKind` 区分连接拒绝、无效 URL、HTTP 状态、不支持编码、解码致命错误、断流、文件结束、SD 挂载失败、文件缺失、文件损坏和内部错误。明确失败立即切下一源；只有“无明确错误但一直无法形成可播放 PCM”才使用首播预算。

## 5 主机验证与真实测量

主机测试覆盖：

- start/stop/mute 串行与静默期；
- 旧请求/旧 attempt 的 PCM、失败、EOF 不影响新请求；
- 静音期间 PCM 进度仍推进；
- 自然静音不触发切源；
- 10 秒无 PCM 进度触发切源；
- 本地文件结束循环。

真实测量尚未执行。接收既有播放器后必须记录 `connecttohost()`、`stopSong()`、TLS、DNS、SD 打开的最长阻塞与取消完成时间；若超过响应目标，应在适配器内采用受控工作任务或小范围适配，并重新验证超时、取消和看门狗。不得用调宽超时掩盖阻塞。

## 6 当前结论

接口、故障模拟和接入说明已完成；真实播放适配未完成，不能把 mock 标为可播放实现。
