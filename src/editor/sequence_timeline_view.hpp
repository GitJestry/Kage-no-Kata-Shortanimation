#pragma once

#include "editor/editor_session.hpp"
#include "engine/engine_core.hpp"

namespace kage::editor {

void drawSequenceTimelineView(engine::EngineCore& parEngine,
                              EditorSession& parSession,
                              bool parFitToSequence, int parZoomDirection);

}  // namespace kage::editor
