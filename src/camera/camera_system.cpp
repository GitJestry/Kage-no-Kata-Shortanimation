#include "camera/camera_system.hpp"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace {

constexpr float DEFAULT_FOCUS_DISTANCE_SCALE = 2.6f;
constexpr float FAR_PLANE_FOCUS_SCALE = 16.0f;
constexpr float MIN_ORBIT_DISTANCE_SCALE = 0.18f;
constexpr float MAX_ORBIT_DISTANCE_SCALE = 28.0f;
constexpr float ORBIT_RADIANS_PER_PIXEL = 0.006f;
constexpr float PAN_PIXEL_SCALE = 1.0f;
constexpr float ORBIT_ZOOM_STEP = 0.88f;
constexpr glm::vec3 WORLD_UP{0.0f, 1.0f, 0.0f};

[[nodiscard]] float getLargestExtent(const kage::math::Bounds3& parBounds) {
  const glm::vec3 bounds_size = parBounds.getSize();
  return std::max({bounds_size.x, bounds_size.y, bounds_size.z, 1.0f});
}

}  // namespace

namespace kage::camera {

CameraSystem::CameraSystem() {
  m_camera.lookAt(glm::vec3(0.0f, 0.7f, 0.0f));
  m_fly_controller.syncFromCamera(m_camera);
}

void CameraSystem::update(float parDeltaSeconds) {
  if (m_mode == CameraMode::Fly) {
    m_fly_controller.update(m_camera, m_fly_input, parDeltaSeconds);
  }
}

void CameraSystem::focusBounds(const math::Bounds3& parBounds) {
  const float extent = getLargestExtent(parBounds);
  const glm::vec3 target =
      parBounds.is_valid ? (parBounds.min + parBounds.max) * 0.5f
                         : glm::vec3(0.0f);
  focusPoint(target, extent);
}

void CameraSystem::focusPoint(const glm::vec3& parPoint, float parExtent) {
  m_orbit_extent = std::max(parExtent, 0.1f);
  m_orbit_target = parPoint;
  m_orbit_distance = m_orbit_extent * DEFAULT_FOCUS_DISTANCE_SCALE;
  m_camera.position =
      m_orbit_target - m_camera.getForward() * m_orbit_distance;
  m_camera.far_plane =
      std::max(m_camera.near_plane * 2.0f,
               m_orbit_distance + m_orbit_extent * FAR_PLANE_FOCUS_SCALE);
  m_camera.lookAt(m_orbit_target);
  m_fly_controller.syncFromCamera(m_camera);
}

void CameraSystem::handleMouseMove(const glm::vec2& parPixelDelta,
                                   bool parLeftButton, bool parRightButton,
                                   bool parMiddleButton,
                                   const glm::vec2& parViewportSize) {
  if (m_mode == CameraMode::Fly) {
    if (parRightButton) {
      m_fly_controller.look(m_camera, parPixelDelta);
    }
    return;
  }

  if (parRightButton || parMiddleButton) {
    const float viewport_height = std::max(parViewportSize.y, 1.0f);
    const float visible_height =
        2.0f * m_orbit_distance *
        std::tan(glm::radians(m_camera.vertical_fov_degrees) * 0.5f);
    const float units_per_pixel =
        (visible_height / viewport_height) * PAN_PIXEL_SCALE;
    const glm::vec3 pan_delta =
        (-m_camera.getRight() * parPixelDelta.x +
         m_camera.getUp() * parPixelDelta.y) *
        units_per_pixel;
    m_orbit_target += pan_delta;
    m_camera.position += pan_delta;
  } else if (parLeftButton) {
    const glm::quat yaw_delta =
        glm::angleAxis(-parPixelDelta.x * ORBIT_RADIANS_PER_PIXEL, WORLD_UP);
    const glm::quat pitch_delta = glm::angleAxis(
        -parPixelDelta.y * ORBIT_RADIANS_PER_PIXEL, m_camera.getRight());
    glm::vec3 camera_offset = m_camera.position - m_orbit_target;
    camera_offset = pitch_delta * yaw_delta * camera_offset;
    m_camera.position = m_orbit_target + camera_offset;
    m_orbit_distance = std::max(glm::length(camera_offset), 0.1f);
    m_camera.lookAt(m_orbit_target);
  }
}

void CameraSystem::handleScroll(float parScrollAmount) {
  if (m_mode == CameraMode::Fly) {
    m_fly_controller.adjustSpeed(parScrollAmount);
    return;
  }

  const float min_distance = m_orbit_extent * MIN_ORBIT_DISTANCE_SCALE;
  const float max_distance = m_orbit_extent * MAX_ORBIT_DISTANCE_SCALE;
  m_orbit_distance *= std::pow(ORBIT_ZOOM_STEP, parScrollAmount);
  m_orbit_distance = std::clamp(m_orbit_distance, min_distance, max_distance);
  m_camera.position = m_orbit_target - m_camera.getForward() * m_orbit_distance;
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

void CameraSystem::setMode(CameraMode parMode) {
  m_mode = parMode;
  if (m_mode == CameraMode::Fly) {
    m_fly_controller.syncFromCamera(m_camera);
  }
}

void CameraSystem::syncFlyControllerFromCamera() {
  m_fly_controller.syncFromCamera(m_camera);
}

void CameraSystem::resetFlyCameraRoll() {
  m_fly_controller.resetRoll(m_camera);
}

void CameraSystem::setFlyMoveSpeed(float parMoveSpeed) {
  m_fly_controller.setMoveSpeed(parMoveSpeed);
}

CameraMode CameraSystem::getMode() const {
  return m_mode;
}

const Camera& CameraSystem::getCamera() const {
  return m_camera;
}

Camera& CameraSystem::getCamera() {
  return m_camera;
}

float CameraSystem::getFlyMoveSpeed() const {
  return m_fly_controller.getMoveSpeed();
}

}  // namespace kage::camera
