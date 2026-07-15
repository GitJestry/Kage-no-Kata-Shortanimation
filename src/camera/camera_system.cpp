#include "camera/camera_system.hpp"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace {

constexpr float DEFAULT_FOCUS_DISTANCE_SCALE = 2.6f;
constexpr float FAR_PLANE_FOCUS_SCALE = 16.0f;

[[nodiscard]] float getLargestExtent(const kage::math::Bounds3& parBounds) {
  const glm::vec3 bounds_size = parBounds.getSize();
  return std::max({bounds_size.x, bounds_size.y, bounds_size.z, 1.0f});
}

}  // namespace

namespace kage::camera {

CameraSystem::CameraSystem() {
  m_editor_camera.lookAt(glm::vec3(0.0f, 0.7f, 0.0f));
  m_fly_controller.syncFromCamera(m_editor_camera);
}

void CameraSystem::update(float parDeltaSeconds) {
  m_fly_controller.update(m_editor_camera, m_fly_input, parDeltaSeconds);
}

void CameraSystem::frameBounds(const math::Bounds3& parBounds) {
  const float extent = getLargestExtent(parBounds);
  const glm::vec3 target =
      parBounds.is_valid ? (parBounds.min + parBounds.max) * 0.5f
                         : glm::vec3(0.0f);
  const float frame_extent = std::max(extent, 0.1f);
  const float frame_distance = frame_extent * DEFAULT_FOCUS_DISTANCE_SCALE;
  m_editor_camera.position =
      target - m_editor_camera.getForward() * frame_distance;
  m_editor_camera.far_plane =
      std::max(m_editor_camera.near_plane * 2.0f,
               frame_distance + frame_extent * FAR_PLANE_FOCUS_SCALE);
  m_editor_camera.lookAt(target);
  m_fly_controller.syncFromCamera(m_editor_camera);
}

void CameraSystem::handleMouseMove(const glm::vec2& parPixelDelta,
                                   bool parLeftButton, bool parRightButton,
                                   bool parMiddleButton,
                                   const glm::vec2& parViewportSize) {
  static_cast<void>(parLeftButton);
  static_cast<void>(parMiddleButton);
  static_cast<void>(parViewportSize);
  if (parRightButton) {
    m_fly_controller.look(m_editor_camera, parPixelDelta);
  }
}

void CameraSystem::handleScroll(float parScrollAmount) {
  m_fly_controller.adjustSpeed(parScrollAmount);
}

void CameraSystem::setMovement(CameraMovement parMovement, bool parActive) {
  switch (parMovement) {
    case CameraMovement::Forward:
      m_fly_input.forward = parActive;
      break;
    case CameraMovement::Backward:
      m_fly_input.backward = parActive;
      break;
    case CameraMovement::Left:
      m_fly_input.left = parActive;
      break;
    case CameraMovement::Right:
      m_fly_input.right = parActive;
      break;
    case CameraMovement::Up:
      m_fly_input.up = parActive;
      break;
    case CameraMovement::Down:
      m_fly_input.down = parActive;
      break;
  }
}

void CameraSystem::syncFlyControllerFromCamera() {
  m_fly_controller.syncFromCamera(m_editor_camera);
}

void CameraSystem::setFlyMoveSpeed(float parMoveSpeed) {
  m_fly_controller.setMoveSpeed(parMoveSpeed);
}

const Camera& CameraSystem::getEditorCamera() const {
  return m_editor_camera;
}

Camera& CameraSystem::getEditorCamera() {
  return m_editor_camera;
}

float CameraSystem::getFlyMoveSpeed() const {
  return m_fly_controller.getMoveSpeed();
}

}  // namespace kage::camera
