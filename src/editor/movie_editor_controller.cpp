#include "editor/movie_editor_controller.hpp"

#include <algorithm>
#include <cmath>

namespace kage::editor {

std::optional<film::TimelineTarget> movieTargetForEntity(
    const scene::EntityRecord& parEntity) {
  if (!parEntity.alive) {
    return std::nullopt;
  }
  if (parEntity.camera.has_value()) {
    return film::TimelineTarget{film::TimelineTargetKind::Camera, parEntity.id};
  }
  if (parEntity.light.has_value()) {
    return film::TimelineTarget{film::TimelineTargetKind::PointLight,
                                parEntity.id};
  }
  if (parEntity.rig.has_value()) {
    return film::TimelineTarget{film::TimelineTargetKind::RiggedEntity,
                                parEntity.id};
  }
  return std::nullopt;
}

bool movementTransitionAvailable(const film::TargetSequence& parSequence,
                                 const film::SequenceClip& parClip) {
  const auto* movement = std::get_if<film::MovementClip>(&parClip.payload);
  if (movement == nullptr ||
      movement->start_mode != film::MovementStartMode::ExplicitPosition ||
      !movement->explicit_start.has_value()) {
    return false;
  }
  std::optional<film::FilmFrame> previous_end;
  for (const film::SequenceClip& candidate : parSequence.clips) {
    if (candidate.id == parClip.id ||
        !std::holds_alternative<film::MovementClip>(candidate.payload) ||
        candidate.end_frame > parClip.start_frame) {
      continue;
    }
    previous_end = previous_end.has_value()
                       ? std::max(*previous_end, candidate.end_frame)
                       : candidate.end_frame;
  }
  return previous_end.has_value() && *previous_end < parClip.start_frame;
}

film::FilmFrame clampMovieAuthoringCursor(film::FilmFrame parFrame) {
  return std::clamp(parFrame, film::FilmFrame{0}, film::MAX_FILM_FRAMES);
}

void setMovieAuthoringCursor(EditorSession& parSession,
                             film::FilmPlayback& parPlayback,
                             film::FilmFrame parDuration,
                             film::FilmFrame parCursorFrame) {
  parSession.authoring_cursor_frame = clampMovieAuthoringCursor(parCursorFrame);
  parPlayback.playing = false;
  parPlayback.playhead_frame = parDuration <= 0 ? 0.0 : static_cast<double>(
      std::clamp(parSession.authoring_cursor_frame, film::FilmFrame{0},
                 parDuration - 1));
  if (parDuration <= 0) {
    parPlayback.stop();
    return;
  }
  parPlayback.previewing = true;
}

namespace {

void selectMovieTarget(EditorSession& parSession,
                       const film::TimelineTarget& parTarget) {
  parSession.movie_selection.target = parTarget;
  parSession.movie_selection.sequence_id = 0;
  parSession.movie_selection.clip_id = 0;
  parSession.movie_selection.instance_id = 0;
}

}  // namespace

void selectMovieTarget(EditorSession& parSession,
                       const film::MovieTimeline& parTimeline,
                       const film::TimelineTarget& parTarget) {
  const film::TargetSequence* current_sequence =
      parTimeline.findSequence(parSession.movie_selection.sequence_id);
  selectMovieTarget(parSession, parTarget);
  if (current_sequence != nullptr && current_sequence->target == parTarget) {
    parSession.movie_selection.sequence_id = current_sequence->id;
    return;
  }

  const auto sequence = std::find_if(
      parTimeline.sequences.begin(), parTimeline.sequences.end(),
      [&parTarget](const film::TargetSequence& candidate) {
        return candidate.target == parTarget;
      });
  if (sequence != parTimeline.sequences.end()) {
    parSession.movie_selection.sequence_id = sequence->id;
  }
}

void selectMovieSequence(EditorSession& parSession,
                         const film::TargetSequence& parSequence) {
  selectMovieTarget(parSession, parSequence.target);
  parSession.movie_selection.sequence_id = parSequence.id;
}

void selectMovieInstance(EditorSession& parSession,
                         film::SequenceInstanceId parInstanceId) {
  parSession.movie_selection.clear();
  parSession.movie_selection.instance_id = parInstanceId;
}

void selectCreatedFilmCamera(EditorSession& parSession,
                             scene::EntityId parEntity,
                             film::TargetSequenceId parSequenceId,
                             film::SequenceInstanceId parInstanceId) {
  selectMovieTarget(
      parSession, {film::TimelineTargetKind::Camera, parEntity});
  parSession.movie_selection.sequence_id = parSequenceId;
  parSession.movie_selection.instance_id = parInstanceId;
}

void deselectMovieTarget(EditorSession& parSession) {
  parSession.movie_selection.clear();
}

const film::TargetSequence* selectedMovieTargetSequence(
    const EditorSession& parSession, const film::MovieTimeline& parTimeline) {
  if (!parSession.movie_selection.target.has_value()) {
    return nullptr;
  }
  const film::TargetSequence* sequence =
      parTimeline.findSequence(parSession.movie_selection.sequence_id);
  return sequence != nullptr &&
                 sequence->target == *parSession.movie_selection.target
             ? sequence
             : nullptr;
}

film::FilmFrame moviePreviewDuration(const EditorSession& parSession,
                                     const film::MovieTimeline& parTimeline) {
  if (!parSession.movie_selection.target.has_value()) {
    return parTimeline.durationFrames();
  }
  const film::TargetSequence* sequence =
      selectedMovieTargetSequence(parSession, parTimeline);
  return sequence != nullptr ? sequence->durationFrames() : 0;
}

bool toggleMoviePlayback(EditorSession& parSession,
                         const film::MovieTimeline& parTimeline,
                         film::FilmPlayback& parPlayback) {
  if (parPlayback.playing) {
    parPlayback.playing = false;
    return false;
  }
  const film::FilmFrame duration =
      moviePreviewDuration(parSession, parTimeline);
  if (duration <= 0) {
    parPlayback.playhead_frame = 0.0;
    parPlayback.stop();
    return false;
  }
  if (!std::isfinite(parPlayback.playhead_frame) ||
      parPlayback.playhead_frame < 0.0 ||
      parPlayback.playhead_frame >= static_cast<double>(duration)) {
    parPlayback.playhead_frame = 0.0;
  }
  parPlayback.playing = true;
  parPlayback.previewing = true;
  return true;
}

void synchronizeMovieAuthoringCursor(
    EditorSession& parSession, const film::FilmPlayback& parPlayback) {
  const double frame = std::isfinite(parPlayback.playhead_frame)
                           ? std::floor(parPlayback.playhead_frame)
                           : 0.0;
  parSession.authoring_cursor_frame = clampMovieAuthoringCursor(
      static_cast<film::FilmFrame>(std::clamp(
          frame, 0.0, static_cast<double>(film::MAX_FILM_FRAMES))));
}

}  // namespace kage::editor
