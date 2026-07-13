#include "film/timeline_edit_service.hpp"

#include <algorithm>
#include <functional>
#include <limits>
#include <utility>

namespace {

using namespace kage::film;

[[nodiscard]] std::string firstError(const TimelineValidation& parValidation) {
  const auto error = std::find_if(
      parValidation.diagnostics.begin(), parValidation.diagnostics.end(),
      [](const TimelineDiagnostic& item) {
        return item.severity == TimelineDiagnostic::Severity::Error;
      });
  return error == parValidation.diagnostics.end() ? "Invalid timeline edit"
                                                   : error->message;
}

template <typename Value, typename Mutation>
[[nodiscard]] std::expected<Value, std::string> commit(
    MovieTimeline& parTimeline, Mutation&& parMutation) {
  MovieTimeline candidate = parTimeline;
  const std::expected<Value, std::string> result = parMutation(candidate);
  if (!result.has_value()) {
    return std::unexpected(result.error());
  }
  const TimelineValidation validation = validateMovieTimeline(candidate);
  if (validation.hasErrors()) {
    return std::unexpected(firstError(validation));
  }
  parTimeline = std::move(candidate);
  return result;
}

template <typename Mutation>
[[nodiscard]] std::expected<void, std::string> commitVoid(
    MovieTimeline& parTimeline, Mutation&& parMutation) {
  MovieTimeline candidate = parTimeline;
  const std::expected<void, std::string> result = parMutation(candidate);
  if (!result.has_value()) {
    return std::unexpected(result.error());
  }
  const TimelineValidation validation = validateMovieTimeline(candidate);
  if (validation.hasErrors()) {
    return std::unexpected(firstError(validation));
  }
  parTimeline = std::move(candidate);
  return {};
}

[[nodiscard]] int laneFor(const SequenceClipPayload& parPayload) {
  if (std::holds_alternative<MovementClip>(parPayload)) {
    return 0;
  }
  if (std::holds_alternative<RigAnimationClip>(parPayload)) {
    return 1;
  }
  return 2 + static_cast<int>(std::get<PropertyClip>(parPayload).kind);
}

[[nodiscard]] SequenceInstance* findInstance(MovieTimeline& parTimeline,
                                              SequenceInstanceId parId) {
  const auto found = std::find_if(parTimeline.instances.begin(),
                                  parTimeline.instances.end(),
                                  [parId](const SequenceInstance& item) {
                                    return item.id == parId;
                                  });
  return found == parTimeline.instances.end() ? nullptr : &*found;
}

[[nodiscard]] bool fitsWithinFilm(FilmFrame parStart, FilmFrame parDuration) {
  return parStart >= 0 && parDuration > 0 && parStart <= MAX_FILM_FRAMES &&
         parDuration <= MAX_FILM_FRAMES - parStart;
}

[[nodiscard]] std::uint64_t highestSequenceId(const MovieTimeline& parTimeline) {
  std::uint64_t highest = 0;
  for (const TargetSequence& sequence : parTimeline.sequences) {
    highest = std::max(highest, sequence.id);
  }
  return highest;
}

[[nodiscard]] std::uint64_t highestInstanceId(const MovieTimeline& parTimeline) {
  std::uint64_t highest = 0;
  for (const SequenceInstance& instance : parTimeline.instances) {
    highest = std::max(highest, instance.id);
  }
  return highest;
}

[[nodiscard]] std::uint64_t highestClipId(const MovieTimeline& parTimeline) {
  std::uint64_t highest = 0;
  for (const TargetSequence& sequence : parTimeline.sequences) {
    for (const SequenceClip& clip : sequence.clips) {
      highest = std::max(highest, clip.id);
    }
  }
  return highest;
}

[[nodiscard]] std::optional<std::uint64_t> allocateId(
    std::uint64_t& parNextId, std::uint64_t parHighestExistingId) {
  if (parHighestExistingId == std::numeric_limits<std::uint64_t>::max()) {
    return std::nullopt;
  }
  const std::uint64_t minimum = parHighestExistingId + 1;
  const std::uint64_t id = std::max(parNextId, minimum);
  if (id == 0) {
    return std::nullopt;
  }
  parNextId = id == std::numeric_limits<std::uint64_t>::max() ? 0 : id + 1;
  return id;
}

[[nodiscard]] std::optional<TargetSequenceId> allocateSequenceId(
    MovieTimeline& parTimeline) {
  return allocateId(parTimeline.next_sequence_id, highestSequenceId(parTimeline));
}

[[nodiscard]] std::optional<SequenceInstanceId> allocateInstanceId(
    MovieTimeline& parTimeline) {
  return allocateId(parTimeline.next_instance_id, highestInstanceId(parTimeline));
}

[[nodiscard]] std::optional<SequenceClipId> allocateClipId(
    MovieTimeline& parTimeline) {
  return allocateId(parTimeline.next_clip_id, highestClipId(parTimeline));
}

}  // namespace

