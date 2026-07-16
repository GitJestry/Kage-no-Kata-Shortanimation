#pragma once

#include "camera/camera.hpp"
#include "camera/screen_metrics.hpp"
#include "math/bounds.hpp"
#include "scene/components.hpp"

#include <glm/glm.hpp>

#include <algorithm>

namespace kage::render {

inline constexpr glm::vec3 GIZMO_AXIS_X_COLOR{0.18f, 0.82f, 0.28f};
inline constexpr glm::vec3 GIZMO_AXIS_Y_COLOR{1.0f, 0.86f, 0.22f};
inline constexpr glm::vec3 GIZMO_AXIS_Z_COLOR{0.22f, 0.48f, 1.0f};
inline constexpr float GIZMO_LARGE_HANDLE_PIXELS = 150.0f;
inline constexpr float GIZMO_MESH_HANDLE_PIXELS = 118.0f;
inline constexpr float GIZMO_MESH_EXTENT_SCALE = 0.18f;
inline constexpr float GIZMO_MIN_LENGTH = 0.24f;
inline constexpr float GIZMO_MAX_LENGTH = 32.0f;

[[nodiscard]] inline float getGizmoLength(
    const camera::Camera& parCamera, const glm::vec2& parViewportSize,
    const scene::EntityRecord& parEntity, const math::Bounds3& parBounds) {
  const float pixels = parEntity.light.has_value() || parEntity.camera.has_value()
                           ? GIZMO_LARGE_HANDLE_PIXELS
                           : GIZMO_MESH_HANDLE_PIXELS;
  const float screen_length = camera::getWorldLengthForPixels(
      parCamera, parEntity.transform.transform.translation, parViewportSize,
      pixels);
  const glm::vec3 size = parBounds.getSize();
  const float mesh_length =
      parBounds.is_valid
          ? std::max({size.x, size.y, size.z, 1.0f}) *
                GIZMO_MESH_EXTENT_SCALE
          : 0.0f;
  return std::clamp(std::max(screen_length, mesh_length), GIZMO_MIN_LENGTH,
                    GIZMO_MAX_LENGTH);
}

}  // namespace kage::render
