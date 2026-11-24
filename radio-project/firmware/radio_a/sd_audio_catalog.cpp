#include "sd_audio_catalog.h"

#include <string.h>

namespace radio {

void SdAudioCatalog::setFileState(const char* path, bool present, bool corrupt) {
  if (path == nullptr || path[0] == '\0') return;
  for (size_t i = 0; i < entryCount_; ++i) {
    if (strcmp(entries_[i].path, path) == 0) {
      entries_[i].present = present;
      entries_[i].corrupt = corrupt;
      return;
    }
  }
  if (entryCount_ >= sizeof(entries_) / sizeof(entries_[0])) return;
  Entry& entry = entries_[entryCount_++];
  strncpy(entry.path, path, sizeof(entry.path) - 1);
  entry.path[sizeof(entry.path) - 1] = '\0';
  entry.present = present;
  entry.corrupt = corrupt;
}

SdAudioCatalog::Status SdAudioCatalog::check(const City& city) const {
  if (!mounted_) return Status::NotMounted;
  if (city.offlinePath == nullptr || city.offlinePath[0] == '\0') {
    return Status::MissingFile;
  }
  for (size_t i = 0; i < entryCount_; ++i) {
    if (strcmp(entries_[i].path, city.offlinePath) == 0) {
      if (!entries_[i].present) return Status::MissingFile;
      if (entries_[i].corrupt) return Status::CorruptFile;
      return Status::Ok;
    }
  }
  return Status::MissingFile;
}

}  // namespace radio
