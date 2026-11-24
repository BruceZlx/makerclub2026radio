#pragma once

#include <stddef.h>
#include <stdint.h>

namespace radio {

// All timing values are milliseconds. They live in one place so host tests and
// firmware use exactly the same constants.
struct TimingConfig {
  uint32_t mainStartTimeoutMs = 15000;
  uint32_t backupStartTimeoutMs = 15000;
  uint32_t localStartTimeoutMs = 2000;
  uint32_t totalStartTimeoutMs = 32000;
  uint32_t streamStallTimeoutMs = 10000;
  uint32_t idleTimeoutMs = 20000;
  uint32_t pcmReadyWindowMs = 200;
  uint32_t arcDurationMs = 1500;
  uint32_t displayDoneTimeoutMs = 3500;
  uint32_t snapshotIntervalMs = 1000;
  uint32_t doneRetryIntervalMs = 250;
  uint8_t doneMaxRetries = 8;
};

enum class View : uint8_t { Standby = 0, Map = 1, Info = 2 };
enum class Anim : uint8_t { None = 0, Running = 1, Done = 2 };
enum class AudioState : uint8_t {
  Idle = 0,
  Connecting = 1,
  Buffering = 2,
  ReadyMuted = 3,
  Playing = 4,
  Error = 5
};
enum class Source : uint8_t { None = 0, Main = 1, Backup = 2, Local = 3 };

enum class FailureKind : uint8_t {
  None = 0,
  ConnectRefused,
  InvalidUrl,
  HttpStatus,
  UnsupportedCodec,
  DecoderFatal,
  StreamStall,
  EndOfStream,
  SdMountFailed,
  FileMissing,
  FileCorrupt,
  Internal
};

struct GeoPoint {
  int32_t longitudeMilli = 0;
  int32_t latitudeMilli = 0;
};

struct City {
  uint8_t id = 0;
  const char* code = "";
  const char* name = "";
  int32_t longitudeMilli = 0;
  int32_t latitudeMilli = 0;
  const char* primaryUrl = "";
  const char* backupUrl = "";
  const char* primaryStation = "";
  const char* backupStation = "";
  const char* offlinePath = "";
  const char* offlineLabel = "";
  const char* timezoneRule = "";
  const char* copy = "";
  uint8_t displayOrder = 0;
};

struct CityCatalog {
  const City* cities = nullptr;
  size_t count = 0;
  const char* configId = "00000000";
  bool complete = false;
  GeoPoint origin{121474, 31230};  // Shanghai, fixed logical-map origin.

  const City* byId(uint8_t id) const;
  const City* byDisplayOrder(uint8_t order) const;
  bool validate() const;
  uint8_t cityCount() const { return static_cast<uint8_t>(count); }
};

struct Snapshot {
  uint16_t session = 0;
  uint32_t request = 0;
  uint8_t city = 0;
  View view = View::Standby;
  Anim anim = Anim::None;
  AudioState audio = AudioState::Idle;
  Source source = Source::None;
};

struct AudioEvent {
  enum class Kind : uint8_t { Started, PcmProgress, Ended, Failed };
  Kind kind = Kind::Started;
  uint16_t session = 0;
  uint32_t request = 0;
  uint32_t attempt = 0;
  uint8_t city = 0;
  FailureKind failure = FailureKind::None;
  uint32_t sampleCount = 0;
};

struct DisplayDoneEvent {
  uint16_t session = 0;
  uint32_t request = 0;
  uint8_t city = 0;
};

struct AudioStartRequest {
  uint16_t session = 0;
  uint32_t request = 0;
  uint32_t attempt = 0;
  uint8_t city = 0;
  Source source = Source::None;
  const char* url = "";
  const char* path = "";
};

enum class StandbyPolicy : uint8_t { AfterInitialLoad = 0, LastUserInput = 1 };

const char* viewName(View value);
const char* animName(Anim value);
const char* audioName(AudioState value);
const char* sourceName(Source value);
bool parseView(const char* text, View& value);
bool parseAnim(const char* text, Anim& value);
bool parseAudio(const char* text, AudioState& value);
bool parseSource(const char* text, Source& value);
const char* failureName(FailureKind value);

}  // namespace radio
