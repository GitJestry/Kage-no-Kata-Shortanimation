#include "render/viewport_policy.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace kage::render {

bool isVisible(const math::Bounds3& parBounds,
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

}  // namespace kage::render
