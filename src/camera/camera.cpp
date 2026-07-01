#include "camera/camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace {

constexpr glm::vec3 WORLD_UP{0.0f, 1.0f, 0.0f};
constexpr glm::vec3 LOCAL_FORWARD{0.0f, 0.0f, -1.0f};
constexpr glm::vec3 LOCAL_RIGHT{1.0f, 0.0f, 0.0f};
constexpr glm::vec3 LOCAL_UP{0.0f, 1.0f, 0.0f};

}  // namespace

namespace kage::camera {

void Camera::lookAt(const glm::vec3& parTarget) {
  const glm::vec3 direction = parTarget - position;
  if (glm::length(direction) <= 0.0001f) {
    return;
  }

  const glm::mat4 view = glm::lookAt(position, parTarget, WORLD_UP);
  orientation = glm::normalize(glm::quat_cast(glm::inverse(view)));
}

glm::vec3 Camera::getForward() const {
  return glm::normalize(orientation * LOCAL_FORWARD);
}

glm::vec3 Camera::getRight() const {
  return glm::normalize(orientation * LOCAL_RIGHT);
}

glm::vec3 Camera::getUp() const {
  return glm::normalize(orientation * LOCAL_UP);
}

glm::mat4 Camera::getViewMatrix() const {
  return glm::lookAt(position, position + getForward(), getUp());
}

glm::mat4 Camera::getProjectionMatrix(const glm::vec2& parViewportSize) const {
  const float aspect_ratio =
      parViewportSize.y > 0.0f ? parViewportSize.x / parViewportSize.y : 1.0f;
  return glm::perspective(glm::radians(vertical_fov_degrees), aspect_ratio,
                          near_plane, far_plane);
}

glm::mat4 Camera::getViewProjectionMatrix(
    const glm::vec2& parViewportSize) const {
  return getProjectionMatrix(parViewportSize) * getViewMatrix();
}

}  // namespace kage::camera
