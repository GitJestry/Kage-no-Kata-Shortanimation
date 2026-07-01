#pragma once

#include "math/bounds.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <limits>

namespace kage::math {

struct ScreenPoint final {
  glm::vec2 pixel{0.0f};
  float depth = 1.0f;
  bool valid = false;
};

struct ScreenBounds final {
  glm::vec2 min{std::numeric_limits<float>::max()};
  glm::vec2 max{std::numeric_limits<float>::lowest()};
  float depth = 1.0f;
  bool valid = false;
};

[[nodiscard]] inline ScreenPoint projectPoint(
    const glm::vec3& parPoint, const glm::mat4& parViewProjection,
    const glm::vec2& parViewportSize) {
  const glm::vec4 clip = parViewProjection * glm::vec4(parPoint, 1.0f);
  if (clip.w <= 0.0001f) {
    return {};
  }

  const glm::vec3 ndc = glm::vec3(clip) / clip.w;
  ScreenPoint point;
  point.pixel = glm::vec2((ndc.x * 0.5f + 0.5f) * parViewportSize.x,
                          (1.0f - (ndc.y * 0.5f + 0.5f)) *
                              parViewportSize.y);
  point.depth = ndc.z;
  point.valid = ndc.z >= -1.0f && ndc.z <= 1.0f;
  return point;
}

[[nodiscard]] inline ScreenBounds projectBounds(
    const Bounds3& parBounds, const glm::mat4& parViewProjection,
    const glm::vec2& parViewportSize) {
  ScreenBounds screen_bounds;
  if (!parBounds.is_valid) {
    return screen_bounds;
  }

  const glm::vec3 min = parBounds.min;
  const glm::vec3 max = parBounds.max;
  const std::array<glm::vec3, 8> corners = {
      glm::vec3(min.x, min.y, min.z), glm::vec3(max.x, min.y, min.z),
      glm::vec3(min.x, max.y, min.z), glm::vec3(max.x, max.y, min.z),
      glm::vec3(min.x, min.y, max.z), glm::vec3(max.x, min.y, max.z),
      glm::vec3(min.x, max.y, max.z), glm::vec3(max.x, max.y, max.z),
  };

  for (const glm::vec3& corner : corners) {
    const ScreenPoint point =
        projectPoint(corner, parViewProjection, parViewportSize);
    if (!point.valid) {
      continue;
    }

    screen_bounds.min = glm::min(screen_bounds.min, point.pixel);
    screen_bounds.max = glm::max(screen_bounds.max, point.pixel);
    screen_bounds.depth = std::min(screen_bounds.depth, point.depth);
    screen_bounds.valid = true;
  }

  return screen_bounds;
}

[[nodiscard]] inline bool containsWithPadding(const ScreenBounds& parBounds,
                                              const glm::vec2& parPixel,
                                              float parPadding) {
  if (!parBounds.valid) {
    return false;
  }

  return parPixel.x >= parBounds.min.x - parPadding &&
         parPixel.x <= parBounds.max.x + parPadding &&
         parPixel.y >= parBounds.min.y - parPadding &&
         parPixel.y <= parBounds.max.y + parPadding;
}

[[nodiscard]] inline float distanceToSegment(const glm::vec2& parPoint,
                                             const glm::vec2& parStart,
                                             const glm::vec2& parEnd) {
  const glm::vec2 segment = parEnd - parStart;
  const float length_squared = glm::dot(segment, segment);
  if (length_squared <= 0.0001f) {
    return glm::length(parPoint - parStart);
  }

  const float t =
      std::clamp(glm::dot(parPoint - parStart, segment) / length_squared,
                 0.0f, 1.0f);
  return glm::length(parPoint - (parStart + segment * t));
}

}  // namespace kage::math
