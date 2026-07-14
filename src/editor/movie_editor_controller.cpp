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
  if (parEntity.light.has_value() &&
      parEntity.light->type == scene::LightType::Point) {
    return film::TimelineTarget{film::TimelineTargetKind::PointLight,
                                parEntity.id};
  }
  if (parEntity.rig.has_value()) {
    return film::TimelineTarget{film::TimelineTargetKind::RiggedEntity,
                                parEntity.id};
  }
  return std::nullopt;
}

bool isMovieTargetOrphaned(const scene::World& parWorld,
                           const film::TimelineTarget& parTarget) {
  return parTarget.kind != film::TimelineTargetKind::Sun &&
         parWorld.findEntity(parTarget.entity) == nullptr;
}

std::vector<film::TimelineTarget> orphanMovieTargetsForKind(
    const scene::World& parWorld, const film::MovieTimeline& parTimeline,
    film::TimelineTargetKind parKind) {
  std::vector<film::TimelineTarget> targets;
  for (const film::TargetSequence& sequence : parTimeline.sequences) {
    if (sequence.target.kind != parKind ||
        !isMovieTargetOrphaned(parWorld, sequence.target) ||
        std::find(targets.begin(), targets.end(), sequence.target) !=
            targets.end()) {
      continue;
    }
    targets.push_back(sequence.target);
  }
  return targets;
}

std::optional<film::ResolvedMovementPath> selectedMovementPath(
    const film::MovieTimeline& parTimeline,
    const MovieEditorSelection& parSelection) {
  if (!parSelection.target.has_value() || parSelection.sequence_id == 0 ||
      parSelection.clip_id == 0) {
    return std::nullopt;
  }
  const film::TargetSequence* sequence =
      parTimeline.findSequence(parSelection.sequence_id);
  if (sequence == nullptr || sequence->target != *parSelection.target) {
    return std::nullopt;
  }
  return film::resolveMovementPath(*sequence, parSelection.clip_id);
}

std::vector<film::ResolvedMovementPath> sequenceMovementPaths(
    const film::MovieTimeline& parTimeline,
    film::TargetSequenceId parSequenceId) {
  const film::TargetSequence* sequence =
      parTimeline.findSequence(parSequenceId);
  if (sequence == nullptr) {
    return {};
  }

  std::vector<const film::SequenceClip*> movement_clips;
  movement_clips.reserve(sequence->clips.size());
  for (const film::SequenceClip& clip : sequence->clips) {
    if (std::holds_alternative<film::MovementClip>(clip.payload)) {
      movement_clips.push_back(&clip);
    }
  }
  std::sort(movement_clips.begin(), movement_clips.end(),
            [](const film::SequenceClip* parLeft,
               const film::SequenceClip* parRight) {
              if (parLeft->start_frame != parRight->start_frame) {
                return parLeft->start_frame < parRight->start_frame;
              }
              return parLeft->id < parRight->id;
            });

  std::vector<film::ResolvedMovementPath> paths;
  paths.reserve(movement_clips.size());
  for (const film::SequenceClip* clip : movement_clips) {
    std::optional<film::ResolvedMovementPath> path =
        film::resolveMovementPath(*sequence, clip->id);
    if (path.has_value()) {
      paths.push_back(std::move(*path));
    }
  }
  return paths;
}

bool initializeCustomMovementCurve(const film::TargetSequence& parSequence,
                                   film::SequenceClipId parClipId,
                                   film::MovementCurve& parCurve) {
  const std::optional<film::ResolvedMovementPath> path =
      film::resolveMovementPath(parSequence, parClipId);
  if (!path.has_value()) {
    return false;
  }
  parCurve.position_control_1 = path->movement.control_1;
  parCurve.position_control_2 = path->movement.control_2;
  parCurve.automatic_position_controls = false;
  return true;
}

