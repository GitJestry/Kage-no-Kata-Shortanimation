#pragma once

#include <glm/glm.hpp>
#include <array>
#include <algorithm>

namespace kage::math {

struct Bounds3 final {
  glm::vec3 min{};
  glm::vec3 max{};
  bool is_valid = false;

  void includePoint(const glm::vec3& parPoint);
  [[nodiscard]] glm::vec3 getSize() const;
};

inline void Bounds3::includePoint(const glm::vec3& parPoint) {
  if (!is_valid) {
    min = parPoint;
    max = parPoint;
    is_valid = true;
    return;
  }

  min = glm::min(min, parPoint);
  max = glm::max(max, parPoint);
}

inline glm::vec3 Bounds3::getSize() const {
  if (!is_valid) {
    return glm::vec3(0.0f);
  }

  return max - min;
}

inline Bounds3 makePointBounds(const glm::vec3& parPoint, float parExtent) {
  Bounds3 bounds;
  const glm::vec3 extent(std::max(parExtent, 0.01f));
  bounds.includePoint(parPoint - extent);
  bounds.includePoint(parPoint + extent);
  return bounds;
}

inline Bounds3 makeAssetPlaceholderBounds() {
  Bounds3 bounds;
  bounds.includePoint(glm::vec3(-0.5f, 0.0f, -0.5f));
  bounds.includePoint(glm::vec3(0.5f, 1.0f, 0.5f));
  return bounds;
}

[[nodiscard]] inline std::array<glm::vec3, 8> boundsCorners(
    const Bounds3& parBounds) {
  return {
      glm::vec3(parBounds.min.x, parBounds.min.y, parBounds.min.z),
      glm::vec3(parBounds.max.x, parBounds.min.y, parBounds.min.z),
      glm::vec3(parBounds.min.x, parBounds.max.y, parBounds.min.z),
      glm::vec3(parBounds.max.x, parBounds.max.y, parBounds.min.z),
      glm::vec3(parBounds.min.x, parBounds.min.y, parBounds.max.z),
      glm::vec3(parBounds.max.x, parBounds.min.y, parBounds.max.z),
      glm::vec3(parBounds.min.x, parBounds.max.y, parBounds.max.z),
      glm::vec3(parBounds.max.x, parBounds.max.y, parBounds.max.z),
  };
}

inline Bounds3 transformBounds(const Bounds3& parBounds,
                               const glm::mat4& parTransform) {
  Bounds3 transformed;
  if (!parBounds.is_valid) {
    return transformed;
  }
  for (const glm::vec3& corner : boundsCorners(parBounds)) {
    transformed.includePoint(
        glm::vec3(parTransform * glm::vec4(corner, 1.0f)));
  }
  return transformed;
}

}  // namespace kage::math
