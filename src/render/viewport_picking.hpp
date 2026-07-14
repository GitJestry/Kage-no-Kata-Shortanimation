#pragma once

#include "camera/camera.hpp"
#include "film/film_frame_state.hpp"
#include "math/bounds.hpp"
#include "scene/world.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace kage::render {

struct ViewportPickRay final {
  glm::vec3 origin{0.0f};
  glm::vec3 direction{0.0f, 0.0f, -1.0f};
};

[[nodiscard]] inline math::Transform viewportEntityTransform(
    const scene::EntityRecord& parEntity,
    const film::FilmFrameState* parFilmState) {
  if (parFilmState != nullptr) {
    const auto evaluated = std::find_if(
        parFilmState->transforms.begin(), parFilmState->transforms.end(),
        [entity = parEntity.id](const film::TransformOverride& item) {
          return item.entity == entity;
        });
    if (evaluated != parFilmState->transforms.end()) {
      return evaluated->transform;
    }
  }
  return parEntity.transform.transform;
}

[[nodiscard]] inline ViewportPickRay makeViewportPickRay(
    const camera::Camera& parCamera, const glm::vec2& parCursorPixel,
    const glm::vec2& parViewportSize) {
  const glm::vec2 viewport = glm::max(parViewportSize, glm::vec2(1.0f));
  const glm::vec2 ndc{(parCursorPixel.x / viewport.x) * 2.0f - 1.0f,
                      1.0f - (parCursorPixel.y / viewport.y) * 2.0f};
  const glm::mat4 inverse_view_projection =
      glm::inverse(parCamera.getViewProjectionMatrix(viewport));
  glm::vec4 near_point =
      inverse_view_projection * glm::vec4(ndc, -1.0f, 1.0f);
  glm::vec4 far_point =
      inverse_view_projection * glm::vec4(ndc, 1.0f, 1.0f);
  near_point /= near_point.w;
  far_point /= far_point.w;
  return {glm::vec3(near_point),
          glm::normalize(glm::vec3(far_point - near_point))};
}

[[nodiscard]] inline bool viewportRayIntersectsBounds(
    const ViewportPickRay& parRay, const math::Bounds3& parBounds,
    float& parDistance) {
  constexpr float RAY_EPSILON = 0.00001f;
  if (!parBounds.is_valid) {
    return false;
  }
  float min_distance = 0.0f;
  float max_distance = std::numeric_limits<float>::max();
  for (int axis = 0; axis < 3; ++axis) {
    const float origin = parRay.origin[axis];
    const float direction = parRay.direction[axis];
    if (std::abs(direction) < RAY_EPSILON) {
      if (origin < parBounds.min[axis] || origin > parBounds.max[axis]) {
        return false;
      }
      continue;
    }
    float near_distance = (parBounds.min[axis] - origin) / direction;
    float far_distance = (parBounds.max[axis] - origin) / direction;
    if (near_distance > far_distance) {
      std::swap(near_distance, far_distance);
    }
    min_distance = std::max(min_distance, near_distance);
    max_distance = std::min(max_distance, far_distance);
    if (min_distance > max_distance) {
      return false;
    }
  }
  parDistance = min_distance;
  return true;
}

[[nodiscard]] inline bool viewportRayIntersectsSphere(
    const ViewportPickRay& parRay, const glm::vec3& parCenter,
    float parRadius, float& parDistance) {
  const glm::vec3 to_center = parCenter - parRay.origin;
  const float projected = glm::dot(to_center, parRay.direction);
  if (projected < 0.0f) {
    return false;
  }
  const glm::vec3 closest = parRay.origin + parRay.direction * projected;
  const glm::vec3 delta = closest - parCenter;
  if (glm::dot(delta, delta) > parRadius * parRadius) {
    return false;
  }
  parDistance = projected;
  return true;
}

[[nodiscard]] inline std::optional<scene::EntityId> pickViewportEntityBounds(
    const scene::World& parWorld, const camera::Camera* parCamera,
    const film::FilmFrameState* parFilmState,
    const glm::vec2& parCursorPixel, const glm::vec2& parViewportSize,
    float parHandleExtent) {
  if (parCamera == nullptr) {
    return std::nullopt;
  }
  const ViewportPickRay ray =
      makeViewportPickRay(*parCamera, parCursorPixel, parViewportSize);
  float closest_distance = std::numeric_limits<float>::max();
  std::optional<scene::EntityId> closest_entity;
  for (const scene::EntityRecord& entity : parWorld.getEntities()) {
    if (!entity.alive) {
      continue;
    }
    const math::Transform transform =
        viewportEntityTransform(entity, parFilmState);
    float distance = 0.0f;
    const bool hit =
        entity.static_mesh.has_value() && entity.static_mesh->visible
            ? viewportRayIntersectsBounds(
                  ray,
                  math::transformBounds(entity.static_mesh->local_bounds,
                                        transform.toMatrix()),
                  distance)
            : viewportRayIntersectsSphere(
                  ray, transform.translation, parHandleExtent, distance);
    if (hit && distance < closest_distance) {
      closest_distance = distance;
      closest_entity = entity.id;
    }
  }
  return closest_entity;
}

}  // namespace kage::render
