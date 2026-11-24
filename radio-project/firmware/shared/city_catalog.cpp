#include "city_catalog.h"

#include <string.h>

namespace radio {

const City* CityCatalog::byId(uint8_t id) const {
  if (cities == nullptr) return nullptr;
  for (size_t i = 0; i < count; ++i) {
    if (cities[i].id == id) return &cities[i];
  }
  return nullptr;
}

const City* CityCatalog::byDisplayOrder(uint8_t order) const {
  if (cities == nullptr) return nullptr;
  for (size_t i = 0; i < count; ++i) {
    if (cities[i].displayOrder == order) return &cities[i];
  }
  return nullptr;
}

bool CityCatalog::validate() const {
  if (!complete) return false;
  if (cities == nullptr || count != 8) return false;
  bool seenId[8] = {false};
  bool seenOrder[8] = {false};
  for (size_t i = 0; i < count; ++i) {
    const City& city = cities[i];
    if (city.id >= 8 || seenId[city.id]) return false;
    seenId[city.id] = true;
    if (city.displayOrder >= 8 || seenOrder[city.displayOrder]) return false;
    seenOrder[city.displayOrder] = true;
    if (city.code == nullptr || city.code[0] == '\0') return false;
    if (city.name == nullptr || city.name[0] == '\0') return false;
    if (city.longitudeMilli < -180000 || city.longitudeMilli > 180000) return false;
    if (city.latitudeMilli < -90000 || city.latitudeMilli > 90000) return false;
    if (city.primaryUrl == nullptr || city.primaryUrl[0] == '\0') return false;
    if (city.backupUrl == nullptr || city.backupUrl[0] == '\0') return false;
    if (city.primaryStation == nullptr || city.primaryStation[0] == '\0') return false;
    if (city.backupStation == nullptr || city.backupStation[0] == '\0') return false;
    if (city.offlinePath == nullptr || city.offlinePath[0] == '\0') return false;
    if (city.offlineLabel == nullptr || city.offlineLabel[0] == '\0') return false;
    if (city.timezoneRule == nullptr || city.timezoneRule[0] == '\0') return false;
    if (city.copy == nullptr || city.copy[0] == '\0') return false;
    for (size_t j = 0; j < i; ++j) {
      if (strcmp(cities[j].code, city.code) == 0) return false;
    }
  }
  return configId != nullptr && strlen(configId) == 8;
}

}  // namespace radio
