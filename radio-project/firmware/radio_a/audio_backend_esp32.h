#pragma once

#if defined(ARDUINO) && defined(ESP32)

#include <Audio.h>
#include <SD.h>

#include "audio_adapter.h"

namespace radio {

// Real ESP32-audioI2S 4.0.1 boundary. The library API and PCM hook signature
// must be verified against the received copy in M0 before this file is built.
class Esp32AudioBackend : public AudioBackend {
 public:
  void begin(uint8_t bclk, uint8_t lrclk, uint8_t dout);
  void startOnline(const char* url) override;
  void startFile(const char* path) override;
  void stop() override;
  void setMuted(bool muted) override;
  bool isStopped() override;
  uint32_t pcmProgressCount() override;
  bool takeFailure(FailureKind& kind) override;
  bool takeEnded() override;
  void loop() override;

 private:
  Audio audio_;
  uint8_t volume_ = 18;
};

}  // namespace radio

#endif
