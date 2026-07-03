#include "lighting/light_math.hpp"

#include <glm/geometric.hpp>

#include <algorithm>

namespace kage::lighting {

glm::vec3 directionToPointLight(const glm::vec3& parSurfacePosition,
                                const glm::vec3& parLightPosition) {
  const glm::vec3 offset = parLightPosition - parSurfacePosition;
  const float length = glm::length(offset);
  return length > 0.0001f ? offset / length : glm::vec3(0.0f, 1.0f, 0.0f);
}

float lambertResponse(const glm::vec3& parNormal,
                      const glm::vec3& parDirectionToLight) {
  return std::max(glm::dot(glm::normalize(parNormal),
                           glm::normalize(parDirectionToLight)),
                  0.0f);
}

}  // namespace kage::lighting
