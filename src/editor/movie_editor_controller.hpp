#pragma once

#include "editor/editor_session.hpp"
#include "scene/world.hpp"

#include <optional>
#include <vector>

namespace kage::engine {
class EngineCore;
}

namespace kage::editor {

// Returns only targets which Movie mode can animate.  Static meshes and
// unsupported lights deliberately have no Movie selection representation.
[[nodiscard]] std::optional<film::TimelineTarget> movieTargetForEntity(
    const scene::EntityRecord& parEntity);
[[nodiscard]] bool isMovieTargetOrphaned(
    const scene::World& parWorld, const film::TimelineTarget& parTarget);
[[nodiscard]] std::vector<film::TimelineTarget> orphanMovieTargetsForKind(
    const scene::World& parWorld, const film::MovieTimeline& parTimeline,
    film::TimelineTargetKind parKind);
[[nodiscard]] std::optional<film::ResolvedMovementPath>
selectedMovementPath(const film::MovieTimeline& parTimeline,
                     const MovieEditorSelection& parSelection);
[[nodiscard]] std::vector<film::ResolvedMovementPath> sequenceMovementPaths(
    const film::MovieTimeline& parTimeline,
    film::TargetSequenceId parSequenceId);
[[nodiscard]] bool initializeCustomMovementCurve(
    const film::TargetSequence& parSequence, film::SequenceClipId parClipId,
    film::MovementCurve& parCurve);
[[nodiscard]] bool initializeCustomMovementTransitionCurve(
    const film::TargetSequence& parSequence, film::SequenceClipId parClipId,
    film::MovementCurve& parCurve);
[[nodiscard]] bool movementTransitionAvailable(
    const film::TargetSequence& parSequence,
    const film::SequenceClip& parClip);

// The editor cursor is an inclusive authoring position, while preview playback
// can only consume the half-open frame range [0, duration).
[[nodiscard]] film::FilmFrame clampMovieAuthoringCursor(
    film::FilmFrame parFrame);
[[nodiscard]] film::FilmFrame moviePlaybackFrameForCursor(
    film::FilmFrame parCursorFrame, film::FilmFrame parDuration);
void setMovieAuthoringCursor(EditorSession& parSession,
                             film::FilmPlayback& parPlayback,
                             film::FilmFrame parDuration,
                             film::FilmFrame parCursorFrame);

// This is the single reset for Movie-only preview state.  It deliberately
// preserves selection and the authoring cursor while returning the viewport to
// normal editor-camera rendering.
void resetMoviePreview(EditorSession& parSession,
                       film::FilmPlayback& parPlayback);
// UI actions use this overload so the Engine's evaluated frame, viewport
// camera, and skin-palette caches are reset with the session state.
void resetMoviePreview(engine::EngineCore& parEngine,
                       EditorSession& parSession);

void selectMovieTarget(EditorSession& parSession,
                       const film::TimelineTarget& parTarget);
// Target list and Movie-viewport selection choose the existing sequence for
// the target, so they always open a usable Target Sequence Timeline when one
// exists.  A target without sequences still opens the target context, where a
// sequence can be created.
void selectMovieTarget(EditorSession& parSession,
                       const film::MovieTimeline& parTimeline,
                       const film::TimelineTarget& parTarget);
void selectMovieSequence(EditorSession& parSession,
                         const film::TargetSequence& parSequence);
void selectMovieInstance(EditorSession& parSession,
                         film::SequenceInstanceId parInstanceId);
[[nodiscard]] bool openMovieInstanceSequence(
    EditorSession& parSession, const film::MovieTimeline& parTimeline,
    film::SequenceInstanceId parInstanceId);
void selectCreatedFilmCamera(EditorSession& parSession,
                             scene::EntityId parEntity,
                             film::TargetSequenceId parSequenceId,
                             film::SequenceInstanceId parInstanceId);
void handleMovieViewportPick(
    EditorSession& parSession, const film::MovieTimeline& parTimeline,
    const std::optional<film::TimelineTarget>& parPickedTarget);
void deselectMovieTarget(EditorSession& parSession);
void deselectMovieTarget(engine::EngineCore& parEngine,
                         EditorSession& parSession);

void updateMoviePreviewContext(EditorSession& parSession,
                               const film::MovieTimeline& parTimeline,
                               film::FilmPlayback& parPlayback);
[[nodiscard]] film::FilmFrame moviePreviewDuration(
    const EditorSession& parSession, const film::MovieTimeline& parTimeline);
[[nodiscard]] bool toggleMoviePlayback(
    EditorSession& parSession, const film::MovieTimeline& parTimeline,
    film::FilmPlayback& parPlayback);
void synchronizeMovieAuthoringCursor(
    EditorSession& parSession, const film::FilmPlayback& parPlayback);

}  // namespace kage::editor
