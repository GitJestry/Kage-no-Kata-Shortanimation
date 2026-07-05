#pragma once

#include "camera/camera.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

namespace kage::camera {

[[nodiscard]] inline float getWorldUnitsPerPixel(
    const Camera& parCamera, const glm::vec3& parWorldPoint,
    const glm::vec2& parViewportSize) {
  const float distance =
      std::max(glm::length(parWorldPoint - parCamera.position), 0.1f);
  const float visible_height =
      2.0f * distance *
      std::tan(glm::radians(parCamera.vertical_fov_degrees) * 0.5f);
  return visible_height / std::max(parViewportSize.y, 1.0f);
}

[[nodiscard]] inline float getWorldLengthForPixels(
    const Camera& parCamera, const glm::vec3& parWorldPoint,
    const glm::vec2& parViewportSize, float parPixels) {
  return getWorldUnitsPerPixel(parCamera, parWorldPoint, parViewportSize) *
         std::max(parPixels, 1.0f);
}

}  // namespace kage::camera
