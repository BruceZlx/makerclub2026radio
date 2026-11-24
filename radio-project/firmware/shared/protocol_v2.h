#pragma once

#include <stddef.h>
#include <stdint.h>

#include "radio_types.h"

namespace radio {
namespace protocol {

constexpr uint8_t kVersion = 2;
constexpr size_t kMaxFrameBytes = 160;
constexpr uint32_t kBaud = 115200;
constexpr size_t kMaxFields = 9;

enum class FrameKind : uint8_t { None = 0, Hello, Snapshot, Done, Ack };
enum class ParseStatus : uint8_t { NeedMore = 0, Ok, Empty, Invalid, Oversize };

struct Frame {
  FrameKind kind = FrameKind::None;
  uint8_t board = 0;  // 0 = A, 1 = B
  uint16_t boot = 0;
  uint32_t config = 0;
  uint16_t session = 0;
  uint32_t request = 0;
  uint8_t city = 0;
  View view = View::Standby;
  Anim anim = Anim::None;
  AudioState audio = AudioState::Idle;
  Source source = Source::None;
  bool ackDone = false;
};

class FrameParser {
 public:
  // Feed one byte. Returns Ok/Invalid/Empty/Oversize exactly when a complete
  // line has been consumed; NeedMore otherwise.
  ParseStatus feed(uint8_t byte, Frame& out);
  void reset();
  size_t droppedFrames() const { return dropped_; }
  size_t maxBufferUsed() const { return maxUsed_; }
  size_t bufferedBytes() const { return length_; }

 private:
  char buffer_[kMaxFrameBytes];
  size_t length_ = 0;
  bool overflow_ = false;
  size_t dropped_ = 0;
  size_t maxUsed_ = 0;
};

// Parse one complete line without its line terminator.
bool parseLine(const char* line, size_t length, Frame& out);

// All encoders return the number of bytes written (excluding the trailing
// NUL) or 0 when the buffer is too small.
size_t encodeHello(char* out, size_t capacity, uint8_t board, uint16_t boot,
                   uint32_t config);
size_t encodeSnapshot(char* out, size_t capacity, const Snapshot& snapshot);
size_t encodeDone(char* out, size_t capacity, uint16_t session, uint32_t request,
                  uint8_t city);
size_t encodeAck(char* out, size_t capacity, uint16_t session, uint32_t request);

}  // namespace protocol
}  // namespace radio
