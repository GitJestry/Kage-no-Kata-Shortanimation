#pragma once

#include "editor/editor_session.hpp"
#include "engine/engine_core.hpp"

#include <string>

namespace kage::editor {

void drawSequenceTimeline(engine::EngineCore& parEngine, EditorSession& parSession,
                          bool parFitToSequence, int parZoomDirection,
                          std::string& parError);

}  // namespace kage::editor
