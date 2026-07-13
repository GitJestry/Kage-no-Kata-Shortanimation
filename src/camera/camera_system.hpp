#pragma once

#include "camera/camera.hpp"
#include "camera/fly_camera_controller.hpp"
#include "math/bounds.hpp"

#include <glm/glm.hpp>

namespace kage::camera {

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
  void frameBounds(const math::Bounds3& parBounds);
  void handleMouseMove(const glm::vec2& parPixelDelta, bool parLeftButton,
                       bool parRightButton, bool parMiddleButton,
                       const glm::vec2& parViewportSize);
  void handleScroll(float parScrollAmount);
  void setMovement(CameraMovement parMovement, bool parActive);
  void syncFlyControllerFromCamera();
  void setFlyMoveSpeed(float parMoveSpeed);

  [[nodiscard]] const Camera& getEditorCamera() const;
  [[nodiscard]] Camera& getEditorCamera();
  [[nodiscard]] float getFlyMoveSpeed() const;

 private:
  Camera m_editor_camera;
  FlyCameraInput m_fly_input;
  FlyCameraController m_fly_controller;
};

}  // namespace kage::camera
