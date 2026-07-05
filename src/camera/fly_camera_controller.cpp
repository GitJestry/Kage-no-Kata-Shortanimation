#include "camera/fly_camera_controller.hpp"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace {

constexpr float MIN_SPEED = 0.2f;
constexpr float MAX_SPEED = 40.0f;
constexpr float SPEED_STEP = 1.12f;
constexpr float MAX_PITCH_RADIANS = glm::radians(88.0f);
constexpr float MIN_HORIZONTAL_FORWARD = 0.0001f;
constexpr glm::vec3 WORLD_UP{0.0f, 1.0f, 0.0f};
constexpr glm::vec3 LOCAL_RIGHT{1.0f, 0.0f, 0.0f};

}  // namespace

namespace kage::camera {

void FlyCameraController::update(Camera& parCamera,
                                 const FlyCameraInput& parInput,
                                 float parDeltaSeconds) const {
  glm::vec3 movement{0.0f};

  if (parInput.forward) {
    movement += parCamera.getForward();
  }
  if (parInput.backward) {
    movement -= parCamera.getForward();
  }
  if (parInput.right) {
    movement += parCamera.getRight();
  }
  if (parInput.left) {
    movement -= parCamera.getRight();
  }
  if (parInput.up) {
    movement += glm::vec3(0.0f, 1.0f, 0.0f);
  }
  if (parInput.down) {
    movement -= glm::vec3(0.0f, 1.0f, 0.0f);
  }

  if (glm::length(movement) <= 0.0f) {
    return;
  }

  parCamera.position +=
      glm::normalize(movement) * m_move_speed * parDeltaSeconds;
}

void FlyCameraController::look(Camera& parCamera,
                               const glm::vec2& parPixelDelta) {
  if (!m_angles_initialized) {
    syncFromCamera(parCamera);
  }

  m_yaw_radians -= parPixelDelta.x * m_mouse_radians_per_pixel;
  m_pitch_radians -= parPixelDelta.y * m_mouse_radians_per_pixel;
  m_pitch_radians =
      std::clamp(m_pitch_radians, -MAX_PITCH_RADIANS, MAX_PITCH_RADIANS);
  applyAngles(parCamera);
}

void FlyCameraController::syncFromCamera(const Camera& parCamera) {
  const glm::vec3 forward = parCamera.getForward();
  const float horizontal_length =
      glm::length(glm::vec2(forward.x, forward.z));

  if (horizontal_length > MIN_HORIZONTAL_FORWARD) {
    m_yaw_radians = std::atan2(-forward.x, -forward.z);
  }
  m_pitch_radians =
      std::asin(std::clamp(forward.y, -1.0f, 1.0f));
  m_pitch_radians =
      std::clamp(m_pitch_radians, -MAX_PITCH_RADIANS, MAX_PITCH_RADIANS);
  m_angles_initialized = true;
}

void FlyCameraController::resetRoll(Camera& parCamera) {
  syncFromCamera(parCamera);
  applyAngles(parCamera);
}

void FlyCameraController::adjustSpeed(float parScrollAmount) {
  m_move_speed *= std::pow(SPEED_STEP, parScrollAmount);
  m_move_speed = std::clamp(m_move_speed, MIN_SPEED, MAX_SPEED);
}

void FlyCameraController::setMoveSpeed(float parMoveSpeed) {
  m_move_speed = std::clamp(parMoveSpeed, MIN_SPEED, MAX_SPEED);
}

float FlyCameraController::getMoveSpeed() const {
  return m_move_speed;
}

void FlyCameraController::applyAngles(Camera& parCamera) const {
  const glm::quat yaw = glm::angleAxis(m_yaw_radians, WORLD_UP);
  const glm::quat pitch = glm::angleAxis(m_pitch_radians, LOCAL_RIGHT);
  parCamera.orientation = glm::normalize(yaw * pitch);
}

}  // namespace kage::camera
