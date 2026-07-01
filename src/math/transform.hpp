#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace kage::math {

struct Transform final {
  glm::vec3 translation{0.0f};
  glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
  glm::vec3 scale{1.0f};

  [[nodiscard]] glm::mat4 toMatrix() const;
};

inline glm::mat4 Transform::toMatrix() const {
  glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation);
  transform *= glm::mat4_cast(rotation);
  return glm::scale(transform, scale);
}

}  // namespace kage::math
