#include "source_manager.h"

namespace radio {

void SourceManager::begin(bool wifiAvailable) {
  stage_ = wifiAvailable ? SourceStage::Main : SourceStage::Local;
}

SourceStage SourceManager::advance(FailureKind kind) {
  if (kind == FailureKind::SdMountFailed || kind == FailureKind::FileMissing ||
      kind == FailureKind::FileCorrupt) {
    stage_ = SourceStage::Error;
    return stage_;
  }
  switch (stage_) {
    case SourceStage::Idle:
    case SourceStage::Main:
      stage_ = SourceStage::Backup;
      break;
    case SourceStage::Backup:
      stage_ = SourceStage::Local;
      break;
    case SourceStage::Local:
    case SourceStage::Error:
      stage_ = SourceStage::Error;
      break;
  }
  return stage_;
}

SourceStage SourceManager::forceLocal() {
  if (stage_ == SourceStage::Error) return stage_;
  stage_ = SourceStage::Local;
  return stage_;
}

SourceStage SourceManager::timeout() {
  if (stage_ == SourceStage::Main) {
    stage_ = SourceStage::Backup;
  } else if (stage_ == SourceStage::Backup) {
    stage_ = SourceStage::Local;
  } else {
    stage_ = SourceStage::Error;
  }
  return stage_;
}

Source SourceManager::source() const {
  switch (stage_) {
    case SourceStage::Main:
      return Source::Main;
    case SourceStage::Backup:
      return Source::Backup;
    case SourceStage::Local:
      return Source::Local;
    case SourceStage::Error:
      return Source::Local;
    case SourceStage::Idle:
      break;
  }
  return Source::None;
}

}  // namespace radio
