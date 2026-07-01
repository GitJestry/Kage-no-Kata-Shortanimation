#pragma once

#include <glm/glm.hpp>

namespace kage::lighting {

struct DirectionalLight final {
  glm::vec3 direction{0.35f, -0.85f, -0.45f};
  glm::vec3 color{1.0f, 0.94f, 0.84f};
  float intensity = 1.0f;
};

struct PointLight final {
  glm::vec3 position{3.0f, 3.0f, 2.0f};
  glm::vec3 color{1.0f, 0.86f, 0.56f};
  float intensity = 3.0f;
  float range = 9.0f;
  bool enabled = false;
};

struct LightingState final {
  glm::vec3 ambient_color{0.18f, 0.2f, 0.24f};
  DirectionalLight sun;
  PointLight point;
};

}  // namespace kage::lighting
