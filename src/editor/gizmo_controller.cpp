#include "editor/gizmo_controller.hpp"

#include "math/screen_projection.hpp"
#include "render/gizmo_metrics.hpp"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace {

constexpr float ROTATION_RADIANS_PER_PIXEL = 0.008f;
constexpr float AXIS_SCREEN_HIT_RADIUS = 28.0f;
constexpr float ROTATION_HANDLE_SCREEN_RADIUS = 34.0f;
constexpr float SCALE_UNITS_PER_PIXEL = 0.006f;

[[nodiscard]] std::array<glm::vec3, 3> getAxes(
    const kage::math::Transform& parTransform,
    kage::editor::GizmoController::AxisSpace parAxisSpace) {
  const glm::quat rotation =
      parAxisSpace == kage::editor::GizmoController::AxisSpace::World
          ? glm::quat(1.0f, 0.0f, 0.0f, 0.0f)
          : parTransform.rotation;
  return {rotation * glm::vec3(1.0f, 0.0f, 0.0f),
          rotation * glm::vec3(0.0f, 1.0f, 0.0f),
          rotation * glm::vec3(0.0f, 0.0f, 1.0f)};
}
[[nodiscard]] float getHandleLength(kage::engine::EngineCore& parEngine,
                                    const kage::scene::EntityRecord& parEntity,
                                    const glm::vec2& parViewportSize) {
  const kage::math::Bounds3 bounds = parEngine.getEntityWorldBounds(
      parEntity.id);
  return kage::render::getGizmoLength(
      parEngine.getCameraSystem().getEditorCamera(), parViewportSize,
      parEntity, bounds);
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
  if (!selected.isValid()) {
    return false;
  }

  scene::EntityRecord* entity = parEngine.getWorld().findEntity(selected);
  if (entity == nullptr) {
    return false;
  }

  glm::vec3 picked_axis{1.0f, 0.0f, 0.0f};
  glm::vec3 picked_axis_color = render::GIZMO_AXIS_X_COLOR;
  if (pickRotationHandle(parEngine, selected, parCursorPixel,
                         parViewportSize)) {
    m_operation = Operation::Rotate;
    m_entity = selected;
    return true;
  }

  if (m_mode != TransformMode::Rotate &&
      pickAxis(parEngine, selected, parCursorPixel, parViewportSize,
               picked_axis, picked_axis_color)) {
    m_entity = selected;
    m_axis = glm::normalize(picked_axis);
    m_axis_color = picked_axis_color;
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

  const camera::Camera& camera =
      parEngine.getCameraSystem().getEditorCamera();
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
          signed_pixels *
          camera::getWorldUnitsPerPixel(camera, origin, parViewportSize);
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
  m_axis_color = render::GIZMO_AXIS_X_COLOR;
  m_drag_height = 0.0f;
}

bool GizmoController::isActive() const {
  return m_operation != Operation::None;
}

std::optional<render::GizmoGuide> GizmoController::getActiveGuide(
    const engine::EngineCore& parEngine) const {
  if (m_operation != Operation::MoveAxis &&
      m_operation != Operation::ScaleAxis) {
    return std::nullopt;
  }

  const scene::EntityRecord* entity =
      parEngine.getWorld().findEntity(m_entity);
  if (entity == nullptr) {
    return std::nullopt;
  }

  render::GizmoGuide guide;
  guide.active = true;
  guide.origin = entity->transform.transform.translation;
  guide.axis = glm::normalize(m_axis);
  guide.color = m_axis_color;
  guide.half_length =
      std::max(static_cast<float>(parEngine.getFloorGridRadius()) * 2.0f,
               80.0f);
  return guide;
}

bool GizmoController::pickAxis(engine::EngineCore& parEngine,
                               scene::EntityId parEntity,
                               const glm::vec2& parCursorPixel,
                               const glm::vec2& parViewportSize,
                               glm::vec3& parAxis,
                               glm::vec3& parAxisColor) const {
  const scene::EntityRecord* entity = parEngine.getWorld().findEntity(parEntity);
  if (entity == nullptr) {
    return false;
  }

  const math::Transform& transform = entity->transform.transform;
  const float axis_length = getHandleLength(parEngine, *entity,
                                            parViewportSize);
  const std::array<glm::vec3, 3> axes = getAxes(transform, m_axis_space);
  const glm::mat4 view_projection =
      parEngine.getCameraSystem().getEditorCamera().getViewProjectionMatrix(
          parViewportSize);
  const kage::math::ScreenPoint origin = kage::math::projectPoint(
      transform.translation, view_projection, parViewportSize);
  if (!origin.valid) {
    return false;
  }

  float closest_distance = AXIS_SCREEN_HIT_RADIUS;
  bool picked = false;
  constexpr std::array<glm::vec3, 3> AXIS_COLORS = {
      render::GIZMO_AXIS_X_COLOR, render::GIZMO_AXIS_Y_COLOR,
      render::GIZMO_AXIS_Z_COLOR};
  for (std::size_t axis_index = 0; axis_index < axes.size(); ++axis_index) {
    const glm::vec3& axis = axes[axis_index];
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
      parAxisColor = AXIS_COLORS[axis_index];
      picked = true;
    }
  }
  return picked;
}

bool GizmoController::pickRotationHandle(
    engine::EngineCore& parEngine, scene::EntityId parEntity,
    const glm::vec2& parCursorPixel, const glm::vec2& parViewportSize) const {
  const scene::EntityRecord* entity = parEngine.getWorld().findEntity(parEntity);
  if (entity == nullptr) {
    return false;
  }

  const math::Transform& transform = entity->transform.transform;
  const float axis_length = getHandleLength(parEngine, *entity,
                                            parViewportSize);
  const auto axes = getAxes(transform, m_axis_space);
  const glm::vec3& right = axes[0];
  const glm::vec3& up = axes[1];
  const glm::vec3 handle_position =
      transform.translation + glm::normalize(right + up) * axis_length * 0.42f;
  const glm::mat4 view_projection =
      parEngine.getCameraSystem().getEditorCamera().getViewProjectionMatrix(
          parViewportSize);
  const kage::math::ScreenPoint handle = kage::math::projectPoint(
      handle_position, view_projection, parViewportSize);
  if (!handle.valid) {
    return false;
  }

  const glm::vec2 delta = parCursorPixel - handle.pixel;
  return glm::dot(delta, delta) <=
         ROTATION_HANDLE_SCREEN_RADIUS * ROTATION_HANDLE_SCREEN_RADIUS;
}

}  // namespace kage::editor
