#pragma once

#include "editor/file_browser_dialog.hpp"
#include "editor/ui_panel_rect.hpp"
#include "engine/engine_core.hpp"

#include <glm/glm.hpp>

#include <array>
#include <optional>
#include <string>

namespace kage::editor {

[[nodiscard]] std::optional<UiPanelRect> drawTimelinePanel(
    engine::EngineCore& parEngine, bool& parVisible,
    const glm::vec2& parViewportSize, FileBrowserDialog& parImportBrowser,
    std::array<char, 128>& parImportLabelBuffer, std::string& parImportError);

}  // namespace kage::editor
