#include "radio_types.h"

#include <string.h>

namespace radio {

const char* viewName(View value) {
  switch (value) {
    case View::Standby:
      return "STANDBY";
    case View::Map:
      return "MAP";
    case View::Info:
      return "INFO";
  }
  return "STANDBY";
}

const char* animName(Anim value) {
  switch (value) {
    case Anim::None:
      return "NONE";
    case Anim::Running:
      return "RUNNING";
    case Anim::Done:
      return "DONE";
  }
  return "NONE";
}

const char* audioName(AudioState value) {
  switch (value) {
    case AudioState::Idle:
      return "IDLE";
    case AudioState::Connecting:
      return "CONNECTING";
    case AudioState::Buffering:
      return "BUFFERING";
    case AudioState::ReadyMuted:
      return "READY";
    case AudioState::Playing:
      return "PLAYING";
    case AudioState::Error:
      return "ERROR";
  }
  return "IDLE";
}

const char* sourceName(Source value) {
  switch (value) {
    case Source::None:
      return "NONE";
    case Source::Main:
      return "MAIN";
    case Source::Backup:
      return "BACKUP";
    case Source::Local:
      return "LOCAL";
  }
  return "NONE";
}

bool parseView(const char* text, View& value) {
  if (text == nullptr) return false;
  if (strcmp(text, "STANDBY") == 0) {
    value = View::Standby;
    return true;
  }
  if (strcmp(text, "MAP") == 0) {
    value = View::Map;
    return true;
  }
  if (strcmp(text, "INFO") == 0) {
    value = View::Info;
    return true;
  }
  return false;
}

bool parseAnim(const char* text, Anim& value) {
  if (text == nullptr) return false;
  if (strcmp(text, "NONE") == 0) {
    value = Anim::None;
    return true;
  }
  if (strcmp(text, "RUNNING") == 0) {
    value = Anim::Running;
    return true;
  }
  if (strcmp(text, "DONE") == 0) {
    value = Anim::Done;
    return true;
  }
  return false;
}

bool parseAudio(const char* text, AudioState& value) {
  if (text == nullptr) return false;
  if (strcmp(text, "IDLE") == 0) {
    value = AudioState::Idle;
    return true;
  }
  if (strcmp(text, "CONNECTING") == 0) {
    value = AudioState::Connecting;
    return true;
  }
  if (strcmp(text, "BUFFERING") == 0) {
    value = AudioState::Buffering;
    return true;
  }
  if (strcmp(text, "READY") == 0) {
    value = AudioState::ReadyMuted;
    return true;
  }
  if (strcmp(text, "PLAYING") == 0) {
    value = AudioState::Playing;
    return true;
  }
  if (strcmp(text, "ERROR") == 0) {
    value = AudioState::Error;
    return true;
  }
  return false;
}

bool parseSource(const char* text, Source& value) {
  if (text == nullptr) return false;
  if (strcmp(text, "NONE") == 0) {
    value = Source::None;
    return true;
  }
  if (strcmp(text, "MAIN") == 0) {
    value = Source::Main;
    return true;
  }
  if (strcmp(text, "BACKUP") == 0) {
    value = Source::Backup;
    return true;
  }
  if (strcmp(text, "LOCAL") == 0) {
    value = Source::Local;
    return true;
  }
  return false;
}

const char* failureName(FailureKind value) {
  switch (value) {
    case FailureKind::None:
      return "NONE";
    case FailureKind::ConnectRefused:
      return "CONNECT_REFUSED";
    case FailureKind::InvalidUrl:
      return "INVALID_URL";
    case FailureKind::HttpStatus:
      return "HTTP_STATUS";
    case FailureKind::UnsupportedCodec:
      return "UNSUPPORTED_CODEC";
    case FailureKind::DecoderFatal:
      return "DECODER_FATAL";
    case FailureKind::StreamStall:
      return "STREAM_STALL";
    case FailureKind::EndOfStream:
      return "END_OF_STREAM";
    case FailureKind::SdMountFailed:
      return "SD_MOUNT_FAILED";
    case FailureKind::FileMissing:
      return "FILE_MISSING";
    case FailureKind::FileCorrupt:
      return "FILE_CORRUPT";
    case FailureKind::Internal:
      return "INTERNAL";
  }
  return "NONE";
}

}  // namespace radio
