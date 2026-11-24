#include "wifi_service.h"

namespace radio {

void WifiService::begin(uint32_t nowMs, bool connected) {
  connected_ = connected;
  lostEvent_ = false;
  startedAt_ = nowMs;
  lastReconnectAt_ = nowMs;
}

void WifiService::update(bool connected, uint32_t nowMs) {
  (void)nowMs;
  if (connected_ && !connected) lostEvent_ = true;
  connected_ = connected;
}

bool WifiService::takeLostEvent() {
  if (!lostEvent_) return false;
  lostEvent_ = false;
  return true;
}

bool WifiService::initialTimedOut(uint32_t nowMs) const {
  return !connected_ && nowMs - startedAt_ >= config_.initialTimeoutMs;
}

bool WifiService::shouldAttemptReconnect(uint32_t nowMs) const {
  return !connected_ && nowMs - lastReconnectAt_ >= config_.reconnectIntervalMs;
}

void WifiService::notifyReconnectStarted(uint32_t nowMs) { lastReconnectAt_ = nowMs; }

}  // namespace radio
