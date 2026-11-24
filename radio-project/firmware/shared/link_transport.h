#pragma once

#include <stddef.h>
#include <stdint.h>

namespace radio {

class LinkTransport {
 public:
  virtual ~LinkTransport() = default;
  virtual void write(const uint8_t* data, size_t length) = 0;
};

}  // namespace radio
