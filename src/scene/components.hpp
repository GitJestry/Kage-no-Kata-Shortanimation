#pragma once

#include "math/bounds.hpp"
#include "math/transform.hpp"

#include <glm/glm.hpp>

#include <cstddef>
#include <string>

namespace kage::scene {

inline constexpr std::size_t INVALID_STATIC_MESH_HANDLE =
    static_cast<std::size_t>(-1);
inline constexpr std::size_t INVALID_ASSET_LIBRARY_INDEX =
    static_cast<std::size_t>(-1);

struct NameComponent final {
  std::string name;
};

struct TransformComponent final {
  math::Transform transform;
};

struct StaticMeshComponent final {
  std::size_t mesh_handle = INVALID_STATIC_MESH_HANDLE;
  std::size_t asset_library_index = INVALID_ASSET_LIBRARY_INDEX;
  math::Bounds3 local_bounds;
  float opacity = 1.0f;
  bool visible = true;
};

struct CameraComponent final {
  bool active = false;
  float vertical_fov_degrees = 45.0f;
  float near_plane = 0.01f;
  float far_plane = 100.0f;
};

struct DirectionalLightComponent final {
  glm::vec3 color{1.0f, 0.94f, 0.84f};
  float intensity = 1.0f;
  bool enabled = true;
};

struct PointLightComponent final {
  glm::vec3 color{1.0f, 0.86f, 0.56f};
  float intensity = 3.0f;
  float range = 9.0f;
  bool enabled = true;
};

}  // namespace kage::scene
