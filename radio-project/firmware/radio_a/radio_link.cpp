#include "radio_link.h"

#include <string.h>

namespace radio {
namespace {

uint32_t configValue(const char* text) {
  uint32_t value = 0;
  if (text == nullptr) return 0;
  for (size_t i = 0; i < 8 && text[i] != '\0'; ++i) {
    value <<= 4;
    const char c = text[i];
    if (c >= '0' && c <= '9') {
      value |= static_cast<uint32_t>(c - '0');
    } else if (c >= 'A' && c <= 'F') {
      value |= static_cast<uint32_t>(c - 'A' + 10);
    } else if (c >= 'a' && c <= 'f') {
      value |= static_cast<uint32_t>(c - 'a' + 10);
    }
  }
  return value;
}

}  // namespace

RadioLink::RadioLink(LinkTransport& transport, const Config& config)
    : transport_(transport), config_(config) {
  configValue_ = configValue(config_.configId);
}

void RadioLink::begin(uint16_t session, uint32_t nowMs) {
  session_ = session == 0 ? 1 : session;
  parser_.reset();
  hasSnapshot_ = false;
  peerHello_ = false;
  configMismatch_ = false;
  displayDegraded_ = false;
  peerBoot_ = 0;
  havePeerBoot_ = false;
  pendingCount_ = 0;
  lastHelloAt_ = 0;
  lastSnapshotAt_ = 0;
  sendHello(nowMs);
}

void RadioLink::sendHello(uint32_t nowMs) {
  char frame[protocol::kMaxFrameBytes];
  const size_t length =
      protocol::encodeHello(frame, sizeof(frame), 0, session_, configValue_);
  if (length > 0) writeFrame(frame, length);
  lastHelloAt_ = nowMs;
}

void RadioLink::writeFrame(const char* frame, size_t length) {
  if (length == 0) return;
  transport_.write(reinterpret_cast<const uint8_t*>(frame), length);
  const uint8_t newline = '\n';
  transport_.write(&newline, 1);
}

void RadioLink::setSession(uint16_t session) {
  session_ = session == 0 ? 1 : session;
  peerHello_ = false;
  lastHelloAt_ = 0;
  pendingCount_ = 0;
  hasSnapshot_ = false;
  sendHello(0);
}

void RadioLink::sendSnapshot(const Snapshot& snapshot) {
  lastSnapshot_ = snapshot;
  lastSnapshot_.session = session_;
  hasSnapshot_ = true;
  char frame[protocol::kMaxFrameBytes];
  const size_t length =
      protocol::encodeSnapshot(frame, sizeof(frame), lastSnapshot_);
  if (length > 0) writeFrame(frame, length);
}

void RadioLink::resendSnapshot() {
  if (!hasSnapshot_) return;
  char frame[protocol::kMaxFrameBytes];
  const size_t length =
      protocol::encodeSnapshot(frame, sizeof(frame), lastSnapshot_);
  if (length > 0) writeFrame(frame, length);
}

void RadioLink::sendAck(uint16_t session, uint32_t request) {
  char frame[protocol::kMaxFrameBytes];
  const size_t length =
      protocol::encodeAck(frame, sizeof(frame), session, request);
  if (length > 0) writeFrame(frame, length);
}

void RadioLink::handleLine(const char* line, size_t length, uint32_t nowMs) {
  (void)nowMs;
  protocol::Frame frame;
  if (!protocol::parseLine(line, length, frame)) return;
  if (frame.kind == protocol::FrameKind::Hello) {
    if (frame.board != 1) return;
    // A peer HELLO always triggers an immediate answer. This is what lets a
    // rebooted display complete the handshake even when board A had already
    // stopped its periodic HELLO.
    sendHello(nowMs);
    if (frame.config != configValue_) {
      configMismatch_ = true;
      if (pendingCount_ < sizeof(pending_) / sizeof(pending_[0])) {
        LinkEvent event;
        event.kind = LinkEvent::Kind::ConfigMismatch;
        pending_[pendingCount_++] = event;
      }
      return;
    }
    if (!peerHello_ || (havePeerBoot_ && peerBoot_ != frame.boot)) {
      if (havePeerBoot_ && peerBoot_ != frame.boot &&
          pendingCount_ < sizeof(pending_) / sizeof(pending_[0])) {
        LinkEvent event;
        event.kind = LinkEvent::Kind::DisplayRebooted;
        pending_[pendingCount_++] = event;
      }
    }
    peerHello_ = true;
    configMismatch_ = false;
    peerBoot_ = frame.boot;
    havePeerBoot_ = true;
    return;
  }
  if (frame.kind == protocol::FrameKind::Done) {
    if (pendingCount_ < sizeof(pending_) / sizeof(pending_[0])) {
      LinkEvent event;
      event.kind = LinkEvent::Kind::Done;
      event.session = frame.session;
      event.request = frame.request;
      event.city = frame.city;
      pending_[pendingCount_++] = event;
    }
  }
}

bool RadioLink::pollEvent(LinkEvent& event) {
  if (pendingCount_ == 0) return false;
  event = pending_[0];
  for (size_t i = 1; i < pendingCount_; ++i) pending_[i - 1] = pending_[i];
  --pendingCount_;
  return true;
}

void RadioLink::tick(uint32_t nowMs) {
  if (!peerHello_ && nowMs - lastHelloAt_ >= config_.helloIntervalMs) {
    sendHello(nowMs);
  }
  if (hasSnapshot_ && nowMs - lastSnapshotAt_ >= config_.snapshotIntervalMs) {
    resendSnapshot();
    lastSnapshotAt_ = nowMs;
  }
}

}  // namespace radio
