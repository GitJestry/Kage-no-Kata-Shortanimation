#pragma once

#include "editor/editor_session.hpp"
#include "scene/world.hpp"

#include <optional>

namespace kage::engine {
class EngineCore;
}

namespace kage::editor {

// Returns only targets which Movie mode can animate. Static meshes have no
// Movie selection representation.
[[nodiscard]] std::optional<film::TimelineTarget> movieTargetForEntity(
    const scene::EntityRecord& parEntity);
[[nodiscard]] bool movementTransitionAvailable(
    const film::TargetSequence& parSequence,
    const film::SequenceClip& parClip);

// The editor cursor is an inclusive authoring position, while preview playback
// can only consume the half-open frame range [0, duration).
[[nodiscard]] film::FilmFrame clampMovieAuthoringCursor(
    film::FilmFrame parFrame);
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
