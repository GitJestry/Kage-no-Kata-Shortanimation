#pragma once

#include "math/bounds.hpp"
#include "math/transform.hpp"

#include <glm/glm.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace kage::scene {

inline constexpr std::size_t INVALID_ASSET_LIBRARY_INDEX =
    static_cast<std::size_t>(-1);

struct NameComponent final {
  std::string name;
};

struct TransformComponent final {
  math::Transform transform;
};

struct StaticMeshComponent final {
  std::size_t asset_library_index = INVALID_ASSET_LIBRARY_INDEX;
  math::Bounds3 local_bounds;
  bool visible = true;
};

struct RigComponent final {
  std::vector<std::vector<glm::mat4>> primitive_skin_matrices;
};

struct CameraComponent final {
  float vertical_fov_degrees = 45.0f;
  float near_plane = 0.01f;
  float far_plane = 100.0f;
};

struct LightComponent final {
  glm::vec3 color{1.0f, 0.86f, 0.56f};
  float intensity = 3.0f;
  float range = 9.0f;
  bool enabled = true;
  bool casts_shadows = true;
};

struct SunLightSettings final {
  glm::vec3 direction_to_sun{0.35f, 0.85f, 0.45f};
  glm::vec3 color{1.0f, 0.94f, 0.84f};
  float intensity = 1.0f;
  bool enabled = true;
};

}  // namespace kage::scene
