#pragma once

#include "film/movie_timeline.hpp"

#include <expected>
#include <string>

namespace kage::film {

// All mutations are performed against a copy and committed only after the
// structural authoring validation succeeds.  This keeps failed edits atomic
// and leaves UI code with only command dispatch to perform.
class TimelineEditService final {
 public:
  explicit TimelineEditService(MovieTimeline& parTimeline);

  [[nodiscard]] std::expected<TargetSequenceId, std::string> createSequence(
      std::string parName, TimelineTarget parTarget,
      CapturedTargetBaseState parCapturedBase);
  [[nodiscard]] std::expected<TargetSequenceId, std::string> duplicateSequence(
      TargetSequenceId parSequenceId);
  [[nodiscard]] std::expected<void, std::string> deleteSequence(
      TargetSequenceId parSequenceId);
  [[nodiscard]] std::expected<void, std::string> recaptureBaseState(
      TargetSequenceId parSequenceId, CapturedTargetBaseState parCapturedBase);

  [[nodiscard]] std::expected<SequenceClipId, std::string> appendClipToLane(
      TargetSequenceId parSequenceId, FilmFrame parDuration,
      SequenceClipPayload parPayload);
  [[nodiscard]] std::expected<SequenceClipId, std::string> rippleInsertClip(
      TargetSequenceId parSequenceId, FilmFrame parStartFrame,
      FilmFrame parDuration, SequenceClipPayload parPayload);
  [[nodiscard]] std::expected<void, std::string> moveClip(
      SequenceClipId parClipId, FilmFrame parStartFrame, FilmFrame parEndFrame);
  [[nodiscard]] std::expected<void, std::string> trimClip(
      SequenceClipId parClipId, FilmFrame parStartFrame, FilmFrame parEndFrame);
  [[nodiscard]] std::expected<void, std::string> deleteClip(SequenceClipId parClipId);
  [[nodiscard]] std::expected<void, std::string> setMovementStartMode(
      SequenceClipId parClipId, MovementStartMode parMode,
      std::optional<math::Transform> parExplicitStart = std::nullopt);
  [[nodiscard]] std::expected<void, std::string> setMovementTransition(
      SequenceClipId parClipId, MovementTransition parTransition);

  [[nodiscard]] std::expected<SequenceInstanceId, std::string> placeSequence(
      TargetSequenceId parSequenceId, FilmFrame parStartFrame);
  [[nodiscard]] std::expected<SequenceInstanceId, std::string> duplicateInstance(
      SequenceInstanceId parInstanceId, FilmFrame parStartFrame);
  [[nodiscard]] std::expected<void, std::string> moveInstance(
      SequenceInstanceId parInstanceId, FilmFrame parStartFrame);
  [[nodiscard]] std::expected<void, std::string> deleteInstance(
      SequenceInstanceId parInstanceId);

  [[nodiscard]] TimelineValidation validateAuthoring() const;
  [[nodiscard]] TimelineValidation validateForBake() const;

 private:
  MovieTimeline& timeline_;
};

}  // namespace kage::film
