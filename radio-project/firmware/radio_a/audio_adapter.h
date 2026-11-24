#pragma once

#include <stdint.h>

#include "app_controller.h"
#include "clock.h"

namespace radio {

// Hardware-facing playback boundary. The adapter is the only owner that calls
// this interface; the PCM hook only advances an atomic counter.
class AudioBackend {
 public:
  virtual ~AudioBackend() = default;
  virtual void startOnline(const char* url) = 0;
  virtual void startFile(const char* path) = 0;
  virtual void stop() = 0;
  virtual void setMuted(bool muted) = 0;
  virtual bool isStopped() = 0;
  virtual uint32_t pcmProgressCount() = 0;
  virtual bool takeFailure(FailureKind& kind) = 0;
  virtual bool takeEnded() = 0;
  virtual void loop() = 0;
};

// Serializes start/stop/mute and isolates callbacks by generation. A new start
// is queued until the previous backend has stopped and the callback stream has
// been quiet for the configured quiescence window.
class AudioAdapter : public AudioAdapterInterface {
 public:
  struct Config {
    uint32_t quiescenceMs = 50;
    bool hardwareMuteStopsPcm = false;
  };

  AudioAdapter(AudioBackend& backend, Clock& clock, const Config& config);

  void begin(uint32_t nowMs);
  void start(const AudioStartRequest& request) override;
  void stop() override;
  void setMuted(bool muted) override;
  void loop();
  bool pollEvent(AudioEvent& event);

  bool muted() const { return muted_; }
  bool busy() const { return !quiesced_ || pending_; }
  const AudioStartRequest& current() const { return current_; }
  uint32_t droppedCallbacks() const { return droppedCallbacks_; }

 private:
  void applyPending(uint32_t nowMs);
  void emit(const AudioEvent& event);

  AudioBackend& backend_;
  Clock& clock_;
  Config config_{};
  AudioStartRequest current_{};
  AudioStartRequest pendingRequest_{};
  bool pending_ = false;
  bool quiesced_ = true;
  bool stopping_ = false;
  bool acceptCallbacks_ = false;
  bool muted_ = true;
  uint32_t quiesceStartAt_ = 0;
  uint32_t lastCallbackAt_ = 0;
  uint32_t lastPcmCount_ = 0;
  uint32_t droppedCallbacks_ = 0;
  AudioEvent events_[8];
  size_t eventCount_ = 0;
};

}  // namespace radio
