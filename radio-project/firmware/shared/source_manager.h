#pragma once

#include "radio_types.h"

namespace radio {

enum class SourceStage : uint8_t { Idle = 0, Main = 1, Backup = 2, Local = 3, Error = 4 };

// One-way source policy: MAIN -> BACKUP -> LOCAL -> ERROR. It never loops back
// to MAIN within the same request.
class SourceManager {
 public:
  void begin(bool wifiAvailable);
  SourceStage advance(FailureKind kind);
  SourceStage forceLocal();
  SourceStage timeout();

  SourceStage stage() const { return stage_; }
  Source source() const;
  bool failed() const { return stage_ == SourceStage::Error; }

 private:
  SourceStage stage_ = SourceStage::Idle;
};

}  // namespace radio
