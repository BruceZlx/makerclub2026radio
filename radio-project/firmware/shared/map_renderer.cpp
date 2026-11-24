#include "map_renderer.h"

#include <cmath>

namespace radio {

namespace {

float wrapLongitude(float longitude) {
  while (longitude > 180.0f) longitude -= 360.0f;
  while (longitude < -180.0f) longitude += 360.0f;
  return longitude;
}

}  // namespace

MapPoint MapRenderer::project(GeoPoint point) const {
  const float longitude = static_cast<float>(point.longitudeMilli) / 1000.0f;
  const float latitude = static_cast<float>(point.latitudeMilli) / 1000.0f;
  MapPoint mapped;
  mapped.x = (longitude + 180.0f) / 360.0f * static_cast<float>(kLogicalWidth);
  mapped.y = (90.0f - latitude) / 180.0f * static_cast<float>(kLogicalHeight);
  return mapped;
}

bool MapRenderer::crossesDateline(GeoPoint from, GeoPoint to) const {
  const float delta = static_cast<float>(to.longitudeMilli - from.longitudeMilli) / 1000.0f;
  return delta > 180.0f || delta < -180.0f;
}

float MapRenderer::easeOutCubic(float value) {
  if (value <= 0.0f) return 0.0f;
  if (value >= 1.0f) return 1.0f;
  const float inverse = 1.0f - value;
  return 1.0f - inverse * inverse * inverse;
}

MapPoint MapRenderer::pointAt(GeoPoint from, GeoPoint to, float progress) const {
  const float t = easeOutCubic(progress);
  const float fromLon = static_cast<float>(from.longitudeMilli) / 1000.0f;
  const float fromLat = static_cast<float>(from.latitudeMilli) / 1000.0f;
  float toLon = static_cast<float>(to.longitudeMilli) / 1000.0f;
  const float toLat = static_cast<float>(to.latitudeMilli) / 1000.0f;

  float delta = toLon - fromLon;
  if (delta > 180.0f) delta -= 360.0f;
  if (delta < -180.0f) delta += 360.0f;
  toLon = fromLon + delta;

  const float midLon = (fromLon + toLon) * 0.5f;
  const float midLat = (fromLat + toLat) * 0.5f;
  const float lift = 8.0f + (std::fabs(delta) / 360.0f) * 18.0f;
  const float controlLon = midLon;
  const float controlLat = midLat + lift;

  const float inverse = 1.0f - t;
  const float lon = inverse * inverse * fromLon +
                    2.0f * inverse * t * controlLon + t * t * toLon;
  const float lat = inverse * inverse * fromLat +
                    2.0f * inverse * t * controlLat + t * t * toLat;

  GeoPoint sample;
  sample.longitudeMilli = static_cast<int32_t>(wrapLongitude(lon) * 1000.0f);
  sample.latitudeMilli = static_cast<int32_t>(lat * 1000.0f);
  return project(sample);
}

std::vector<ArcPoint> MapRenderer::buildArc(GeoPoint from, GeoPoint to,
                                           uint8_t steps) const {
  std::vector<ArcPoint> points;
  if (steps == 0) steps = 1;
  points.reserve(steps + 1);

  const float fromLon = static_cast<float>(from.longitudeMilli) / 1000.0f;
  float toLon = static_cast<float>(to.longitudeMilli) / 1000.0f;
  float delta = toLon - fromLon;
  if (delta > 180.0f) delta -= 360.0f;
  if (delta < -180.0f) delta += 360.0f;
  toLon = fromLon + delta;

  const float fromLat = static_cast<float>(from.latitudeMilli) / 1000.0f;
  const float toLat = static_cast<float>(to.latitudeMilli) / 1000.0f;
  const float midLon = (fromLon + toLon) * 0.5f;
  const float midLat = (fromLat + toLat) * 0.5f;
  const float lift = 8.0f + (std::fabs(delta) / 360.0f) * 18.0f;
  const float controlLon = midLon;
  const float controlLat = midLat + lift;

  float previousWrapped = 0.0f;
  bool hasPrevious = false;
  for (uint8_t i = 0; i <= steps; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(steps);
    const float inverse = 1.0f - t;
    const float lon = inverse * inverse * fromLon +
                      2.0f * inverse * t * controlLon + t * t * toLon;
    const float lat = inverse * inverse * fromLat +
                      2.0f * inverse * t * controlLat + t * t * toLat;
    const float wrapped = wrapLongitude(lon);
    ArcPoint point;
    point.breakBefore = hasPrevious && std::fabs(wrapped - previousWrapped) > 180.0f;
    GeoPoint sample;
    sample.longitudeMilli = static_cast<int32_t>(wrapped * 1000.0f);
    sample.latitudeMilli = static_cast<int32_t>(lat * 1000.0f);
    const MapPoint mapped = project(sample);
    point.x = mapped.x;
    point.y = mapped.y;
    points.push_back(point);
    previousWrapped = wrapped;
    hasPrevious = true;
  }
  return points;
}

}  // namespace radio
