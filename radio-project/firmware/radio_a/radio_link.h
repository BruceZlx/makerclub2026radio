#pragma once

#include <stdint.h>

#include "app_controller.h"
#include "link_transport.h"
#include "protocol_v2.h"
#include "radio_types.h"

namespace radio {

struct LinkEvent {
  enum class Kind : uint8_t { None = 0, Done, PeerHello, ConfigMismatch, DisplayRebooted };
  Kind kind = Kind::None;
  uint16_t session = 0;
  uint32_t request = 0;
  uint8_t city = 0;
};

// Board A side of the v2 link: owns HELLO, periodic snapshots, DONE/ACK and
// display-timeout degradation. Parsing is bounded by protocol_v2.
class RadioLink : public DisplayLinkInterface {
 public:
  struct Config {
    const char* configId = "00000000";
    uint16_t bootId = 1;
    uint32_t helloIntervalMs = 1000;
    uint32_t snapshotIntervalMs = 1000;
  };

  RadioLink(LinkTransport& transport, const Config& config);

  void begin(uint16_t session, uint32_t nowMs);
  void setSession(uint16_t session) override;
  void sendSnapshot(const Snapshot& snapshot) override;
  void resendSnapshot() override;
  void sendAck(uint16_t session, uint32_t request) override;
  void markDisplayDegraded() override { displayDegraded_ = true; }
  void handleLine(const char* line, size_t length, uint32_t nowMs);
  void tick(uint32_t nowMs);
  bool pollEvent(LinkEvent& event);

  bool peerHello() const { return peerHello_; }
  bool configMismatch() const { return configMismatch_; }
  bool displayDegraded() const { return displayDegraded_; }
  const Snapshot& lastSnapshot() const { return lastSnapshot_; }
  bool hasSnapshot() const { return hasSnapshot_; }

 private:
  void sendHello(uint32_t nowMs);
  void writeFrame(const char* frame, size_t length);

  LinkTransport& transport_;
  Config config_{};
  uint32_t configValue_ = 0;
  protocol::FrameParser parser_;
  Snapshot lastSnapshot_{};
  bool hasSnapshot_ = false;
  uint16_t session_ = 1;
  uint32_t lastHelloAt_ = 0;
  uint32_t lastSnapshotAt_ = 0;
  bool peerHello_ = false;
  bool configMismatch_ = false;
  bool displayDegraded_ = false;
  uint16_t peerBoot_ = 0;
  bool havePeerBoot_ = false;
  LinkEvent pending_[8];
  size_t pendingCount_ = 0;
};

}  // namespace radio
