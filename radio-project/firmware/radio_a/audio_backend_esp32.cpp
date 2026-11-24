#include "audio_backend_esp32.h"

#if defined(ARDUINO) && defined(ESP32)

#include <atomic>

namespace radio {
namespace {
std::atomic<uint32_t> g_pcmProgress{0};
std::atomic<int> g_failure{0};
std::atomic<bool> g_ended{false};
}  // namespace

// ESP32-audioI2S calls this hook before gain is applied, so zero-amplitude
// programme silence still advances the progress counter. The exact signature
// must be checked against the received 4.0.1 source in M0.
void audio_process_raw_samples(const int16_t* sampleBuffer, int sampleCount,
                               int channels, int samplesPerSecond) {
  (void)sampleBuffer;
  (void)channels;
  (void)samplesPerSecond;
  if (sampleCount > 0) {
    g_pcmProgress.fetch_add(static_cast<uint32_t>(sampleCount),
                            std::memory_order_relaxed);
  }
}

void Esp32AudioBackend::begin(uint8_t bclk, uint8_t lrclk, uint8_t dout) {
  audio_.setPinout(bclk, lrclk, dout);
  audio_.setVolume(volume_);
  audio_.setConnectionTimeout(
      static_cast<uint16_t>(TimingConfig{}.mainStartTimeoutMs));
}

void Esp32AudioBackend::startOnline(const char* url) {
  g_failure.store(0);
  g_ended.store(false);
  audio_.connecttohost(url);
}

void Esp32AudioBackend::startFile(const char* path) {
  g_failure.store(0);
  g_ended.store(false);
  audio_.connecttoFS(SD, path);
}

void Esp32AudioBackend::stop() { audio_.stopSong(); }

void Esp32AudioBackend::setMuted(bool muted) {
  // Output gating keeps the decoder and PCM hook running while muted.
  audio_.setVolume(muted ? 0 : volume_);
}

bool Esp32AudioBackend::isStopped() { return !audio_.isRunning(); }

uint32_t Esp32AudioBackend::pcmProgressCount() {
  return g_pcmProgress.load(std::memory_order_relaxed);
}

bool Esp32AudioBackend::takeFailure(FailureKind& kind) {
  const int value = g_failure.exchange(0);
  if (value == 0) return false;
  kind = static_cast<FailureKind>(value);
  return true;
}

bool Esp32AudioBackend::takeEnded() { return g_ended.exchange(false); }

void Esp32AudioBackend::loop() { audio_.loop(); }

}  // namespace radio

#endif
