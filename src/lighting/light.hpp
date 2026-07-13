#pragma once

#include <glm/glm.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace kage::lighting {

inline constexpr std::size_t MAX_POINT_LIGHTS = 32;
inline constexpr std::size_t MAX_POINT_SHADOWS = 2;

struct DirectionalLight final {
  glm::vec3 direction_to_light{0.35f, 0.85f, 0.45f};
  glm::vec3 color{1.0f, 0.94f, 0.84f};
  float intensity = 1.0f;
  bool enabled = false;
};

struct PointLight final {
  glm::vec3 position{3.0f, 3.0f, 2.0f};
  glm::vec3 color{1.0f, 0.86f, 0.56f};
  float intensity = 3.0f;
  float range = 9.0f;
  bool enabled = false;
  bool casts_shadow = false;
  std::uint32_t entity_id = 0;
};

struct LightingState final {
  glm::vec3 ambient_diffuse{0.0f};
  glm::vec3 ambient_specular{0.0f};
  float exposure = 1.0f;
  DirectionalLight sun;
  std::array<PointLight, MAX_POINT_LIGHTS> point_lights{};
  std::size_t point_light_count = 0;
};

}  // namespace kage::lighting
