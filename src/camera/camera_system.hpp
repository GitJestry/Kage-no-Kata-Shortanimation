#pragma once

#include "camera/camera.hpp"
#include "camera/fly_camera_controller.hpp"
#include "math/bounds.hpp"

#include <glm/glm.hpp>

namespace kage::camera {

enum class CameraMode {
  Fly,
  Orbit
};

enum class CameraMovement {
  Forward,
  Backward,
  Left,
  Right,
  Up,
  Down
};

class CameraSystem final {
 public:
  CameraSystem();

  void update(float parDeltaSeconds);
  void focusBounds(const math::Bounds3& parBounds);
  void focusPoint(const glm::vec3& parPoint, float parExtent);
  void handleMouseMove(const glm::vec2& parPixelDelta, bool parLeftButton,
                       bool parRightButton, bool parMiddleButton,
                       const glm::vec2& parViewportSize);
  void handleScroll(float parScrollAmount);
  void setMovement(CameraMovement parMovement, bool parActive);
  void setMode(CameraMode parMode);
  void syncFlyControllerFromCamera();
  void resetFlyCameraRoll();
  void setFlyMoveSpeed(float parMoveSpeed);

  [[nodiscard]] CameraMode getMode() const;
  [[nodiscard]] const Camera& getCamera() const;
  [[nodiscard]] Camera& getCamera();
  [[nodiscard]] float getFlyMoveSpeed() const;

 private:
  Camera m_camera;
  CameraMode m_mode = CameraMode::Fly;
  FlyCameraInput m_fly_input;
  FlyCameraController m_fly_controller;
  glm::vec3 m_orbit_target{0.0f};
  float m_orbit_distance = 4.0f;
  float m_orbit_extent = 1.0f;
};

}  // namespace kage::camera
