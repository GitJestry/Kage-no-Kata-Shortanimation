#pragma once

#include "math/bounds.hpp"

#include <glm/glm.hpp>

namespace kage::render {

enum class ViewportMode {
  Bounds,
  Solid,
  Material,
  Final
};

[[nodiscard]] bool isVisible(const math::Bounds3& parBounds,
                             const glm::mat4& parViewProjection);

}  // namespace kage::render
