#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace kage::camera {

struct Camera final {
  glm::vec3 position{4.0f, 2.6f, 6.0f};
  glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};
  float vertical_fov_degrees = 50.0f;
  float near_plane = 0.03f;
  float far_plane = 500.0f;

  void lookAt(const glm::vec3& parTarget);
  [[nodiscard]] glm::vec3 getForward() const;
  [[nodiscard]] glm::vec3 getRight() const;
  [[nodiscard]] glm::vec3 getUp() const;
  [[nodiscard]] glm::mat4 getViewMatrix() const;
  [[nodiscard]] glm::mat4 getProjectionMatrix(
      const glm::vec2& parViewportSize) const;
  [[nodiscard]] glm::mat4 getViewProjectionMatrix(
      const glm::vec2& parViewportSize) const;
};

}  // namespace kage::camera
