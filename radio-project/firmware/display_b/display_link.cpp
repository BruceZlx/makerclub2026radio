#include "display_link.h"

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

DisplayLink::DisplayLink(LinkTransport& transport, DisplayController& controller,
                         const Config& config)
    : transport_(transport), controller_(controller), config_(config) {
  configValue_ = configValue(config_.configId);
}

void DisplayLink::sendHello(uint32_t nowMs) {
  char frame[protocol::kMaxFrameBytes];
  const size_t length =
      protocol::encodeHello(frame, sizeof(frame), 1, config_.bootId, configValue_);
  if (length > 0) writeFrame(frame, length);
  lastHelloAt_ = nowMs;
}

void DisplayLink::writeFrame(const char* frame, size_t length) {
  if (length == 0) return;
  transport_.write(reinterpret_cast<const uint8_t*>(frame), length);
  const uint8_t newline = '\n';
  transport_.write(&newline, 1);
}

void DisplayLink::begin(uint32_t nowMs) {
  parser_.reset();
  handshakeSeen_ = false;
  lastHelloAt_ = 0;
  sendHello(nowMs);
}

void DisplayLink::handleLine(const char* line, size_t length, uint32_t nowMs) {
  (void)nowMs;
  protocol::Frame frame;
  if (!protocol::parseLine(line, length, frame)) return;
  if (frame.kind == protocol::FrameKind::Hello) {
    if (frame.board != 0) return;
    handshakeSeen_ = true;
    controller_.handleHello(frame.board, frame.boot, frame.config, nowMs);
    return;
  }
  if (frame.kind == protocol::FrameKind::Snapshot) {
    Snapshot snapshot;
    snapshot.session = frame.session;
    snapshot.request = frame.request;
    snapshot.city = frame.city;
    snapshot.view = frame.view;
    snapshot.anim = frame.anim;
    snapshot.audio = frame.audio;
    snapshot.source = frame.source;
    controller_.handleSnapshot(snapshot, nowMs);
    return;
  }
  if (frame.kind == protocol::FrameKind::Ack) {
    controller_.handleAck(frame.session, frame.request);
  }
}

void DisplayLink::tick(uint32_t nowMs) {
  if (!handshakeSeen_ && nowMs - lastHelloAt_ >= config_.helloIntervalMs) {
    sendHello(nowMs);
  }
  if (controller_.tick(nowMs)) {
    DisplayDoneEvent event;
    if (controller_.takeDone(event)) {
      char frame[protocol::kMaxFrameBytes];
      const size_t length =
          protocol::encodeDone(frame, sizeof(frame), event.session, event.request,
                               event.city);
      if (length > 0) writeFrame(frame, length);
    }
  }
}

}  // namespace radio
