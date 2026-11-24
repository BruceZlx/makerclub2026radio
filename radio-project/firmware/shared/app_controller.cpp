#include "app_controller.h"

namespace radio {

AppController::AppController(Clock& clock, const CityCatalog& catalog,
                             AudioAdapterInterface& audio,
                             DisplayLinkInterface& link, const Config& config)
    : clock_(clock),
      catalog_(catalog),
      audio_(audio),
      link_(link),
      config_(config) {}

void AppController::begin(uint32_t nowMs) {
  uint16_t session = config_.initialSession;
  if (session == 0) {
    session = static_cast<uint16_t>((nowMs ^ (nowMs >> 16)) | 1u);
  }
  identity_.reset(session);
  session_ = identity_.session();
  request_ = 0;
  attempt_ = 0;
  city_ = wrapCity(0);
  active_ = false;
  view_ = View::Standby;
  anim_ = Anim::None;
  audioState_ = AudioState::Idle;
  source_ = Source::None;
  muted_ = true;
  audioReady_ = false;
  soundOpened_ = false;
  displayDegraded_ = false;
  hasError_ = false;
  requestStartedAt_ = nowMs;
  sourceStartedAt_ = nowMs;
  lastUserActivityAt_ = nowMs;
  firstPcmAt_ = 0;
  lastPcmAt_ = 0;
  firstSoundAt_ = 0;
  errorAt_ = 0;
  displayDoneDeadline_ = 0;
  lastSnapshotAt_ = nowMs;
  lastActivityEventAt_ = nowMs;
  queueCount_ = 0;
  droppedEvents_ = 0;
  audio_.setMuted(true);
  audio_.stop();
  link_.setSession(session_);
  publish();
}

uint8_t AppController::wrapCity(int8_t delta) const {
  const uint8_t count = catalog_.count > 0 ? static_cast<uint8_t>(catalog_.count) : 8;
  int16_t value = static_cast<int16_t>(city_) + delta;
  while (value < 0) value += count;
  while (value >= count) value -= count;
  return static_cast<uint8_t>(value);
}

void AppController::push(const QueueEntry& entry) {
  if (queueCount_ >= sizeof(queue_) / sizeof(queue_[0])) {
    ++droppedEvents_;
    return;
  }
  queue_[queueCount_++] = entry;
}

void AppController::queueRotate(int8_t delta) {
  QueueEntry entry;
  entry.kind = QueueKind::Rotate;
  entry.delta = delta;
  push(entry);
}

void AppController::queuePress() {
  QueueEntry entry;
  entry.kind = QueueKind::Press;
  push(entry);
}

void AppController::queueWifiLost() {
  QueueEntry entry;
  entry.kind = QueueKind::WifiLost;
  push(entry);
}

void AppController::setWifiConnected(bool connected) {
  if (wifiConnected_ && !connected) queueWifiLost();
  wifiConnected_ = connected;
}

void AppController::queueAudio(const AudioEvent& event) {
  QueueEntry entry;
  entry.kind = QueueKind::Audio;
  entry.audio = event;
  push(entry);
}

void AppController::queueDisplayDone(const DisplayDoneEvent& event) {
  QueueEntry entry;
  entry.kind = QueueKind::DisplayDone;
  entry.done = event;
  push(entry);
}

void AppController::rotate(int8_t delta, uint32_t nowMs) {
  city_ = wrapCity(delta);
  const RequestIdentity identity = identity_.newRequest();
  session_ = identity.session;
  request_ = identity.request;
  link_.setSession(session_);

  active_ = true;
  view_ = View::Map;
  anim_ = Anim::Running;
  audioState_ = AudioState::Connecting;
  source_ = Source::None;
  hasError_ = false;
  errorAt_ = 0;
  audioReady_ = false;
  soundOpened_ = false;
  displayDegraded_ = false;
  firstPcmAt_ = 0;
  lastPcmAt_ = 0;
  firstSoundAt_ = 0;
  requestStartedAt_ = nowMs;
  sourceStartedAt_ = nowMs;
  lastUserActivityAt_ = nowMs;
  lastActivityEventAt_ = nowMs;
  displayDoneDeadline_ = nowMs + config_.timing.displayDoneTimeoutMs;
  lastSnapshotAt_ = 0;

  sourceManager_.begin(wifiConnected_);
  const Source next = sourceManager_.source();
  startAttempt(next, nowMs);
  publish();
}

void AppController::press(uint32_t nowMs) {
  if (!active_) return;  // A standby press must not wake the device.
  lastUserActivityAt_ = nowMs;
  lastActivityEventAt_ = nowMs;
}

void AppController::startAttempt(Source source, uint32_t nowMs) {
  const City* city = catalog_.byDisplayOrder(city_);
  if (city == nullptr && catalog_.count > 0) {
    city = catalog_.byId(city_);
  }
  source_ = source;
  attempt_ = identity_.nextAttempt();
  audioReady_ = false;
  soundOpened_ = false;
  firstPcmAt_ = 0;
  lastPcmAt_ = 0;
  sourceStartedAt_ = nowMs;
  audioState_ = AudioState::Connecting;
  muted_ = true;
  audio_.setMuted(true);
  audio_.stop();

  AudioStartRequest request;
  request.session = session_;
  request.request = request_;
  request.attempt = attempt_;
  request.city = city_;
  request.source = source;
  if (source == Source::Local) {
    request.path = city ? city->offlinePath : "";
  } else if (source == Source::Main) {
    request.url = city ? city->primaryUrl : "";
  } else if (source == Source::Backup) {
    request.url = city ? city->backupUrl : "";
  }
  audio_.start(request);
  publish();
}

void AppController::handleWifiLost(uint32_t nowMs) {
  wifiConnected_ = false;
  if (!active_) {
    publish();
    return;
  }
  if (source_ == Source::Local || hasError_) {
    publish();
    return;
  }
  sourceManager_.forceLocal();
  startAttempt(Source::Local, nowMs);
}

void AppController::handleAudio(const AudioEvent& event, uint32_t nowMs) {
  if (!active_ || hasError_) return;
  if (event.session != session_ || event.request != request_ ||
      event.attempt != attempt_ || event.city != city_) {
    return;
  }

  bool changed = false;
  if (event.kind == AudioEvent::Kind::Started) {
    audioState_ = AudioState::Connecting;
    changed = true;
  } else if (event.kind == AudioEvent::Kind::PcmProgress) {
    lastPcmAt_ = nowMs;
    if (firstPcmAt_ == 0) firstPcmAt_ = nowMs;
    changed = updateAudioReady(nowMs) || changed;
  } else if (event.kind == AudioEvent::Kind::Ended) {
    if (source_ == Source::Local) {
      // Local files loop until a new user request or standby.
      startAttempt(Source::Local, nowMs);
      return;
    }
    handleFailure(FailureKind::EndOfStream, nowMs);
    return;
  } else if (event.kind == AudioEvent::Kind::Failed) {
    handleFailure(event.failure, nowMs);
    return;
  }

  if (!soundOpened_ && audioReady_ && (anim_ == Anim::Done || displayDegraded_)) {
    openSound(nowMs);
  }
  if (changed) publish();
}

void AppController::handleDisplayDone(const DisplayDoneEvent& event,
                                      uint32_t nowMs) {
  if (!active_ || event.session != session_ || event.request != request_ ||
      event.city != city_) {
    return;
  }
  if (anim_ == Anim::Running) {
    anim_ = Anim::Done;
    displayDegraded_ = false;
  }
  updateAudioReady(nowMs);
  link_.sendAck(event.session, event.request);
  if (!soundOpened_ && audioReady_) openSound(nowMs);
  publish();
}

void AppController::handleFailure(FailureKind kind, uint32_t nowMs) {
  if (!active_ || hasError_) return;
  if (kind == FailureKind::SdMountFailed || kind == FailureKind::FileMissing ||
      kind == FailureKind::FileCorrupt) {
    enterError(nowMs);
    return;
  }
  if (source_ == Source::Local) {
    // A hard local-file failure is terminal; only a natural EOF is allowed to
    // loop. Retrying the same unreadable file would spin forever.
    enterError(nowMs);
    return;
  }
  if (!wifiConnected_) {
    sourceManager_.forceLocal();
    if (sourceManager_.stage() == SourceStage::Error) {
      enterError(nowMs);
    } else {
      startAttempt(Source::Local, nowMs);
    }
    return;
  }
  const SourceStage next = sourceManager_.advance(kind);
  if (next == SourceStage::Error) {
    enterError(nowMs);
  } else {
    startAttempt(sourceManager_.source(), nowMs);
  }
}

void AppController::enterError(uint32_t nowMs) {
  audio_.stop();
  audio_.setMuted(true);
  muted_ = true;
  audioState_ = AudioState::Error;
  source_ = Source::Local;
  hasError_ = true;
  errorAt_ = nowMs;
  soundOpened_ = false;
  audioReady_ = false;
  displayDoneDeadline_ = 0;
  if (anim_ == Anim::Running) anim_ = Anim::Done;
  publish();
}

void AppController::enterStandby(uint32_t nowMs) {
  audio_.stop();
  audio_.setMuted(true);
  active_ = false;
  muted_ = true;
  view_ = View::Standby;
  anim_ = Anim::None;
  audioState_ = AudioState::Idle;
  source_ = Source::None;
  audioReady_ = false;
  soundOpened_ = false;
  displayDegraded_ = false;
  hasError_ = false;
  firstSoundAt_ = 0;
  errorAt_ = 0;
  lastSnapshotAt_ = nowMs;
  publish();
}

void AppController::openSound(uint32_t nowMs) {
  if (!active_ || hasError_ || !audioReady_ || soundOpened_) return;
  if (anim_ != Anim::Done && !displayDegraded_) return;
  audio_.setMuted(false);
  muted_ = false;
  audioState_ = AudioState::Playing;
  soundOpened_ = true;
  if (firstSoundAt_ == 0) firstSoundAt_ = nowMs;
  publish();
}

bool AppController::standbyExpired(uint32_t nowMs) const {
  uint32_t anchor = lastUserActivityAt_;
  if (config_.standbyPolicy == StandbyPolicy::AfterInitialLoad) {
    if (firstSoundAt_ == 0 && !hasError_) return false;
    if (firstSoundAt_ != 0 &&
        static_cast<int32_t>(firstSoundAt_ - anchor) > 0) {
      anchor = firstSoundAt_;
    }
    if (hasError_ && static_cast<int32_t>(errorAt_ - anchor) > 0) {
      anchor = errorAt_;
    }
  }
  return nowMs - anchor >= config_.timing.idleTimeoutMs;
}

bool AppController::updateAudioReady(uint32_t nowMs) {
  if (audioReady_ || firstPcmAt_ == 0) return false;
  if (nowMs - firstPcmAt_ < config_.timing.pcmReadyWindowMs) return false;
  if (lastPcmAt_ == 0 ||
      nowMs - lastPcmAt_ >= config_.timing.streamStallTimeoutMs) {
    return false;
  }
  audioReady_ = true;
  audioState_ = muted_ ? AudioState::ReadyMuted : AudioState::Playing;
  return true;
}

void AppController::tick(uint32_t nowMs) {
  if (!active_) return;

  if (hasError_) {
    if (standbyExpired(nowMs)) enterStandby(nowMs);
    return;
  }

  if (firstSoundAt_ == 0 &&
      nowMs - requestStartedAt_ >= config_.timing.totalStartTimeoutMs) {
    enterError(nowMs);
    return;
  }

  if (updateAudioReady(nowMs)) publish();

  if (standbyExpired(nowMs)) {
    enterStandby(nowMs);
    return;
  }

  if (!audioReady_) {
    uint32_t timeout = config_.timing.localStartTimeoutMs;
    if (source_ == Source::Main) {
      timeout = config_.timing.mainStartTimeoutMs;
    } else if (source_ == Source::Backup) {
      timeout = config_.timing.backupStartTimeoutMs;
    }
    if (nowMs - sourceStartedAt_ >= timeout) {
      if (source_ == Source::Local) {
        enterError(nowMs);
      } else {
        const SourceStage next = sourceManager_.timeout();
        if (next == SourceStage::Error) {
          enterError(nowMs);
        } else {
          startAttempt(sourceManager_.source(), nowMs);
        }
      }
      return;
    }
  }

  if (audioReady_ && lastPcmAt_ != 0 &&
      nowMs - lastPcmAt_ >= config_.timing.streamStallTimeoutMs) {
    handleFailure(FailureKind::StreamStall, nowMs);
    return;
  }

  if (!soundOpened_ && audioReady_ &&
      (anim_ == Anim::Done || displayDegraded_)) {
    openSound(nowMs);
  }

  if (anim_ == Anim::Running && !displayDegraded_ &&
      nowMs - requestStartedAt_ >= config_.timing.displayDoneTimeoutMs) {
    displayDegraded_ = true;
    link_.markDisplayDegraded();
    publish();
    if (!soundOpened_ && audioReady_) openSound(nowMs);
  }

  if (nowMs - lastSnapshotAt_ >= config_.timing.snapshotIntervalMs) {
    publish();
  }
}

void AppController::run(uint32_t nowMs) {
  // Rotation wins over every other event in the same scheduling cycle.
  for (size_t i = 0; i < queueCount_; ++i) {
    if (queue_[i].kind == QueueKind::Rotate && !queue_[i].consumed) {
      rotate(queue_[i].delta, nowMs);
      queue_[i].consumed = true;
    }
  }

  for (size_t i = 0; i < queueCount_; ++i) {
    if (queue_[i].kind == QueueKind::WifiLost && !queue_[i].consumed) {
      handleWifiLost(nowMs);
      queue_[i].consumed = true;
    }
  }
  for (size_t i = 0; i < queueCount_; ++i) {
    if (queue_[i].kind == QueueKind::Audio && !queue_[i].consumed) {
      handleAudio(queue_[i].audio, nowMs);
      queue_[i].consumed = true;
    }
  }
  for (size_t i = 0; i < queueCount_; ++i) {
    if (queue_[i].kind == QueueKind::DisplayDone && !queue_[i].consumed) {
      handleDisplayDone(queue_[i].done, nowMs);
      queue_[i].consumed = true;
    }
  }
  for (size_t i = 0; i < queueCount_; ++i) {
    if (queue_[i].kind == QueueKind::Press && !queue_[i].consumed) {
      press(nowMs);
      queue_[i].consumed = true;
    }
  }

  size_t writeIndex = 0;
  for (size_t i = 0; i < queueCount_; ++i) {
    if (!queue_[i].consumed) {
      queue_[writeIndex++] = queue_[i];
    }
  }
  queueCount_ = writeIndex;

  tick(nowMs);
}

void AppController::publish() {
  snapshot_.session = session_;
  snapshot_.request = request_;
  snapshot_.city = city_;
  snapshot_.view = view_;
  snapshot_.anim = anim_;
  snapshot_.audio = audioState_;
  snapshot_.source = source_;
  link_.sendSnapshot(snapshot_);
  lastSnapshotAt_ = clock_.nowMs();
}

}  // namespace radio
