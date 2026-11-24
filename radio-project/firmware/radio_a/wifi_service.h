#pragma once

#include <stdint.h>

namespace radio {

// Non-blocking Wi-Fi status tracker. It never decides the page or the source;
// AppController consumes the confirmed-lost event.
class WifiService {
 public:
  struct Config {
    uint32_t initialTimeoutMs = 15000;
    uint32_t reconnectIntervalMs = 5000;
  };

  void begin(uint32_t nowMs, bool connected);
  void update(bool connected, uint32_t nowMs);
  bool takeLostEvent();
  bool initialTimedOut(uint32_t nowMs) const;
  bool shouldAttemptReconnect(uint32_t nowMs) const;
  void notifyReconnectStarted(uint32_t nowMs);

  bool connected() const { return connected_; }

 private:
  Config config_{};
  bool connected_ = false;
  bool lostEvent_ = false;
  uint32_t startedAt_ = 0;
  uint32_t lastReconnectAt_ = 0;
};

}  // namespace radio
