#pragma once

#include <glm/glm.hpp>

#include <algorithm>

namespace kage::render {

// Framebuffer-pixel rectangle. The origin follows input coordinates
// (top-left); glViewportY() performs the OpenGL coordinate conversion.
struct ViewportRect final {
  glm::ivec2 origin{0};
  glm::ivec2 size{1};
  int framebuffer_height = 1;

  [[nodiscard]] glm::vec2 extent() const {
    return glm::vec2(std::max(size.x, 1), std::max(size.y, 1));
  }

  [[nodiscard]] bool contains(const glm::vec2& parPixel) const {
    return parPixel.x >= static_cast<float>(origin.x) &&
           parPixel.y >= static_cast<float>(origin.y) &&
           parPixel.x < static_cast<float>(origin.x + size.x) &&
           parPixel.y < static_cast<float>(origin.y + size.y);
  }

  [[nodiscard]] glm::vec2 toLocal(const glm::vec2& parPixel) const {
    return parPixel - glm::vec2(origin);
  }

  [[nodiscard]] int glViewportY() const {
    return std::max(framebuffer_height - origin.y - size.y, 0);
  }
};

}  // namespace kage::render
