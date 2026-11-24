#pragma once

#include <stddef.h>
#include <stdint.h>

#include "radio_types.h"

namespace radio {

// Checks mount/file presence/corruption only. Decoding remains in the player.
class SdAudioCatalog {
 public:
  enum class Status : uint8_t { Ok = 0, NotMounted, MissingFile, CorruptFile };

  void setMounted(bool mounted) { mounted_ = mounted; }
  void setFileState(const char* path, bool present, bool corrupt);
  Status check(const City& city) const;
  bool mounted() const { return mounted_; }

 private:
  struct Entry {
    char path[64];
    bool present = false;
    bool corrupt = false;
  };

  bool mounted_ = false;
  Entry entries_[8];
  size_t entryCount_ = 0;
};

}  // namespace radio
