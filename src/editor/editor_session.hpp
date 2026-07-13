#pragma once

#include "film/film_sequence.hpp"

#include <filesystem>

namespace kage::editor {

enum class Workspace {
  WorldEdit,
  Movie,
};

struct EditorSession final {
  Workspace workspace = Workspace::WorldEdit;
  bool shot_preview = false;
  bool solo_clip_preview = false;
  film::FilmClipId selected_film_clip = 0;
  float film_editor_height = 260.0f;
};

void loadEditorSession(const std::filesystem::path& parPath,
                       EditorSession& parSession);
void saveEditorSession(const std::filesystem::path& parPath,
                       const EditorSession& parSession);

}  // namespace kage::editor
