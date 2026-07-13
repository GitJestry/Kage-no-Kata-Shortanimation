#include "camera/camera.hpp"
#include "camera/fly_camera_controller.hpp"
#include "camera/session_camera.hpp"
#include "render/viewport_rect.hpp"

#include <glm/gtc/matrix_inverse.hpp>

#include <cmath>
#include <iostream>
#include <limits>

int main() {
  const kage::render::ViewportRect movie_view{{0, 0}, {2560, 856}, 1440};
  if (!movie_view.contains(glm::vec2(1280.0f, 428.0f)) ||
      movie_view.contains(glm::vec2(1280.0f, 1200.0f)) ||
      movie_view.toLocal(glm::vec2(1280.0f, 428.0f)) !=
          glm::vec2(1280.0f, 428.0f) ||
      movie_view.glViewportY() != 584) {
    std::cerr << "viewport rectangle coordinate conversion failed\n";
    return 1;
  }

  kage::camera::Camera camera;
  camera.lookAt(glm::vec3(0.0f));
  const glm::vec2 viewport(1920.0f, 1080.0f);
  const glm::mat4 inverse = glm::inverse(camera.getViewProjectionMatrix(viewport));
  glm::vec4 near_point = inverse * glm::vec4(0.0f, 0.0f, -1.0f, 1.0f);
  glm::vec4 far_point = inverse * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
  near_point /= near_point.w;
  far_point /= far_point.w;
  const glm::vec3 center_ray = glm::normalize(glm::vec3(far_point - near_point));
  if (glm::dot(center_ray, camera.getForward()) < 0.9999f) {
    std::cerr << "center projection ray does not match camera forward\n";
    return 1;
  }

  kage::camera::FlyCameraController controller;
  controller.syncFromCamera(camera);
  const glm::quat before = camera.orientation;
  controller.look(camera, glm::vec2(0.0f));
  if (std::abs(glm::dot(before, camera.orientation)) < 0.9999f) {
    std::cerr << "fly controller changed a synchronized camera\n";
    return 1;
  }

  kage::math::Transform session;
  session.translation = camera.position;
  session.rotation = camera.orientation;
  if (!kage::camera::isValidSessionCamera(session, 50.0f, 0.03f, 500.0f)) {
    std::cerr << "valid session camera was rejected\n";
    return 1;
  }
  session.translation.x = std::numeric_limits<float>::quiet_NaN();
  if (kage::camera::isValidSessionCamera(session, 50.0f, 0.03f, 500.0f)) {
    std::cerr << "invalid session camera was accepted\n";
    return 1;
  }
  return 0;
}
