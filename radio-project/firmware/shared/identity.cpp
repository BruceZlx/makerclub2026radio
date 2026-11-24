#include "identity.h"

namespace radio {

void Identity::reset(uint16_t session) {
  session_ = session == 0 ? 1 : session;
  nextRequest_ = 1;
  attempt_ = 1;
}

RequestIdentity Identity::newRequest() {
  if (nextRequest_ >= 0xFFFFFFF0u) {
    session_ = static_cast<uint16_t>((session_ % 0xFFFEu) + 1u);
    nextRequest_ = 1;
  }
  RequestIdentity identity;
  identity.session = session_;
  identity.request = nextRequest_++;
  return identity;
}

uint32_t Identity::nextAttempt() {
  if (attempt_ == 0xFFFFFFFFu) attempt_ = 1;
  return attempt_++;
}

bool Identity::requestNearWrap() const { return nextRequest_ >= 0xFFFFFFF0u; }

}  // namespace radio
