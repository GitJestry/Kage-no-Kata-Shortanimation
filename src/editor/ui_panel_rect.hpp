#pragma once

#include <glm/glm.hpp>

namespace kage::editor {

struct UiPanelRect final {
  glm::vec2 min{0.0f};
  glm::vec2 max{0.0f};

  [[nodiscard]] bool contains(const glm::vec2& parPoint) const {
    return parPoint.x >= min.x && parPoint.x <= max.x &&
           parPoint.y >= min.y && parPoint.y <= max.y;
  }
};

}  // namespace kage::editor
