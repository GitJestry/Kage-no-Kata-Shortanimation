#include "camera/session_camera.hpp"

#include <glm/gtc/quaternion.hpp>

#include <cmath>

namespace kage::camera {

bool isValidSessionCamera(const math::Transform& parTransform,
                          float parFovDegrees, float parNearPlane,
                          float parFarPlane) {
  const glm::vec3 position = parTransform.translation;
  const float rotation_length = glm::length(parTransform.rotation);
  const bool finite = std::isfinite(position.x) &&
                      std::isfinite(position.y) &&
                      std::isfinite(position.z) &&
                      std::isfinite(rotation_length) &&
                      std::isfinite(parFovDegrees) &&
                      std::isfinite(parNearPlane) &&
                      std::isfinite(parFarPlane);
  return finite && rotation_length > 0.0001f &&
         std::abs(position.x) <= 1000000.0f &&
         std::abs(position.y) <= 1000000.0f &&
         std::abs(position.z) <= 1000000.0f &&
         parFovDegrees >= 10.0f && parFovDegrees <= 120.0f &&
         parNearPlane >= 0.001f && parFarPlane > parNearPlane &&
         parFarPlane <= 5000.0f;
}

}  // namespace kage::camera
