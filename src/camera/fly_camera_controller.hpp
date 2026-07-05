#pragma once

#include "camera/camera.hpp"

#include <glm/glm.hpp>

namespace kage::camera {

struct FlyCameraInput final {
  bool forward = false;
  bool backward = false;
  bool left = false;
  bool right = false;
  bool up = false;
  bool down = false;
};

class FlyCameraController final {
 public:
  void update(Camera& parCamera, const FlyCameraInput& parInput,
              float parDeltaSeconds) const;
  void look(Camera& parCamera, const glm::vec2& parPixelDelta);
  void syncFromCamera(const Camera& parCamera);
  void resetRoll(Camera& parCamera);
  void adjustSpeed(float parScrollAmount);
  void setMoveSpeed(float parMoveSpeed);

  [[nodiscard]] float getMoveSpeed() const;

 private:
  void applyAngles(Camera& parCamera) const;

  float m_move_speed = 6.0f;
  float m_mouse_radians_per_pixel = 0.0035f;
  float m_yaw_radians = 0.0f;
  float m_pitch_radians = 0.0f;
  bool m_angles_initialized = false;
};

}  // namespace kage::camera
