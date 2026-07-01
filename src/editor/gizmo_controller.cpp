#include "editor/gizmo_controller.hpp"

#include "math/screen_projection.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace {

constexpr float ROTATION_RADIANS_PER_PIXEL = 0.008f;
constexpr float AXIS_SCREEN_HIT_RADIUS = 22.0f;
constexpr float SCALE_UNITS_PER_PIXEL = 0.006f;

[[nodiscard]] float getLargestExtent(const kage::math::Bounds3& parBounds) {
  const glm::vec3 size = parBounds.getSize();
  return std::max({size.x, size.y, size.z, 1.0f});
}

[[nodiscard]] float getUnitsPerPixel(const kage::camera::Camera& parCamera,
                                     const glm::vec3& parPoint,
                                     const glm::vec2& parViewportSize) {
  const float distance = std::max(glm::length(parPoint - parCamera.position),
                                  0.1f);
  const float visible_height =
      2.0f * distance *
      std::tan(glm::radians(parCamera.vertical_fov_degrees) * 0.5f);
  return visible_height / std::max(parViewportSize.y, 1.0f);
}

}  // namespace

namespace kage::editor {

void GizmoController::setMode(TransformMode parMode) {
  m_mode = parMode;
}

void GizmoController::setAxisSpace(AxisSpace parAxisSpace) {
  m_axis_space = parAxisSpace;
}

GizmoController::TransformMode GizmoController::getMode() const {
  return m_mode;
}

GizmoController::AxisSpace GizmoController::getAxisSpace() const {
  return m_axis_space;
}

bool GizmoController::begin(engine::EngineCore& parEngine,
                            const glm::vec2& parCursorPixel,
                            const glm::vec2& parViewportSize) {
  const scene::EntityId selected = parEngine.getSelectedEntity();
  if (!selected.isValid() || selected == parEngine.getEditorCameraEntity()) {
    return false;
  }

  scene::EntityRecord* entity = parEngine.getWorld().findEntity(selected);
  if (entity == nullptr) {
    return false;
  }

  glm::vec3 picked_axis{1.0f, 0.0f, 0.0f};
  if (m_mode != TransformMode::Rotate &&
      pickAxis(parEngine, selected, parCursorPixel, parViewportSize,
               picked_axis)) {
    m_entity = selected;
    m_axis = glm::normalize(picked_axis);
    m_operation = m_mode == TransformMode::Scale ? Operation::ScaleAxis
                                                 : Operation::MoveAxis;
    return true;
  }

  if (parEngine.isCursorOverEntityCore(selected, parCursorPixel,
                                       parViewportSize)) {
    m_operation = m_mode == TransformMode::Scale ? Operation::ScaleUniform
                                                 : Operation::Rotate;
    m_entity = selected;
    return true;
  }

  const std::optional<scene::EntityId> picked =
      parEngine.pickEntity(parCursorPixel, parViewportSize);
  if (!picked.has_value() || *picked != selected) {
    return false;
  }

  m_operation = Operation::Move;
  m_entity = selected;
  m_drag_height = entity->static_mesh.has_value()
                      ? 0.0f
                      : entity->transform.transform.translation.y;
  glm::vec3 cursor_floor =
      parEngine.getPlacementPointOnFloor(parCursorPixel, parViewportSize);
  cursor_floor.y = m_drag_height;
  m_drag_offset = entity->transform.transform.translation - cursor_floor;
  return true;
}

void GizmoController::update(engine::EngineCore& parEngine,
                             const glm::vec2& parPixelDelta,
                             const glm::vec2& parCursorPixel,
                             const glm::vec2& parViewportSize,
                             bool parLeftButton) {
  if (!parLeftButton) {
    end();
    return;
  }
  if (!isActive()) {
    return;
  }

  scene::EntityRecord* entity = parEngine.getWorld().findEntity(m_entity);
  if (entity == nullptr) {
    end();
    return;
  }

  if (m_operation == Operation::Move) {
    glm::vec3 position =
        parEngine.getPlacementPointOnFloor(parCursorPixel, parViewportSize);
    position.y = m_drag_height;
    parEngine.setEntityPosition(m_entity, position + m_drag_offset);
    return;
  }

  const camera::Camera& camera = parEngine.getCameraSystem().getCamera();
  if (m_operation == Operation::MoveAxis ||
      m_operation == Operation::ScaleAxis) {
    const math::Transform transform = entity->transform.transform;
    const glm::vec3 origin = transform.translation;
    const glm::mat4 view_projection =
        camera.getViewProjectionMatrix(parViewportSize);
    const kage::math::ScreenPoint origin_screen =
        kage::math::projectPoint(origin, view_projection, parViewportSize);
    const kage::math::ScreenPoint axis_screen = kage::math::projectPoint(
        origin + m_axis, view_projection, parViewportSize);
    if (!origin_screen.valid || !axis_screen.valid) {
      return;
    }

    glm::vec2 axis_direction = axis_screen.pixel - origin_screen.pixel;
    if (glm::dot(axis_direction, axis_direction) <= 0.001f) {
      return;
    }
    axis_direction = glm::normalize(axis_direction);
    const float signed_pixels = glm::dot(parPixelDelta, axis_direction);
    if (m_operation == Operation::MoveAxis) {
      const float units =
          signed_pixels * getUnitsPerPixel(camera, origin, parViewportSize);
      parEngine.setEntityPosition(m_entity, transform.translation +
                                                m_axis * units);
      return;
    }

    math::Transform scaled = transform;
    const float scale_delta = signed_pixels * SCALE_UNITS_PER_PIXEL;
    const glm::vec3 local_axis = glm::inverse(transform.rotation) * m_axis;
    const int dominant_axis =
        std::abs(local_axis.x) > std::abs(local_axis.y) &&
                std::abs(local_axis.x) > std::abs(local_axis.z)
            ? 0
            : std::abs(local_axis.y) > std::abs(local_axis.z) ? 1 : 2;
    scaled.scale[dominant_axis] =
        std::max(0.001f, scaled.scale[dominant_axis] + scale_delta);
    parEngine.setEntityTransform(m_entity, scaled);
    return;
  }

  if (m_operation == Operation::ScaleUniform) {
    math::Transform scaled = entity->transform.transform;
    const float scale_delta =
        (parPixelDelta.x - parPixelDelta.y) * SCALE_UNITS_PER_PIXEL;
    scaled.scale =
        glm::max(scaled.scale + glm::vec3(scale_delta), glm::vec3(0.001f));
    parEngine.setEntityTransform(m_entity, scaled);
    return;
  }

  const glm::quat yaw_delta = glm::angleAxis(
      -parPixelDelta.x * ROTATION_RADIANS_PER_PIXEL, glm::vec3(0.0f, 1.0f, 0.0f));
  const glm::quat pitch_delta = glm::angleAxis(
      -parPixelDelta.y * ROTATION_RADIANS_PER_PIXEL, camera.getRight());
  math::Transform transform = entity->transform.transform;
  transform.rotation =
      glm::normalize(pitch_delta * yaw_delta * transform.rotation);
  parEngine.setEntityTransform(m_entity, transform);
}

void GizmoController::end() {
  m_operation = Operation::None;
  m_entity = {};
  m_drag_offset = glm::vec3(0.0f);
  m_axis = glm::vec3(1.0f, 0.0f, 0.0f);
  m_drag_height = 0.0f;
}

bool GizmoController::isActive() const {
  return m_operation != Operation::None;
}

GizmoController::Operation GizmoController::getOperation() const {
  return m_operation;
}

bool GizmoController::pickAxis(engine::EngineCore& parEngine,
                               scene::EntityId parEntity,
                               const glm::vec2& parCursorPixel,
                               const glm::vec2& parViewportSize,
                               glm::vec3& parAxis) const {
  const scene::EntityRecord* entity = parEngine.getWorld().findEntity(parEntity);
  if (entity == nullptr) {
    return false;
  }

  const math::Transform& transform = entity->transform.transform;
  const float axis_length =
      getLargestExtent(parEngine.getEntityWorldBounds(parEntity)) * 0.35f;
  const std::array<glm::vec3, 3> axes =
      m_axis_space == AxisSpace::World
          ? std::array<glm::vec3, 3>{glm::vec3(1.0f, 0.0f, 0.0f),
                                     glm::vec3(0.0f, 1.0f, 0.0f),
                                     glm::vec3(0.0f, 0.0f, 1.0f)}
          : std::array<glm::vec3, 3>{
                transform.rotation * glm::vec3(1.0f, 0.0f, 0.0f),
                transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f),
                transform.rotation * glm::vec3(0.0f, 0.0f, 1.0f)};
  const glm::mat4 view_projection =
      parEngine.getCameraSystem().getCamera().getViewProjectionMatrix(
          parViewportSize);
  const kage::math::ScreenPoint origin = kage::math::projectPoint(
      transform.translation, view_projection, parViewportSize);
  if (!origin.valid) {
    return false;
  }

  float closest_distance = AXIS_SCREEN_HIT_RADIUS;
  bool picked = false;
  for (const glm::vec3& axis : axes) {
    const kage::math::ScreenPoint end = kage::math::projectPoint(
        transform.translation + axis * axis_length, view_projection,
        parViewportSize);
    if (!end.valid) {
      continue;
    }

    const float distance =
        kage::math::distanceToSegment(parCursorPixel, origin.pixel, end.pixel);
    if (distance <= closest_distance) {
      closest_distance = distance;
      parAxis = axis;
      picked = true;
    }
  }
  return picked;
}

}  // namespace kage::editor
