#pragma once

#include "engine/engine_core.hpp"

#include <glm/vec3.hpp>

#include <cstddef>
#include <vector>

namespace kage::editor {

class PaintbrushTool final {
 public:
  void drawUi(engine::EngineCore& parEngine);
  void paintAssets(engine::EngineCore& parEngine,
                   const glm::vec3& parCenter,
                   const std::vector<std::size_t>& parAssetIndices) const;

  [[nodiscard]] bool isEnabled() const;
  [[nodiscard]] int getBrushSize() const;
  [[nodiscard]] int getPaintDensity() const;
  [[nodiscard]] std::vector<std::size_t> getSelectedAssetIndices() const;

 private:
  bool m_enabled = false;
  bool m_randomize_scale = false;
  bool m_randomize_rotation = false;
  int m_brush_size = 4;
  int m_paint_density = 3;
  std::vector<bool> m_selected_assets;
};

}  // namespace kage::editor
