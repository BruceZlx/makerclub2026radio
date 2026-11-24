#include "protocol_v2.h"

#include <stdio.h>
#include <string.h>

namespace radio {
namespace protocol {
namespace {

struct Token {
  const char* data = nullptr;
  size_t length = 0;
};

bool tokenEquals(const Token& token, const char* text) {
  const size_t length = strlen(text);
  return token.length == length && memcmp(token.data, text, length) == 0;
}

bool tokenIs(const Token& token, char value) {
  return token.length == 1 && token.data[0] == value;
}

bool copyToken(const Token& token, char* out, size_t capacity) {
  if (token.length + 1 > capacity) return false;
  memcpy(out, token.data, token.length);
  out[token.length] = '\0';
  return true;
}

bool parseDecimal(const Token& token, uint32_t& value) {
  if (token.length == 0 || token.length > 10) return false;
  uint64_t accumulated = 0;
  for (size_t i = 0; i < token.length; ++i) {
    const char c = token.data[i];
    if (c < '0' || c > '9') return false;
    accumulated = accumulated * 10u + static_cast<uint64_t>(c - '0');
    if (accumulated > 0xFFFFFFFFull) return false;
  }
  value = static_cast<uint32_t>(accumulated);
  return true;
}

bool parseHex(const Token& token, uint32_t& value, size_t expectedLength) {
  if (token.length != expectedLength) return false;
  uint32_t accumulated = 0;
  for (size_t i = 0; i < token.length; ++i) {
    const char c = token.data[i];
    uint8_t digit;
    if (c >= '0' && c <= '9') {
      digit = static_cast<uint8_t>(c - '0');
    } else if (c >= 'A' && c <= 'F') {
      digit = static_cast<uint8_t>(c - 'A' + 10);
    } else {
      return false;
    }
    accumulated = (accumulated << 4) | digit;
  }
  value = accumulated;
  return true;
}

size_t splitTokens(char* line, size_t length, Token* tokens, size_t maxTokens) {
  size_t count = 0;
  size_t start = 0;
  for (size_t i = 0; i <= length; ++i) {
    if (i == length || line[i] == ':') {
      if (count >= maxTokens) return 0;
      tokens[count].data = line + start;
      tokens[count].length = i - start;
      ++count;
      start = i + 1;
    }
  }
  return count;
}

bool parseEnum(const Token& token, View& value) {
  char text[16];
  if (!copyToken(token, text, sizeof(text))) return false;
  return parseView(text, value);
}

bool parseEnum(const Token& token, Anim& value) {
  char text[16];
  if (!copyToken(token, text, sizeof(text))) return false;
  return parseAnim(text, value);
}

bool parseEnum(const Token& token, AudioState& value) {
  char text[16];
  if (!copyToken(token, text, sizeof(text))) return false;
  return parseAudio(text, value);
}

bool parseEnum(const Token& token, Source& value) {
  char text[16];
  if (!copyToken(token, text, sizeof(text))) return false;
  return parseSource(text, value);
}

}  // namespace

ParseStatus FrameParser::feed(uint8_t byte, Frame& out) {
  if (byte == '\n') {
    if (overflow_) {
      reset();
      ++dropped_;
      return ParseStatus::Oversize;
    }
    size_t length = length_;
    if (length > 0 && buffer_[length - 1] == '\r') --length;
    if (length == 0) {
      length_ = 0;
      return ParseStatus::Empty;
    }
    buffer_[length] = '\0';
    const bool valid = parseLine(buffer_, length, out);
    if (!valid) ++dropped_;
    ParseStatus status = valid ? ParseStatus::Ok : ParseStatus::Invalid;
    length_ = 0;
    return status;
  }

  if (byte == '\r') {
    // CR is only accepted as part of CRLF. A bare CR is ignored; a CR in the
    // middle of a frame remains in the buffer and will fail field validation.
    if (length_ == 0 || overflow_) return ParseStatus::NeedMore;
    if (length_ < kMaxFrameBytes) {
      buffer_[length_++] = static_cast<char>(byte);
      if (length_ > maxUsed_) maxUsed_ = length_;
    } else {
      overflow_ = true;
    }
    return ParseStatus::NeedMore;
  }

  if (byte == 0) {
    ++dropped_;
    length_ = 0;
    overflow_ = false;
    return ParseStatus::Invalid;
  }

  if (overflow_) return ParseStatus::NeedMore;
  if (length_ + 1 >= kMaxFrameBytes) {
    overflow_ = true;
    return ParseStatus::NeedMore;
  }
  buffer_[length_++] = static_cast<char>(byte);
  if (length_ > maxUsed_) maxUsed_ = length_;
  return ParseStatus::NeedMore;
}

void FrameParser::reset() {
  length_ = 0;
  overflow_ = false;
}

bool parseLine(const char* line, size_t length, Frame& out) {
  if (line == nullptr || length == 0 || length >= kMaxFrameBytes) return false;
  char copy[kMaxFrameBytes];
  memcpy(copy, line, length);
  copy[length] = '\0';

  Token tokens[kMaxFields];
  const size_t count = splitTokens(copy, length, tokens, kMaxFields);
  if (count == 0) return false;

  Frame frame;
  if (tokenEquals(tokens[0], "HELLO")) {
    if (count != 5 || !tokenIs(tokens[1], '2')) return false;
    if (!tokenIs(tokens[2], 'A') && !tokenIs(tokens[2], 'B')) return false;
    uint32_t boot = 0;
    if (!parseHex(tokens[3], boot, 4) || boot == 0) return false;
    uint32_t config = 0;
    if (!parseHex(tokens[4], config, 8)) return false;
    frame.kind = FrameKind::Hello;
    frame.board = tokens[2].data[0] == 'A' ? 0 : 1;
    frame.boot = static_cast<uint16_t>(boot);
    frame.config = config;
    out = frame;
    return true;
  }

  if (tokenEquals(tokens[0], "ST")) {
    if (count != 9 || !tokenIs(tokens[1], '2')) return false;
    uint32_t session = 0;
    if (!parseHex(tokens[2], session, 4) || session == 0) return false;
    if (!parseDecimal(tokens[3], frame.request)) return false;
    uint32_t cityValue = 0;
    if (!parseDecimal(tokens[4], cityValue) || cityValue > 7) return false;
    frame.city = static_cast<uint8_t>(cityValue);
    if (!parseEnum(tokens[5], frame.view)) return false;
    if (!parseEnum(tokens[6], frame.anim)) return false;
    if (!parseEnum(tokens[7], frame.audio)) return false;
    if (!parseEnum(tokens[8], frame.source)) return false;
    frame.kind = FrameKind::Snapshot;
    frame.session = static_cast<uint16_t>(session);
    out = frame;
    return true;
  }

  if (tokenEquals(tokens[0], "DONE")) {
    if (count != 5 || !tokenIs(tokens[1], '2')) return false;
    uint32_t session = 0;
    if (!parseHex(tokens[2], session, 4) || session == 0) return false;
    if (!parseDecimal(tokens[3], frame.request)) return false;
    uint32_t cityValue = 0;
    if (!parseDecimal(tokens[4], cityValue) || cityValue > 7) return false;
    frame.city = static_cast<uint8_t>(cityValue);
    frame.kind = FrameKind::Done;
    frame.session = static_cast<uint16_t>(session);
    out = frame;
    return true;
  }

  if (tokenEquals(tokens[0], "ACK")) {
    if (count != 5 || !tokenIs(tokens[1], '2')) return false;
    uint32_t session = 0;
    if (!parseHex(tokens[2], session, 4) || session == 0) return false;
    if (!parseDecimal(tokens[3], frame.request)) return false;
    if (!tokenEquals(tokens[4], "DONE")) return false;
    frame.kind = FrameKind::Ack;
    frame.session = static_cast<uint16_t>(session);
    frame.ackDone = true;
    out = frame;
    return true;
  }

  return false;
}

size_t encodeHello(char* out, size_t capacity, uint8_t board, uint16_t boot,
                   uint32_t config) {
  if (out == nullptr || capacity == 0) return 0;
  const int written = snprintf(out, capacity, "HELLO:2:%c:%04X:%08X",
                               board == 0 ? 'A' : 'B',
                               static_cast<unsigned int>(boot),
                               static_cast<unsigned int>(config));
  if (written <= 0 || static_cast<size_t>(written) >= capacity) return 0;
  return static_cast<size_t>(written);
}

size_t encodeSnapshot(char* out, size_t capacity, const Snapshot& snapshot) {
  if (out == nullptr || capacity == 0) return 0;
  const int written = snprintf(
      out, capacity, "ST:2:%04X:%lu:%u:%s:%s:%s:%s",
      static_cast<unsigned int>(snapshot.session),
      static_cast<unsigned long>(snapshot.request),
      static_cast<unsigned int>(snapshot.city), viewName(snapshot.view),
      animName(snapshot.anim), audioName(snapshot.audio),
      sourceName(snapshot.source));
  if (written <= 0 || static_cast<size_t>(written) >= capacity) return 0;
  return static_cast<size_t>(written);
}

size_t encodeDone(char* out, size_t capacity, uint16_t session, uint32_t request,
                  uint8_t city) {
  if (out == nullptr || capacity == 0) return 0;
  const int written = snprintf(out, capacity, "DONE:2:%04X:%lu:%u",
                               static_cast<unsigned int>(session),
                               static_cast<unsigned long>(request),
                               static_cast<unsigned int>(city));
  if (written <= 0 || static_cast<size_t>(written) >= capacity) return 0;
  return static_cast<size_t>(written);
}

size_t encodeAck(char* out, size_t capacity, uint16_t session, uint32_t request) {
  if (out == nullptr || capacity == 0) return 0;
  const int written = snprintf(out, capacity, "ACK:2:%04X:%lu:DONE",
                               static_cast<unsigned int>(session),
                               static_cast<unsigned long>(request));
  if (written <= 0 || static_cast<size_t>(written) >= capacity) return 0;
  return static_cast<size_t>(written);
}

}  // namespace protocol
}  // namespace radio
