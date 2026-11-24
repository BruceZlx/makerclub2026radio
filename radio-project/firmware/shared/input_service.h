#pragma once

#include <stdint.h>

namespace radio {

enum class InputEventKind : uint8_t { None = 0, Rotate, EncoderPress };

struct InputEvent {
  InputEventKind kind = InputEventKind::None;
  int8_t delta = 0;
  uint32_t atMs = 0;
};

class PinReader {
 public:
  virtual ~PinReader() = default;
  virtual bool readA() = 0;
  virtual bool readB() = 0;
  virtual bool readSwitch() = 0;
};

// Quadrature decoder with a configurable number of edges per detent and a
// release-triggered, 50 ms debounced encoder press.
class Encoder {
 public:
  struct Config {
    uint8_t stepsPerDetent = 4;
    uint16_t pressDebounceMs = 50;
  };

  void configure(const Config& config) {
    config_ = config;
    if (config_.stepsPerDetent == 0) config_.stepsPerDetent = 1;
  }

  void begin(bool a, bool b, bool sw, uint32_t nowMs);
  InputEvent update(bool a, bool b, bool sw, uint32_t nowMs);
  int32_t position() const { return position_; }

 private:
  Config config_{};
  uint8_t state_ = 0;
  int8_t accumulator_ = 0;
  int32_t position_ = 0;

  bool rawPressed_ = false;
  bool stablePressed_ = false;
  uint32_t lastSwitchChangeMs_ = 0;
  bool pressPending_ = false;
};

class InputService {
 public:
  explicit InputService(PinReader& reader) : reader_(reader) {}

  void configure(const Encoder::Config& config) { encoder_.configure(config); }
  void begin(uint32_t nowMs);
  // Returns true when an event is available. Rotations are always returned
  // before a pending press from the same polling cycle.
  bool update(uint32_t nowMs, InputEvent& event);

 private:
  PinReader& reader_;
  Encoder encoder_;
  InputEvent pending_{};
  bool hasPending_ = false;
};

}  // namespace radio
