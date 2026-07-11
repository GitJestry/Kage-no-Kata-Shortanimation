#pragma once

#include "camera/camera.hpp"
#include "math/bounds.hpp"

#include <glm/glm.hpp>

#include <cstddef>
#include <optional>

namespace kage::render {

enum class ViewportMode {
  Bounds,
  Solid,
  Material,
  Final
};

class ViewportPolicy final {
 public:
  [[nodiscard]] static bool isVisible(const math::Bounds3& parBounds,
                                      const glm::mat4& parViewProjection);
  [[nodiscard]] static float projectedDiameterPixels(
      const math::Bounds3& parBounds, const camera::Camera& parCamera,
      const glm::vec2& parViewportSize);
  [[nodiscard]] static std::size_t selectLod(
      ViewportMode parMode, float parProjectedPixels, bool parSelected,
      std::optional<std::size_t> parPreviousLod = std::nullopt);
};

}  // namespace kage::render
