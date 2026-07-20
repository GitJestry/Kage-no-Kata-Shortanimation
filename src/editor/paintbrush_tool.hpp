#pragma once

#include "editor/paintbrush_settings.hpp"
#include "engine/engine_core.hpp"

#include <glm/vec3.hpp>

#include <cstddef>
#include <vector>

namespace kage::editor {

class PaintbrushTool final {
public:
  void drawUi(engine::EngineCore& parEngine);

  [[nodiscard]] bool isEnabled() const;
  [[nodiscard]] const PaintbrushSettings& getSettings() const;
  [[nodiscard]] std::vector<std::size_t> getSelectedAssetIndices() const;

private:
  bool m_enabled = false;
  PaintbrushSettings m_settings;
  std::vector<bool> m_selected_assets;
};

} // namespace kage::editor
