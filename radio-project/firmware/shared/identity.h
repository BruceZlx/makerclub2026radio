#pragma once

#include <stdint.h>

namespace radio {

struct RequestIdentity {
  uint16_t session = 0;
  uint32_t request = 0;
};

// Session/request/attempt identity. Requests are allowed to wrap only after a
// new session is generated, so a monotonically increasing request comparison
// never crosses a session boundary.
class Identity {
 public:
  void reset(uint16_t session);
  RequestIdentity newRequest();
  uint32_t nextAttempt();
  bool requestNearWrap() const;

  uint16_t session() const { return session_; }
  uint32_t nextRequestValue() const { return nextRequest_; }
  uint32_t attempt() const { return attempt_; }

  // Test hook: move the counter close to the wrap boundary.
  void setNextRequestForTest(uint32_t value) { nextRequest_ = value; }

 private:
  uint16_t session_ = 1;
  uint32_t nextRequest_ = 1;
  uint32_t attempt_ = 1;
};

}  // namespace radio
