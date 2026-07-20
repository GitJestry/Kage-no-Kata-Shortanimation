#pragma once

#include "editor/editor_session.hpp"
#include "engine/engine_core.hpp"

#include <string>

namespace kage::editor {

void drawMovieTargetList(engine::EngineCore& parEngine, EditorSession& parSession,
                         std::string& parError);

}  // namespace kage::editor
