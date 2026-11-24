#pragma once

#include <stdint.h>

#include "city_catalog.h"
#include "clock.h"
#include "identity.h"
#include "radio_types.h"
#include "source_manager.h"

namespace radio {

class AudioAdapterInterface {
 public:
  virtual ~AudioAdapterInterface() = default;
  virtual void start(const AudioStartRequest& request) = 0;
  virtual void stop() = 0;
  virtual void setMuted(bool muted) = 0;
};

class DisplayLinkInterface {
 public:
  virtual ~DisplayLinkInterface() = default;
  virtual void setSession(uint16_t session) = 0;
  virtual void sendSnapshot(const Snapshot& snapshot) = 0;
  virtual void resendSnapshot() = 0;
  virtual void sendAck(uint16_t session, uint32_t request) = 0;
  virtual void markDisplayDegraded() = 0;
};

class AppController {
 public:
  struct Config {
    TimingConfig timing;
    StandbyPolicy standbyPolicy = StandbyPolicy::AfterInitialLoad;
    uint8_t firstCity = 0;
    uint16_t initialSession = 0;  // 0 = derive from begin() time
  };

  AppController(Clock& clock, const CityCatalog& catalog,
                AudioAdapterInterface& audio, DisplayLinkInterface& link,
                const Config& config);

  void begin(uint32_t nowMs);
  void queueRotate(int8_t delta);
  void queuePress();
  void queueWifiLost();
  void queueAudio(const AudioEvent& event);
  void queueDisplayDone(const DisplayDoneEvent& event);

  // Drains queued events with rotation priority, then evaluates timeouts.
  void run(uint32_t nowMs);

  const Snapshot& snapshot() const { return snapshot_; }
  bool active() const { return active_; }
  bool audioReady() const { return audioReady_; }
  bool soundOpened() const { return soundOpened_; }
  bool hasError() const { return hasError_; }
  bool displayDegraded() const { return displayDegraded_; }
  uint32_t droppedEvents() const { return droppedEvents_; }
  uint32_t request() const { return request_; }
  uint32_t attempt() const { return attempt_; }
  uint16_t session() const { return session_; }
  uint8_t city() const { return city_; }
  uint32_t firstSoundAt() const { return firstSoundAt_; }
  Source source() const { return source_; }
  View view() const { return view_; }
  Anim anim() const { return anim_; }
  AudioState audioState() const { return audioState_; }

  // The main loop calls this from its Wi-Fi status poll. A confirmed loss is
  // queued so the next run() applies it after any rotation in the same cycle.
  void setWifiConnected(bool connected);

 private:
  enum class QueueKind : uint8_t { Rotate, Press, WifiLost, Audio, DisplayDone };
  struct QueueEntry {
    QueueKind kind = QueueKind::Rotate;
    int8_t delta = 0;
    AudioEvent audio{};
    DisplayDoneEvent done{};
    bool consumed = false;
  };

  void rotate(int8_t delta, uint32_t nowMs);
  void press(uint32_t nowMs);
  void handleWifiLost(uint32_t nowMs);
  void handleAudio(const AudioEvent& event, uint32_t nowMs);
  void handleDisplayDone(const DisplayDoneEvent& event, uint32_t nowMs);
  void handleFailure(FailureKind kind, uint32_t nowMs);
  void startAttempt(Source source, uint32_t nowMs);
  void enterError(uint32_t nowMs);
  void enterStandby(uint32_t nowMs);
  void openSound(uint32_t nowMs);
  void tick(uint32_t nowMs);
  bool standbyExpired(uint32_t nowMs) const;
  bool updateAudioReady(uint32_t nowMs);
  void push(const QueueEntry& entry);
  void publish();
  uint8_t wrapCity(int8_t delta) const;

  Clock& clock_;
  const CityCatalog& catalog_;
  AudioAdapterInterface& audio_;
  DisplayLinkInterface& link_;
  Config config_{};
  Identity identity_;
  SourceManager sourceManager_;

  bool active_ = false;
  uint16_t session_ = 1;
  uint32_t request_ = 0;
  uint32_t attempt_ = 0;
  uint8_t city_ = 0;
  View view_ = View::Standby;
  Anim anim_ = Anim::None;
  AudioState audioState_ = AudioState::Idle;
  Source source_ = Source::None;
  Snapshot snapshot_{};

  bool wifiConnected_ = false;
  bool muted_ = true;
  bool audioReady_ = false;
  bool soundOpened_ = false;
  bool displayDegraded_ = false;
  bool hasError_ = false;

  uint32_t requestStartedAt_ = 0;
  uint32_t sourceStartedAt_ = 0;
  uint32_t lastUserActivityAt_ = 0;
  uint32_t firstPcmAt_ = 0;
  uint32_t lastPcmAt_ = 0;
  uint32_t firstSoundAt_ = 0;
  uint32_t errorAt_ = 0;
  uint32_t displayDoneDeadline_ = 0;
  uint32_t lastSnapshotAt_ = 0;
  uint32_t lastActivityEventAt_ = 0;

  QueueEntry queue_[24];
  size_t queueCount_ = 0;
  uint32_t droppedEvents_ = 0;
};

}  // namespace radio
