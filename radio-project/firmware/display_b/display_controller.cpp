#include "display_controller.h"

namespace radio {

void DisplayController::begin(uint16_t bootId, const char* configId,
                              uint32_t nowMs) {
  bootId_ = bootId == 0 ? 1 : bootId;
  configId_ = 0;
  if (configId != nullptr) {
    for (size_t i = 0; i < 8 && configId[i] != '\0'; ++i) {
      configId_ <<= 4;
      const char c = configId[i];
      if (c >= '0' && c <= '9') {
        configId_ |= static_cast<uint32_t>(c - '0');
      } else if (c >= 'A' && c <= 'F') {
        configId_ |= static_cast<uint32_t>(c - 'A' + 10);
      } else if (c >= 'a' && c <= 'f') {
        configId_ |= static_cast<uint32_t>(c - 'a' + 10);
      }
    }
  }
  configMismatch_ = false;
  handshake_ = false;
  peerSession_ = 0;
  peerBoot_ = 0;
  arcStartedAt_ = 0;
  lastDoneAt_ = 0;
  doneDue_ = false;
  state_ = DisplayRenderState{};
  state_.standbyStartedAt = nowMs;
  pendingDone_ = DisplayDoneEvent{};
}

void DisplayController::handleHello(uint8_t board, uint16_t boot, uint32_t config,
                                    uint32_t nowMs) {
  (void)nowMs;
  if (board != 0) return;
  if (config != configId_) {
    configMismatch_ = true;
    handshake_ = false;
    state_.view = View::Standby;
    state_.anim = Anim::None;
    state_.arcProgress = 0.0f;
    state_.awaitingAck = false;
    doneDue_ = false;
    return;
  }
  configMismatch_ = false;
  handshake_ = true;
  peerSession_ = boot;
  if (peerBoot_ != boot) {
    peerBoot_ = boot;
    // A new board A session invalidates the previous request. The next ST
    // snapshot with this session will restore the authoritative state.
    state_.request = 0;
    state_.view = View::Standby;
    state_.anim = Anim::None;
    state_.awaitingAck = false;
    state_.arcProgress = 0.0f;
    doneDue_ = false;
  }
}

void DisplayController::startArc(uint16_t session, uint32_t request, uint8_t city,
                                 uint32_t nowMs) {
  state_.session = session;
  state_.request = request;
  state_.city = city;
  state_.view = View::Map;
  state_.anim = Anim::Running;
  state_.arcProgress = 0.0f;
  state_.awaitingAck = false;
  state_.doneRetries = 0;
  arcStartedAt_ = nowMs;
  lastDoneAt_ = 0;
  doneDue_ = false;
}

void DisplayController::finishArc(uint32_t nowMs) {
  state_.anim = Anim::Done;
  state_.arcProgress = 1.0f;
  state_.awaitingAck = true;
  state_.doneRetries = 0;
  lastDoneAt_ = nowMs;
  queueDone();
}

void DisplayController::queueDone() {
  pendingDone_.session = state_.session;
  pendingDone_.request = state_.request;
  pendingDone_.city = state_.city;
  doneDue_ = true;
}

void DisplayController::handleSnapshot(const Snapshot& snapshot, uint32_t nowMs) {
  if (!handshake_ || configMismatch_) return;
  if (snapshot.session != peerSession_) return;

  if (snapshot.request < state_.request) return;

  if (snapshot.request == state_.request) {
    state_.city = snapshot.city;
    state_.view = snapshot.view;
    state_.audio = snapshot.audio;
    state_.source = snapshot.source;
    if (snapshot.view == View::Standby) {
      state_.anim = Anim::None;
      state_.arcProgress = 0.0f;
      state_.awaitingAck = false;
      doneDue_ = false;
      state_.standbyStartedAt = nowMs;
      return;
    }
    if (snapshot.anim == Anim::Done && state_.anim == Anim::Running) {
      finishArc(nowMs);
    }
    // A repeated snapshot for the same request never restarts the arc, even
    // if it says RUNNING while the display has already finished.
    return;
  }

  state_.session = snapshot.session;
  state_.request = snapshot.request;
  state_.city = snapshot.city;
  state_.view = snapshot.view;
  state_.audio = snapshot.audio;
  state_.source = snapshot.source;

  if (snapshot.view == View::Map) {
    if (snapshot.anim == Anim::Done) {
      state_.anim = Anim::Done;
      state_.arcProgress = 1.0f;
      state_.awaitingAck = true;
      state_.doneRetries = 0;
      lastDoneAt_ = nowMs;
      queueDone();
    } else if (snapshot.anim == Anim::Running) {
      startArc(snapshot.session, snapshot.request, snapshot.city, nowMs);
    } else {
      state_.anim = Anim::None;
      state_.arcProgress = 0.0f;
      state_.awaitingAck = false;
      doneDue_ = false;
    }
    return;
  }

  if (snapshot.view == View::Standby) {
    state_.anim = Anim::None;
    state_.arcProgress = 0.0f;
    state_.awaitingAck = false;
    doneDue_ = false;
    state_.standbyStartedAt = nowMs;
    return;
  }

  state_.anim = Anim::None;
  state_.arcProgress = 0.0f;
  state_.awaitingAck = false;
  doneDue_ = false;
}

void DisplayController::handleAck(uint16_t session, uint32_t request) {
  if (session != state_.session || request != state_.request) return;
  state_.awaitingAck = false;
}

bool DisplayController::tick(uint32_t nowMs) {
  if (state_.view == View::Map && state_.anim == Anim::Running) {
    const uint32_t elapsed = nowMs - arcStartedAt_;
    if (elapsed >= config_.arcDurationMs) {
      finishArc(nowMs);
    } else {
      state_.arcProgress =
          static_cast<float>(elapsed) / static_cast<float>(config_.arcDurationMs);
    }
  }

  if (!doneDue_ && state_.awaitingAck) {
    if (nowMs - lastDoneAt_ >= config_.doneRetryIntervalMs) {
      if (state_.doneRetries < config_.doneMaxRetries) {
        ++state_.doneRetries;
        lastDoneAt_ = nowMs;
        queueDone();
      } else {
        state_.awaitingAck = false;
      }
    }
  }
  return doneDue_;
}

bool DisplayController::takeDone(DisplayDoneEvent& event) {
  if (!doneDue_) return false;
  event = pendingDone_;
  doneDue_ = false;
  return true;
}

}  // namespace radio
