#pragma once

#include <glm/glm.hpp>

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

}  // namespace kage::math
