#pragma once

#include <stdint.h>

#include "radio_types.h"

namespace radio {

struct DisplayRenderState {
  bool handshake = false;
  uint16_t session = 0;
  uint32_t request = 0;
  uint8_t city = 0;
  View view = View::Standby;
  Anim anim = Anim::None;
  AudioState audio = AudioState::Idle;
  Source source = Source::None;
  float arcProgress = 0.0f;
  uint32_t standbyStartedAt = 0;
  bool awaitingAck = false;
  uint8_t doneRetries = 0;
};

class DisplayController {
 public:
  struct Config {
    uint32_t arcDurationMs = 1500;
    uint32_t doneRetryIntervalMs = 250;
    uint8_t doneMaxRetries = 8;
  };

  void begin(uint16_t bootId, const char* configId, uint32_t nowMs);
  void handleHello(uint8_t board, uint16_t boot, uint32_t config, uint32_t nowMs);
  void handleSnapshot(const Snapshot& snapshot, uint32_t nowMs);
  void handleAck(uint16_t session, uint32_t request);

  // Returns true when the caller should transmit a DONE frame now.
  bool tick(uint32_t nowMs);
  bool takeDone(DisplayDoneEvent& event);

  const DisplayRenderState& state() const { return state_; }
  bool configMismatch() const { return configMismatch_; }
  uint8_t doneRetryCount() const { return state_.doneRetries; }

 private:
  void startArc(uint16_t session, uint32_t request, uint8_t city, uint32_t nowMs);
  void finishArc(uint32_t nowMs);
  void queueDone();

  Config config_{};
  uint16_t bootId_ = 1;
  uint32_t configId_ = 0;
  bool configMismatch_ = false;
  bool handshake_ = false;
  uint16_t peerSession_ = 0;
  uint32_t peerBoot_ = 0;
  uint32_t arcStartedAt_ = 0;
  uint32_t lastDoneAt_ = 0;
  bool doneDue_ = false;
  DisplayRenderState state_{};
  DisplayDoneEvent pendingDone_{};
};

}  // namespace radio
