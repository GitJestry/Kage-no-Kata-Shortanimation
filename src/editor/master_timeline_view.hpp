#pragma once

#include "editor/editor_session.hpp"
#include "engine/engine_core.hpp"

namespace kage::editor {

void drawMasterTimelineView(engine::EngineCore& parEngine,
                            EditorSession& parSession,
                            bool parFitToMovie, int parZoomDirection);

}  // namespace kage::editor
