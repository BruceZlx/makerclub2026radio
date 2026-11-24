#include "input_service.h"

namespace radio {
namespace {

int8_t quadratureDelta(uint8_t previous, uint8_t current) {
  static const int8_t kTable[16] = {
      0,  -1, 1,  0,   //
      1,  0,  0,  -1,  //
      -1, 0,  0,  1,   //
      0,  1,  -1, 0,   //
  };
  return kTable[(previous << 2) | (current & 0x03)];
}

}  // namespace

void Encoder::begin(bool a, bool b, bool sw, uint32_t nowMs) {
  if (config_.stepsPerDetent == 0) config_.stepsPerDetent = 1;
  state_ = (a ? 1u : 0u) | (b ? 2u : 0u);
  accumulator_ = 0;
  position_ = 0;
  rawPressed_ = sw;
  stablePressed_ = sw;
  lastSwitchChangeMs_ = nowMs;
  pressPending_ = false;
}

InputEvent Encoder::update(bool a, bool b, bool sw, uint32_t nowMs) {
  InputEvent event;
  const uint8_t next = (a ? 1u : 0u) | (b ? 2u : 0u);
  if (next != state_) {
    accumulator_ += quadratureDelta(state_, next);
    state_ = next;
  }

  // Switch debounce runs every poll, even when a rotation is also ready. The
  // release event is held and delivered after the higher-priority rotation.
  if (sw != rawPressed_) {
    rawPressed_ = sw;
    lastSwitchChangeMs_ = nowMs;
  } else if (static_cast<uint32_t>(nowMs - lastSwitchChangeMs_) >= config_.pressDebounceMs &&
             stablePressed_ != rawPressed_) {
    const bool wasPressed = stablePressed_;
    stablePressed_ = rawPressed_;
    if (!stablePressed_ && wasPressed) {
      pressPending_ = true;
    }
  }

  if (accumulator_ >= config_.stepsPerDetent) {
    accumulator_ -= config_.stepsPerDetent;
    position_ += 1;
    event.kind = InputEventKind::Rotate;
    event.delta = 1;
    event.atMs = nowMs;
    return event;
  }
  if (accumulator_ <= -config_.stepsPerDetent) {
    accumulator_ += config_.stepsPerDetent;
    position_ -= 1;
    event.kind = InputEventKind::Rotate;
    event.delta = -1;
    event.atMs = nowMs;
    return event;
  }

  if (pressPending_) {
    pressPending_ = false;
    event.kind = InputEventKind::EncoderPress;
    event.atMs = nowMs;
    return event;
  }

  event.kind = InputEventKind::None;
  return event;
}

void InputService::begin(uint32_t nowMs) {
  encoder_.begin(reader_.readA(), reader_.readB(), reader_.readSwitch(), nowMs);
  hasPending_ = false;
}

bool InputService::update(uint32_t nowMs, InputEvent& event) {
  if (hasPending_) {
    event = pending_;
    hasPending_ = false;
    return true;
  }
  InputEvent produced = encoder_.update(reader_.readA(), reader_.readB(),
                                        reader_.readSwitch(), nowMs);
  if (produced.kind == InputEventKind::None) return false;
  // If a rotation and a press happen in the same poll, rotation wins and the
  // press is delivered on the next poll.
  event = produced;
  return true;
}

}  // namespace radio
