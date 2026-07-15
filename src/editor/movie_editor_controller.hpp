#pragma once

#include "editor/editor_session.hpp"
#include "scene/world.hpp"

#include <optional>

namespace kage::editor {

// Maps a live entity to one of the entity-backed Movie targets:
// camera, point light, or rigged entity.
[[nodiscard]] std::optional<film::TimelineTarget> movieTargetForEntity(
    const scene::EntityRecord& parEntity);

// The editor cursor is an inclusive authoring position, while preview playback
// can only consume the half-open frame range [0, duration).
[[nodiscard]] film::FilmFrame clampMovieAuthoringCursor(
    film::FilmFrame parFrame);
void setMovieAuthoringCursor(EditorSession& parSession,
                             film::FilmPlayback& parPlayback,
                             film::FilmFrame parDuration,
                             film::FilmFrame parCursorFrame);

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
void selectCreatedFilmCamera(EditorSession& parSession,
                             scene::EntityId parEntity,
                             film::TargetSequenceId parSequenceId,
                             film::SequenceInstanceId parInstanceId);
void deselectMovieTarget(EditorSession& parSession);

[[nodiscard]] const film::TargetSequence* selectedMovieTargetSequence(
    const EditorSession& parSession, const film::MovieTimeline& parTimeline);
[[nodiscard]] film::FilmFrame moviePreviewDuration(
    const EditorSession& parSession, const film::MovieTimeline& parTimeline);
[[nodiscard]] bool toggleMoviePlayback(
    EditorSession& parSession, const film::MovieTimeline& parTimeline,
    film::FilmPlayback& parPlayback);
void synchronizeMovieAuthoringCursor(
    EditorSession& parSession, const film::FilmPlayback& parPlayback);

}  // namespace kage::editor
