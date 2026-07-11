#include "render/viewport_policy.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace kage::render {

bool ViewportPolicy::isVisible(const math::Bounds3& parBounds,
                               const glm::mat4& parViewProjection) {
  if (!parBounds.is_valid) {
    return true;
  }
  const std::array<glm::vec3, 8> corners = {
      glm::vec3(parBounds.min.x, parBounds.min.y, parBounds.min.z),
      glm::vec3(parBounds.max.x, parBounds.min.y, parBounds.min.z),
      glm::vec3(parBounds.min.x, parBounds.max.y, parBounds.min.z),
      glm::vec3(parBounds.max.x, parBounds.max.y, parBounds.min.z),
      glm::vec3(parBounds.min.x, parBounds.min.y, parBounds.max.z),
      glm::vec3(parBounds.max.x, parBounds.min.y, parBounds.max.z),
      glm::vec3(parBounds.min.x, parBounds.max.y, parBounds.max.z),
      glm::vec3(parBounds.max.x, parBounds.max.y, parBounds.max.z),
  };
  std::array<int, 6> outside{};
  for (const glm::vec3& corner : corners) {
    const glm::vec4 clip = parViewProjection * glm::vec4(corner, 1.0f);
    outside[0] += clip.x < -clip.w;
    outside[1] += clip.x > clip.w;
    outside[2] += clip.y < -clip.w;
    outside[3] += clip.y > clip.w;
    outside[4] += clip.z < -clip.w;
    outside[5] += clip.z > clip.w;
  }
  return std::none_of(outside.begin(), outside.end(),
                      [](int count) { return count == 8; });
}

float ViewportPolicy::projectedDiameterPixels(
    const math::Bounds3& parBounds, const camera::Camera& parCamera,
    const glm::vec2& parViewportSize) {
  if (!parBounds.is_valid) {
    return 0.0f;
  }
  const glm::vec3 center = (parBounds.min + parBounds.max) * 0.5f;
  const float distance =
      std::max(glm::length(center - parCamera.position), 0.01f);
  const float diameter =
      std::max({parBounds.getSize().x, parBounds.getSize().y,
                parBounds.getSize().z});
  const float focal_pixels =
      std::max(parViewportSize.y, 1.0f) /
      (2.0f * std::tan(glm::radians(parCamera.vertical_fov_degrees) * 0.5f));
  return diameter * focal_pixels / distance;
}

std::size_t ViewportPolicy::selectLod(
    ViewportMode parMode, float parProjectedPixels, bool parSelected,
    std::optional<std::size_t> parPreviousLod) {
  if (parMode == ViewportMode::Final || parSelected) {
    return 0;
  }
  if (parMode == ViewportMode::Solid) {
    return 2;
  }

  constexpr float LOD0_THRESHOLD = 350.0f;
  constexpr float LOD1_THRESHOLD = 80.0f;
  constexpr float HYSTERESIS = 0.15f;
  if (parPreviousLod == 0 &&
      parProjectedPixels >= LOD0_THRESHOLD * (1.0f - HYSTERESIS)) {
    return 0;
  }
  if (parPreviousLod == 1) {
    if (parProjectedPixels > LOD0_THRESHOLD * (1.0f + HYSTERESIS)) {
      return 0;
    }
    if (parProjectedPixels >= LOD1_THRESHOLD * (1.0f - HYSTERESIS)) {
      return 1;
    }
  }
  if (parPreviousLod == 2 &&
      parProjectedPixels <= LOD1_THRESHOLD * (1.0f + HYSTERESIS)) {
    return 2;
  }
  if (parProjectedPixels >= LOD0_THRESHOLD) {
    return 0;
  }
  return parProjectedPixels >= LOD1_THRESHOLD ? 1 : 2;
}

}  // namespace kage::render
