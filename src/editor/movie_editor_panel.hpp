#pragma once

#include "editor/editor_session.hpp"
#include "editor/movie_editor_layout.hpp"
#include "editor/ui_panel_rect.hpp"
#include "engine/engine_core.hpp"

#include <glm/glm.hpp>

#include <vector>

namespace kage::editor {

[[nodiscard]] std::vector<UiPanelRect> drawMovieEditorPanel(
    engine::EngineCore& parEngine, EditorSession& parSession,
    const MovieEditorLayout& parLayout);

}  // namespace kage::editor
