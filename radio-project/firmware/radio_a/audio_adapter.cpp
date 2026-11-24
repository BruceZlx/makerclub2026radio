#include "audio_adapter.h"

namespace radio {

AudioAdapter::AudioAdapter(AudioBackend& backend, Clock& clock,
                           const Config& config)
    : backend_(backend), clock_(clock), config_(config) {}

void AudioAdapter::begin(uint32_t nowMs) {
  current_ = AudioStartRequest{};
  pending_ = false;
  quiesced_ = true;
  stopping_ = false;
  acceptCallbacks_ = false;
  muted_ = true;
  lastPcmCount_ = backend_.pcmProgressCount();
  lastCallbackAt_ = nowMs;
  eventCount_ = 0;
  backend_.setMuted(true);
}

void AudioAdapter::start(const AudioStartRequest& request) {
  pendingRequest_ = request;
  pending_ = true;
  acceptCallbacks_ = false;
  if (quiesced_ && !stopping_) {
    applyPending(clock_.nowMs());
  }
}

void AudioAdapter::applyPending(uint32_t nowMs) {
  if (!pending_) return;
  // Drain any failure/EOF left by the previous generation before the new
  // generation becomes the callback owner.
  FailureKind staleFailure = FailureKind::None;
  while (backend_.takeFailure(staleFailure)) {
  }
  while (backend_.takeEnded()) {
  }
  current_ = pendingRequest_;
  pending_ = false;
  quiesced_ = false;
  stopping_ = false;
  acceptCallbacks_ = true;
  lastPcmCount_ = backend_.pcmProgressCount();
  lastCallbackAt_ = nowMs;
  if (current_.source == Source::Local) {
    backend_.startFile(current_.path);
  } else {
    backend_.startOnline(current_.url);
  }
  AudioEvent event;
  event.kind = AudioEvent::Kind::Started;
  event.session = current_.session;
  event.request = current_.request;
  event.attempt = current_.attempt;
  event.city = current_.city;
  emit(event);
}

void AudioAdapter::stop() {
  pending_ = false;
  acceptCallbacks_ = false;
  backend_.stop();
  stopping_ = true;
  quiesceStartAt_ = clock_.nowMs();
  current_ = AudioStartRequest{};
  eventCount_ = 0;
}

void AudioAdapter::setMuted(bool muted) {
  muted_ = muted;
  backend_.setMuted(muted);
}

void AudioAdapter::loop() { backend_.loop(); }

bool AudioAdapter::pollEvent(AudioEvent& event) {
  const uint32_t nowMs = clock_.nowMs();
  if (stopping_ && backend_.isStopped() &&
      nowMs - lastCallbackAt_ >= config_.quiescenceMs) {
    stopping_ = false;
    quiesced_ = true;
    if (pending_) applyPending(nowMs);
  }

  FailureKind failure = FailureKind::None;
  if (acceptCallbacks_ && backend_.takeFailure(failure)) {
    AudioEvent failed;
    failed.kind = AudioEvent::Kind::Failed;
    failed.session = current_.session;
    failed.request = current_.request;
    failed.attempt = current_.attempt;
    failed.city = current_.city;
    failed.failure = failure;
    emit(failed);
  }
  if (acceptCallbacks_ && backend_.takeEnded()) {
    AudioEvent ended;
    ended.kind = AudioEvent::Kind::Ended;
    ended.session = current_.session;
    ended.request = current_.request;
    ended.attempt = current_.attempt;
    ended.city = current_.city;
    emit(ended);
  }

  const uint32_t progress = backend_.pcmProgressCount();
  if (progress != lastPcmCount_) {
    lastPcmCount_ = progress;
    lastCallbackAt_ = nowMs;
    if (acceptCallbacks_) {
      AudioEvent event;
      event.kind = AudioEvent::Kind::PcmProgress;
      event.session = current_.session;
      event.request = current_.request;
      event.attempt = current_.attempt;
      event.city = current_.city;
      event.sampleCount = progress;
      emit(event);
    }
  }

  if (eventCount_ == 0) return false;
  event = events_[0];
  for (size_t i = 1; i < eventCount_; ++i) events_[i - 1] = events_[i];
  --eventCount_;
  return true;
}

void AudioAdapter::emit(const AudioEvent& event) {
  if (eventCount_ >= sizeof(events_) / sizeof(events_[0])) {
    ++droppedCallbacks_;
    return;
  }
  events_[eventCount_++] = event;
}

}  // namespace radio