bool initializeCustomMovementTransitionCurve(
    const film::TargetSequence& parSequence, film::SequenceClipId parClipId,
    film::MovementCurve& parCurve) {
  const std::optional<film::ResolvedMovementPath> path =
      film::resolveMovementPath(parSequence, parClipId);
  if (!path.has_value() || !path->transition_before.has_value()) {
    return false;
  }
  parCurve.position_control_1 = path->transition_before->control_1;
  parCurve.position_control_2 = path->transition_before->control_2;
  parCurve.automatic_position_controls = false;
  return true;
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

film::FilmFrame moviePlaybackFrameForCursor(film::FilmFrame parCursorFrame,
                                            film::FilmFrame parDuration) {
  if (parDuration <= 0) {
    return 0;
  }
  return std::clamp(clampMovieAuthoringCursor(parCursorFrame),
                    film::FilmFrame{0}, parDuration - 1);
}

void setMovieAuthoringCursor(EditorSession& parSession,
                             film::FilmPlayback& parPlayback,
                             film::FilmFrame parDuration,
                             film::FilmFrame parCursorFrame) {
  parSession.authoring_cursor_frame = clampMovieAuthoringCursor(parCursorFrame);
  parPlayback.playing = false;
  parPlayback.playhead_frame = static_cast<double>(
      moviePlaybackFrameForCursor(parSession.authoring_cursor_frame,
                                  parDuration));
  if (parDuration <= 0) {
    resetMoviePreview(parSession, parPlayback);
    return;
  }
  parPlayback.previewing = true;
}

void resetMoviePreview(EditorSession& parSession,
                       film::FilmPlayback& parPlayback) {
  parPlayback.playing = false;
  parPlayback.previewing = false;
  parSession.shot_preview = false;
  parSession.shot_preview_sequence_id = 0;
}

void selectMovieTarget(EditorSession& parSession,
                       const film::TimelineTarget& parTarget) {
  parSession.movie_selection.target = parTarget;
  parSession.movie_selection.sequence_id = 0;
  parSession.movie_selection.clip_id = 0;
  parSession.movie_selection.instance_id = 0;
}

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

bool openMovieInstanceSequence(EditorSession& parSession,
                               const film::MovieTimeline& parTimeline,
                               film::SequenceInstanceId parInstanceId) {
  const auto instance = std::find_if(
      parTimeline.instances.begin(), parTimeline.instances.end(),
      [parInstanceId](const film::SequenceInstance& candidate) {
        return candidate.id == parInstanceId;
      });
  if (instance == parTimeline.instances.end()) {
    return false;
  }
  const film::TargetSequence* sequence =
      parTimeline.findSequence(instance->sequence_id);
  if (sequence == nullptr) {
    return false;
  }
  selectMovieSequence(parSession, *sequence);
  parSession.movie_selection.instance_id = parInstanceId;
  return true;
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

void handleMovieViewportPick(
    EditorSession& parSession, const film::MovieTimeline& parTimeline,
    const std::optional<film::TimelineTarget>& parPickedTarget) {
  if (parPickedTarget.has_value()) {
    selectMovieTarget(parSession, parTimeline, *parPickedTarget);
  }
}

void deselectMovieTarget(EditorSession& parSession) {
  parSession.movie_selection.clear();
}

void updateMoviePreviewContext(EditorSession& parSession,
                               const film::MovieTimeline& parTimeline,
                               film::FilmPlayback& parPlayback) {
  if (!parPlayback.previewing) {
    parSession.shot_preview = false;
    parSession.shot_preview_sequence_id = 0;
    return;
  }
  if (!parSession.movie_selection.target.has_value()) {
    parSession.shot_preview = false;
    parSession.shot_preview_sequence_id = 0;
    return;
  }
  const film::TargetSequence* sequence =
      parTimeline.findSequence(parSession.movie_selection.sequence_id);
  if (sequence == nullptr ||
      sequence->target != *parSession.movie_selection.target ||
      sequence->durationFrames() <= 0) {
    resetMoviePreview(parSession, parPlayback);
    return;
  }
  parSession.shot_preview_sequence_id = sequence->id;
  parSession.shot_preview = film::isCameraSequence(*sequence);
}

film::FilmFrame moviePreviewDuration(const EditorSession& parSession,
                                     const film::MovieTimeline& parTimeline) {
  if (!parSession.movie_selection.target.has_value()) {
    return parTimeline.durationFrames();
  }
  const film::TargetSequence* sequence = parTimeline.findSequence(
      parSession.movie_selection.sequence_id);
  if (sequence != nullptr &&
      sequence->target == *parSession.movie_selection.target) {
    return sequence->durationFrames();
  }
  return 0;
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
    resetMoviePreview(parSession, parPlayback);
    return false;
  }
  if (!std::isfinite(parPlayback.playhead_frame) ||
      parPlayback.playhead_frame < 0.0 ||
      parPlayback.playhead_frame >= static_cast<double>(duration)) {
    parPlayback.playhead_frame = 0.0;
  }
  parPlayback.playing = true;
  parPlayback.previewing = true;
  updateMoviePreviewContext(parSession, parTimeline, parPlayback);
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
