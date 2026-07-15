#pragma once

#include "editor/movie_editor_layout.hpp"
#include "film/movie_timeline.hpp"

#include <optional>

namespace kage::editor {

enum class Workspace {
  WorldEdit,
  Movie,
};

struct MovieEditorSelection final {
  std::optional<film::TimelineTarget> target;
  film::TargetSequenceId sequence_id = 0;
  film::SequenceClipId clip_id = 0;
  film::SequenceInstanceId instance_id = 0;

  void clear() {
    target.reset();
    sequence_id = 0;
    clip_id = 0;
    instance_id = 0;
  }
};

struct EditorSession final {
  Workspace workspace = Workspace::WorldEdit;
  film::TargetSequenceId shown_movement_paths_sequence_id = 0;
  MovieEditorSelection movie_selection;
  MovieEditorLayout movie_layout;
  float target_sequence_pixels_per_frame = 12.0f;
  // Authoring is inclusive so an insertion can target the frame immediately
  // after a movie's exclusive end. Playback remains bounded by its duration.
  film::FilmFrame authoring_cursor_frame = 0;
  float film_editor_height = 260.0f;
};

}  // namespace kage::editor
