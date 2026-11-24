#pragma once

#include <stdint.h>

#include "display_controller.h"
#include "link_transport.h"
#include "protocol_v2.h"

namespace radio {

// Board B side of the v2 link. It only frames/validates; animation state is
// owned by DisplayController.
class DisplayLink {
 public:
  struct Config {
    const char* configId = "00000000";
    uint16_t bootId = 1;
    uint32_t helloIntervalMs = 1000;
  };

  DisplayLink(LinkTransport& transport, DisplayController& controller,
              const Config& config);

  void begin(uint32_t nowMs);
  void handleLine(const char* line, size_t length, uint32_t nowMs);
  void tick(uint32_t nowMs);
  bool configMismatch() const { return controller_.configMismatch(); }

 private:
  void sendHello(uint32_t nowMs);
  void writeFrame(const char* frame, size_t length);

  LinkTransport& transport_;
  DisplayController& controller_;
  Config config_{};
  uint32_t configValue_ = 0;
  protocol::FrameParser parser_;
  uint32_t lastHelloAt_ = 0;
  bool handshakeSeen_ = false;
};

}  // namespace radio
