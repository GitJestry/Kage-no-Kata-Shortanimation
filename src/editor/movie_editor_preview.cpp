#include "editor/movie_editor_controller.hpp"

#include "engine/engine_core.hpp"

namespace kage::editor {

void resetMoviePreview(engine::EngineCore& parEngine,
                       EditorSession& parSession) {
  resetMoviePreview(parSession, parEngine.getFilmPlayback());
  parEngine.clearFilmPreviewState();
}

void deselectMovieTarget(engine::EngineCore& parEngine,
                         EditorSession& parSession) {
  resetMoviePreview(parEngine, parSession);
  deselectMovieTarget(parSession);
}

}  // namespace kage::editor