namespace kage::film {

TimelineEditService::TimelineEditService(MovieTimeline& parTimeline)
    : timeline_(parTimeline) {}

std::expected<TargetSequenceId, std::string> TimelineEditService::createSequence(
    std::string parName, TimelineTarget parTarget,
    CapturedTargetBaseState parCapturedBase) {
  return commit<TargetSequenceId>(timeline_,
                                  [=, name = std::move(parName),
                                   base = std::move(parCapturedBase)](
                                      MovieTimeline& timeline) mutable {
    TargetSequence sequence;
    const auto id = allocateSequenceId(timeline);
    if (!id.has_value()) {
      return std::expected<TargetSequenceId, std::string>{
          std::unexpected("No sequence IDs remain")};
    }
    sequence.id = *id;
    sequence.name = std::move(name);
    sequence.target = parTarget;
    sequence.captured_base = std::move(base);
    timeline.sequences.push_back(std::move(sequence));
    return std::expected<TargetSequenceId, std::string>{timeline.sequences.back().id};
  });
}

std::expected<TargetSequenceId, std::string> TimelineEditService::duplicateSequence(
    TargetSequenceId parSequenceId) {
  return commit<TargetSequenceId>(timeline_, [parSequenceId](MovieTimeline& timeline) {
    const TargetSequence* source = timeline.findSequence(parSequenceId);
    if (source == nullptr) {
      return std::expected<TargetSequenceId, std::string>{
          std::unexpected("Sequence was not found")};
    }
    TargetSequence duplicate = *source;
    const auto sequence_id = allocateSequenceId(timeline);
    if (!sequence_id.has_value()) {
      return std::expected<TargetSequenceId, std::string>{
          std::unexpected("No sequence IDs remain")};
    }
    duplicate.id = *sequence_id;
    duplicate.name += " Copy";
    std::uint64_t highest_clip_id = highestClipId(timeline);
    for (SequenceClip& clip : duplicate.clips) {
      const auto clip_id = allocateId(timeline.next_clip_id, highest_clip_id);
      if (!clip_id.has_value()) {
        return std::expected<TargetSequenceId, std::string>{
            std::unexpected("No clip IDs remain")};
      }
      clip.id = *clip_id;
      highest_clip_id = *clip_id;
    }
    timeline.sequences.push_back(std::move(duplicate));
    return std::expected<TargetSequenceId, std::string>{timeline.sequences.back().id};
  });
}

std::expected<void, std::string> TimelineEditService::deleteSequence(
    TargetSequenceId parSequenceId) {
  return commitVoid(timeline_, [parSequenceId](MovieTimeline& timeline) {
    const std::size_t previous_size = timeline.sequences.size();
    std::erase_if(timeline.sequences, [parSequenceId](const TargetSequence& item) {
      return item.id == parSequenceId;
    });
    if (timeline.sequences.size() == previous_size) {
      return std::expected<void, std::string>{std::unexpected("Sequence was not found")};
    }
    std::erase_if(timeline.instances, [parSequenceId](const SequenceInstance& item) {
      return item.sequence_id == parSequenceId;
    });
    return std::expected<void, std::string>{};
  });
}

std::expected<void, std::string> TimelineEditService::recaptureBaseState(
    TargetSequenceId parSequenceId, CapturedTargetBaseState parCapturedBase) {
  return commitVoid(timeline_, [parSequenceId, base = std::move(parCapturedBase)](
                                   MovieTimeline& timeline) mutable {
    TargetSequence* sequence = timeline.findSequence(parSequenceId);
    if (sequence == nullptr) {
      return std::expected<void, std::string>{std::unexpected("Sequence was not found")};
    }
    sequence->captured_base = std::move(base);
    return std::expected<void, std::string>{};
  });
}

std::expected<SequenceClipId, std::string> TimelineEditService::appendClipToLane(
    TargetSequenceId parSequenceId, FilmFrame parDuration,
    SequenceClipPayload parPayload) {
  return commit<SequenceClipId>(timeline_,
                                [parSequenceId, parDuration,
                                 payload = std::move(parPayload)](
                                    MovieTimeline& timeline) mutable {
    if (parDuration <= 0) {
      return std::expected<SequenceClipId, std::string>{
          std::unexpected("Clip duration must be positive")};
    }
    TargetSequence* sequence = timeline.findSequence(parSequenceId);
    if (sequence == nullptr) {
      return std::expected<SequenceClipId, std::string>{
          std::unexpected("Sequence was not found")};
    }
    if (!isAuthorablePayloadForTarget(sequence->target.kind, payload)) {
      return std::expected<SequenceClipId, std::string>{
          std::unexpected("Clip is not authorable on this target type")};
    }
    FilmFrame start = 0;
    const int lane = laneFor(payload);
    for (const SequenceClip& clip : sequence->clips) {
      if (laneFor(clip.payload) == lane) {
        start = std::max(start, clip.end_frame);
      }
    }
    if (!fitsWithinFilm(start, parDuration)) {
      return std::expected<SequenceClipId, std::string>{
          std::unexpected("Clip exceeds the film frame limit")};
    }
    const auto id = allocateClipId(timeline);
    if (!id.has_value()) {
      return std::expected<SequenceClipId, std::string>{
          std::unexpected("No clip IDs remain")};
    }
    sequence->clips.push_back({*id, start, start + parDuration, std::move(payload)});
    return std::expected<SequenceClipId, std::string>{sequence->clips.back().id};
  });
}

std::expected<SequenceClipId, std::string> TimelineEditService::rippleInsertClip(
    TargetSequenceId parSequenceId, FilmFrame parStartFrame, FilmFrame parDuration,
    SequenceClipPayload parPayload) {
  return commit<SequenceClipId>(timeline_,
                                [parSequenceId, parStartFrame, parDuration,
                                 payload = std::move(parPayload)](
                                    MovieTimeline& timeline) mutable {
    if (parStartFrame < 0 || parDuration <= 0) {
      return std::expected<SequenceClipId, std::string>{
          std::unexpected("Clip range must be positive and non-negative")};
    }
    TargetSequence* sequence = timeline.findSequence(parSequenceId);
    if (sequence == nullptr) {
      return std::expected<SequenceClipId, std::string>{
          std::unexpected("Sequence was not found")};
    }
    if (!isAuthorablePayloadForTarget(sequence->target.kind, payload)) {
      return std::expected<SequenceClipId, std::string>{
          std::unexpected("Clip is not authorable on this target type")};
    }
    if (!fitsWithinFilm(parStartFrame, parDuration)) {
      return std::expected<SequenceClipId, std::string>{
          std::unexpected("Clip exceeds the film frame limit")};
    }
    const int lane = laneFor(payload);
    for (const SequenceClip& clip : sequence->clips) {
      if (laneFor(clip.payload) == lane && clip.start_frame < parStartFrame &&
          clip.end_frame > parStartFrame) {
        return std::expected<SequenceClipId, std::string>{
            std::unexpected("Cannot ripple into the middle of a clip")};
      }
      if (laneFor(clip.payload) == lane && clip.start_frame >= parStartFrame &&
          static_cast<std::int64_t>(clip.end_frame) +
                  static_cast<std::int64_t>(parDuration) >
              MAX_FILM_FRAMES) {
        return std::expected<SequenceClipId, std::string>{
            std::unexpected("Ripple insertion exceeds the film frame limit")};
      }
    }
    for (SequenceClip& clip : sequence->clips) {
      if (laneFor(clip.payload) == lane && clip.start_frame >= parStartFrame) {
        clip.start_frame += parDuration;
        clip.end_frame += parDuration;
      }
    }
    const auto id = allocateClipId(timeline);
    if (!id.has_value()) {
      return std::expected<SequenceClipId, std::string>{
          std::unexpected("No clip IDs remain")};
    }
    sequence->clips.push_back({*id, parStartFrame, parStartFrame + parDuration,
                               std::move(payload)});
    return std::expected<SequenceClipId, std::string>{*id};
  });
}

std::expected<void, std::string> TimelineEditService::moveClip(
    SequenceClipId parClipId, FilmFrame parStartFrame, FilmFrame parEndFrame) {
  return commitVoid(timeline_, [=](MovieTimeline& timeline) {
    SequenceClip* clip = timeline.findClip(parClipId);
    if (clip == nullptr) {
      return std::expected<void, std::string>{std::unexpected("Clip was not found")};
    }
    clip->start_frame = parStartFrame;
    clip->end_frame = parEndFrame;
    return std::expected<void, std::string>{};
  });
}

std::expected<void, std::string> TimelineEditService::trimClip(
    SequenceClipId parClipId, FilmFrame parStartFrame, FilmFrame parEndFrame) {
  return moveClip(parClipId, parStartFrame, parEndFrame);
}

std::expected<void, std::string> TimelineEditService::deleteClip(
    SequenceClipId parClipId) {
  return commitVoid(timeline_, [parClipId](MovieTimeline& timeline) {
    for (TargetSequence& sequence : timeline.sequences) {
      const std::size_t previous_size = sequence.clips.size();
      std::erase_if(sequence.clips, [parClipId](const SequenceClip& item) {
        return item.id == parClipId;
      });
      if (sequence.clips.size() != previous_size) {
        return std::expected<void, std::string>{};
      }
    }
    return std::expected<void, std::string>{std::unexpected("Clip was not found")};
  });
}

std::expected<void, std::string> TimelineEditService::setMovementStartMode(
    SequenceClipId parClipId, MovementStartMode parMode,
    std::optional<math::Transform> parExplicitStart) {
  return commitVoid(timeline_, [=](MovieTimeline& timeline) {
    SequenceClip* clip = timeline.findClip(parClipId);
    MovementClip* movement =
        clip == nullptr ? nullptr : std::get_if<MovementClip>(&clip->payload);
    if (movement == nullptr) {
      return std::expected<void, std::string>{std::unexpected("Movement clip was not found")};
    }
    if (parMode == MovementStartMode::ExplicitPosition && !parExplicitStart.has_value()) {
      return std::expected<void, std::string>{
          std::unexpected("Explicit movement starts require a transform")};
    }
    movement->start_mode = parMode;
    movement->explicit_start = parMode == MovementStartMode::ExplicitPosition
                                   ? parExplicitStart
                                   : std::nullopt;
    return std::expected<void, std::string>{};
  });
}

std::expected<void, std::string> TimelineEditService::setMovementTransition(
    SequenceClipId parClipId, MovementTransition parTransition) {
  return commitVoid(timeline_, [parClipId, transition = std::move(parTransition)](
                                   MovieTimeline& timeline) mutable {
    SequenceClip* clip = timeline.findClip(parClipId);
    MovementClip* movement =
        clip == nullptr ? nullptr : std::get_if<MovementClip>(&clip->payload);
    if (movement == nullptr) {
      return std::expected<void, std::string>{std::unexpected("Movement clip was not found")};
    }
    movement->transition_before = std::move(transition);
    return std::expected<void, std::string>{};
  });
}

std::expected<SequenceInstanceId, std::string> TimelineEditService::placeSequence(
    TargetSequenceId parSequenceId, FilmFrame parStartFrame) {
  return commit<SequenceInstanceId>(timeline_, [=](MovieTimeline& timeline) {
    const TargetSequence* sequence = timeline.findSequence(parSequenceId);
    if (sequence == nullptr || sequence->durationFrames() <= 0) {
      return std::expected<SequenceInstanceId, std::string>{
          std::unexpected("Only non-empty sequences can be placed")};
    }
    if (!fitsWithinFilm(parStartFrame, sequence->durationFrames())) {
      return std::expected<SequenceInstanceId, std::string>{
          std::unexpected("Instance exceeds the film frame limit")};
    }
    const auto id = allocateInstanceId(timeline);
    if (!id.has_value()) {
      return std::expected<SequenceInstanceId, std::string>{
          std::unexpected("No instance IDs remain")};
    }
    timeline.instances.push_back({*id, parSequenceId, parStartFrame});
    return std::expected<SequenceInstanceId, std::string>{*id};
  });
}

std::expected<SequenceInstanceId, std::string> TimelineEditService::duplicateInstance(
    SequenceInstanceId parInstanceId, FilmFrame parStartFrame) {
  return commit<SequenceInstanceId>(timeline_, [=](MovieTimeline& timeline) {
    const SequenceInstance* source = findInstance(timeline, parInstanceId);
    if (source == nullptr) {
      return std::expected<SequenceInstanceId, std::string>{
          std::unexpected("Instance was not found")};
    }
    const TargetSequence* sequence = timeline.findSequence(source->sequence_id);
    if (sequence == nullptr ||
        !fitsWithinFilm(parStartFrame, sequence->durationFrames())) {
      return std::expected<SequenceInstanceId, std::string>{
          std::unexpected("Instance exceeds the film frame limit")};
    }
    const auto id = allocateInstanceId(timeline);
    if (!id.has_value()) {
      return std::expected<SequenceInstanceId, std::string>{
          std::unexpected("No instance IDs remain")};
    }
    timeline.instances.push_back({*id, source->sequence_id, parStartFrame});
    return std::expected<SequenceInstanceId, std::string>{*id};
  });
}

std::expected<void, std::string> TimelineEditService::moveInstance(
    SequenceInstanceId parInstanceId, FilmFrame parStartFrame) {
  return commitVoid(timeline_, [=](MovieTimeline& timeline) {
    SequenceInstance* instance = findInstance(timeline, parInstanceId);
    if (instance == nullptr) {
      return std::expected<void, std::string>{std::unexpected("Instance was not found")};
    }
    const TargetSequence* sequence = timeline.findSequence(instance->sequence_id);
    if (sequence == nullptr ||
        !fitsWithinFilm(parStartFrame, sequence->durationFrames())) {
      return std::expected<void, std::string>{
          std::unexpected("Instance exceeds the film frame limit")};
    }
    instance->start_frame = parStartFrame;
    return std::expected<void, std::string>{};
  });
}

std::expected<void, std::string> TimelineEditService::deleteInstance(
    SequenceInstanceId parInstanceId) {
  return commitVoid(timeline_, [=](MovieTimeline& timeline) {
    const std::size_t previous_size = timeline.instances.size();
    std::erase_if(timeline.instances, [=](const SequenceInstance& item) {
      return item.id == parInstanceId;
    });
    if (timeline.instances.size() == previous_size) {
      return std::expected<void, std::string>{std::unexpected("Instance was not found")};
    }
    return std::expected<void, std::string>{};
  });
}

TimelineValidation TimelineEditService::validateAuthoring() const {
  return validateMovieTimeline(timeline_);
}

TimelineValidation TimelineEditService::validateForBake() const {
  return validateMovieTimeline(timeline_, true);
}

}  // namespace kage::film
