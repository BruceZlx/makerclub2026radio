#pragma once

#include <stdint.h>

namespace radio {

class Clock {
 public:
  virtual ~Clock() = default;
  virtual uint32_t nowMs() = 0;
};

class ManualClock : public Clock {
 public:
  uint32_t nowMs() override { return nowMs_; }
  void set(uint32_t value) { nowMs_ = value; }
  void advance(uint32_t delta) { nowMs_ += delta; }

 private:
  uint32_t nowMs_ = 0;
};

}  // namespace radio
