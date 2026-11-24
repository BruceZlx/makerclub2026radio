#pragma once

#include <stdint.h>

#include <vector>

#include "radio_types.h"

namespace radio {

struct MapPoint {
  float x = 0;
  float y = 0;
};

struct ArcPoint {
  float x = 0;
  float y = 0;
  bool breakBefore = false;
};

class MapRenderer {
 public:
  static constexpr uint32_t kLogicalWidth = 1000;
  static constexpr uint32_t kLogicalHeight = 500;
  static constexpr uint8_t kDefaultArcSteps = 48;

  MapPoint project(GeoPoint point) const;
  bool crossesDateline(GeoPoint from, GeoPoint to) const;
  std::vector<ArcPoint> buildArc(GeoPoint from, GeoPoint to,
                                 uint8_t steps = kDefaultArcSteps) const;
  MapPoint pointAt(GeoPoint from, GeoPoint to, float progress) const;
  static float easeOutCubic(float value);
};

}  // namespace radio
