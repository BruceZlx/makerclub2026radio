# 既有播放器接入状态

版本：v1.2 实施记录  
日期：2026-11-24

## 当前状态

`NOT RECEIVED`。partner 已确认单城市电台闭环完成，但完整播放器源码、构建脚本、依赖副本和校验值尚未在当前工作区交付。诊断 `demo` 只包含 Wi-Fi、HTTP 请求和最多 256 字节读取，不能作为播放器，也不得据此重写播放器。

## 已建立的接入边界

- `AudioBackend` 是真实播放库的唯一硬件边界。
- `AudioAdapter` 是 `AudioBackend` 的唯一所有者，串行执行 start/stop/mute，并用代次与静默期隔离旧回调。
- `AppController` 只通过 `AudioAdapterInterface` 发起请求和接收事件，不直接接触播放库。
- `Esp32AudioBackend` 仅作为待核验的 4.0.1 适配草案，未在真实构建中编译，不能标记为可播放实现。
- 主机测试使用 `FakeBackend` 验证取消、静音、PCM 就绪、失败分类和旧回调隔离；mock 不是可播放实现。

## 接收后必须核对

1. `Audio.h` / `Audio.cpp` 的实际版本、提交或压缩包 SHA256。
2. `connecttohost()`、`connecttoFS()`、`stopSong()`、`setVolume()`/`setMute()`、`loop()` 的真实签名与阻塞行为。
3. `audio_process_raw_samples()` 钩子是否在增益处理前调用，能否在静音时持续计数。
4. TLS、DNS、重定向、SD 打开、停止与取消的最长阻塞时间。
5. MAX98357 的 BCLK/LRCLK/DOUT 实际时序、声道与既有配置。

## 禁止事项

- 不从诊断 demo 重写播放器。
- 不把 mock 或接口实现写成“真实播放适配完成”。
- 不在未收到库副本前猜测 API 或校验值。
- 不在 IO 未确认时用默认 GPIO 构建。
