#pragma once

#include <glm/glm.hpp>

namespace kage::lighting {

[[nodiscard]] glm::vec3 directionToPointLight(const glm::vec3& parSurfacePosition,
                                              const glm::vec3& parLightPosition);
[[nodiscard]] float lambertResponse(const glm::vec3& parNormal,
                                    const glm::vec3& parDirectionToLight);

}  // namespace kage::lighting
