#include "film/timeline_edit_service.hpp"

#include <algorithm>
#include <functional>
#include <limits>
#include <utility>

namespace {

using namespace kage::film;

template <typename Value = void>
[[nodiscard]] std::expected<Value, std::string> fail(std::string parMessage) {
  return std::unexpected(std::move(parMessage));
}

[[nodiscard]] std::string firstError(const TimelineValidation& parValidation) {
  const TimelineDiagnostic* error = parValidation.firstError();
  return error == nullptr ? "Invalid timeline edit" : error->message;
}

template <typename Mutation>
[[nodiscard]] auto commit(MovieTimeline& parTimeline, Mutation&& parMutation)
    -> decltype(parMutation(parTimeline)) {
  MovieTimeline candidate = parTimeline;
  const auto result = parMutation(candidate);
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

struct LocatedClip final {
  TargetSequence* sequence = nullptr;
  SequenceClip* clip = nullptr;
};

[[nodiscard]] LocatedClip locateClip(MovieTimeline& parTimeline,
                                     SequenceClipId parId) {
  for (TargetSequence& sequence : parTimeline.sequences) {
    const auto found = std::find_if(sequence.clips.begin(), sequence.clips.end(),
                                    [parId](const SequenceClip& item) {
                                      return item.id == parId;
                                    });
    if (found != sequence.clips.end()) {
      return {&sequence, &*found};
    }
  }
  return {};
}

[[nodiscard]] bool fitsWithinFilm(FilmFrame parStart, FilmFrame parDuration) {
  return parStart >= 0 && parDuration > 0 && parStart <= MAX_FILM_FRAMES &&
         parDuration <= MAX_FILM_FRAMES - parStart;
}

[[nodiscard]] bool rangesOverlap(FilmFrame parStart, FilmFrame parEnd,
                                 FilmFrame parOtherStart,
                                 FilmFrame parOtherEnd) {
  return parStart < parOtherEnd && parEnd > parOtherStart;
}

[[nodiscard]] bool canPlaceInstanceAt(const MovieTimeline& parTimeline,
                                      const TargetSequence& parSequence,
                                      FilmFrame parStartFrame) {
  const FilmFrame duration = parSequence.durationFrames();
  if (!fitsWithinFilm(parStartFrame, duration)) {
    return false;
  }
  if (parSequence.target.kind == TimelineTargetKind::Camera) {
    return true;
  }

  const FilmFrame end_frame = parStartFrame + duration;
  for (const SequenceInstance& instance : parTimeline.instances) {
    const TargetSequence* other = parTimeline.findSequence(instance.sequence_id);
    if (other == nullptr || other->target != parSequence.target) {
      continue;
    }
    const FilmFrame other_duration = other->durationFrames();
    if (other_duration > 0 &&
        rangesOverlap(parStartFrame, end_frame, instance.start_frame,
                      instance.start_frame + other_duration)) {
      return false;
    }
  }
  return true;
}

template <typename Range, typename Projection>
[[nodiscard]] std::uint64_t highestId(const Range& parItems,
                                      Projection parProjection) {
  std::uint64_t highest = 0;
  for (const auto& item : parItems) {
    highest = std::max(
        highest, static_cast<std::uint64_t>(std::invoke(parProjection, item)));
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
  return allocateId(parTimeline.next_sequence_id,
                    highestId(parTimeline.sequences, &TargetSequence::id));
}

[[nodiscard]] std::optional<SequenceInstanceId> allocateInstanceId(
    MovieTimeline& parTimeline) {
  return allocateId(parTimeline.next_instance_id,
                    highestId(parTimeline.instances, &SequenceInstance::id));
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
  return commit(timeline_, [=, name = std::move(parName),
                                   base = std::move(parCapturedBase)](
                                      MovieTimeline& timeline) mutable {
    TargetSequence sequence;
    const auto id = allocateSequenceId(timeline);
    if (!id.has_value()) {
      return fail<TargetSequenceId>("No sequence IDs remain");
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
  return commit(timeline_, [parSequenceId](MovieTimeline& timeline) {
    const TargetSequence* source = timeline.findSequence(parSequenceId);
    if (source == nullptr) {
      return fail<TargetSequenceId>("Sequence was not found");
    }
    // Snapshot first so ID allocation and insertion cannot affect the source
    // sequence.  Clips are value types, making this a deep copy of authored data.
    TargetSequence duplicate = *source;
    const auto sequence_id = allocateSequenceId(timeline);
    if (!sequence_id.has_value()) {
      return fail<TargetSequenceId>("No sequence IDs remain");
    }
    duplicate.id = *sequence_id;
    duplicate.name += " Copy";
    std::uint64_t highest_clip_id = highestClipId(timeline);
    for (SequenceClip& clip : duplicate.clips) {
      const auto clip_id = allocateId(timeline.next_clip_id, highest_clip_id);
      if (!clip_id.has_value()) {
        return fail<TargetSequenceId>("No clip IDs remain");
      }
      clip.id = *clip_id;
      highest_clip_id = *clip_id;
    }
    timeline.sequences.push_back(std::move(duplicate));
    return std::expected<TargetSequenceId, std::string>{timeline.sequences.back().id};
  });
}

std::expected<void, std::string> TimelineEditService::renameSequence(
    TargetSequenceId parSequenceId, std::string parName) {
  return commit(timeline_, [parSequenceId, name = std::move(parName)](
                                   MovieTimeline& timeline) mutable {
    if (name.empty()) {
      return fail("Sequence name cannot be empty");
    }
    TargetSequence* sequence = timeline.findSequence(parSequenceId);
    if (sequence == nullptr) {
      return fail("Sequence was not found");
    }
    sequence->name = std::move(name);
    return std::expected<void, std::string>{};
  });
}

std::expected<void, std::string> TimelineEditService::deleteSequence(
    TargetSequenceId parSequenceId) {
  return commit(timeline_, [parSequenceId](MovieTimeline& timeline) {
    const std::size_t previous_size = timeline.sequences.size();
    std::erase_if(timeline.sequences, [parSequenceId](const TargetSequence& item) {
      return item.id == parSequenceId;
    });
    if (timeline.sequences.size() == previous_size) {
      return fail("Sequence was not found");
    }
    std::erase_if(timeline.instances, [parSequenceId](const SequenceInstance& item) {
      return item.sequence_id == parSequenceId;
    });
    return std::expected<void, std::string>{};
  });
}

std::expected<void, std::string> TimelineEditService::setCameraGapMode(
    CameraGapMode parMode) {
  return commit(timeline_, [parMode](MovieTimeline& timeline) {
    if (parMode != CameraGapMode::HoldLastCameraState &&
        parMode != CameraGapMode::Black) {
      return fail("Camera gap mode is invalid");
    }
    timeline.camera_gap_mode = parMode;
    return std::expected<void, std::string>{};
  });
}

std::expected<SequenceClipId, std::string> TimelineEditService::appendClipToLane(
    TargetSequenceId parSequenceId, FilmFrame parDuration,
    SequenceClipPayload parPayload) {
  return commit(timeline_, [parSequenceId, parDuration,
                                 payload = std::move(parPayload)](
                                    MovieTimeline& timeline) mutable {
    if (parDuration <= 0) {
      return fail<SequenceClipId>("Clip duration must be positive");
    }
    TargetSequence* sequence = timeline.findSequence(parSequenceId);
    if (sequence == nullptr) {
      return fail<SequenceClipId>("Sequence was not found");
    }
    if (!isPayloadCompatibleWithTarget(sequence->target.kind, payload)) {
      return fail<SequenceClipId>("Clip is not authorable on this target type");
    }
    FilmFrame start = 0;
    const int lane = laneFor(payload);
    for (const SequenceClip& clip : sequence->clips) {
      if (laneFor(clip.payload) == lane) {
        start = std::max(start, clip.end_frame);
      }
    }
    if (!fitsWithinFilm(start, parDuration)) {
      return fail<SequenceClipId>("Clip exceeds the film frame limit");
    }
    const auto id = allocateClipId(timeline);
    if (!id.has_value()) {
      return fail<SequenceClipId>("No clip IDs remain");
    }
    sequence->clips.push_back({*id, start, start + parDuration, std::move(payload)});
    return std::expected<SequenceClipId, std::string>{sequence->clips.back().id};
  });
}

std::expected<SequenceClipId, std::string> TimelineEditService::duplicateClip(
    SequenceClipId parClipId) {
  return commit(timeline_, [parClipId](MovieTimeline& timeline) {
    const LocatedClip located = locateClip(timeline, parClipId);
    if (located.sequence == nullptr || located.clip == nullptr) {
      return fail<SequenceClipId>("Clip was not found");
    }

    const SequenceClip source_snapshot = *located.clip;
    const FilmFrame duration =
        source_snapshot.end_frame - source_snapshot.start_frame;
    if (duration <= 0 || source_snapshot.start_frame < 0 ||
        source_snapshot.end_frame > MAX_FILM_FRAMES) {
      return fail<SequenceClipId>("Source clip has an invalid frame range");
    }

    const int lane = laneFor(source_snapshot.payload);
    const FilmFrame last_start = MAX_FILM_FRAMES - duration;
    std::optional<FilmFrame> start_frame;
    for (FilmFrame candidate_start = source_snapshot.end_frame;
         candidate_start <= last_start; ++candidate_start) {
      const FilmFrame candidate_end = candidate_start + duration;
      const bool overlaps_existing = std::any_of(
          located.sequence->clips.begin(), located.sequence->clips.end(),
          [lane, candidate_start, candidate_end](const SequenceClip& candidate) {
            return laneFor(candidate.payload) == lane &&
                   rangesOverlap(candidate_start, candidate_end,
                                 candidate.start_frame, candidate.end_frame);
          });
      if (!overlaps_existing) {
        start_frame = candidate_start;
        break;
      }
    }
    if (!start_frame.has_value()) {
      return fail<SequenceClipId>(
          "No valid frame remains for clip duplicate");
    }

    const auto id = allocateClipId(timeline);
    if (!id.has_value()) {
      return fail<SequenceClipId>("No clip IDs remain");
    }
    located.sequence->clips.push_back(
        {*id, *start_frame, *start_frame + duration, source_snapshot.payload});
    return std::expected<SequenceClipId, std::string>{*id};
  });
}

std::expected<void, std::string> TimelineEditService::moveClip(
    SequenceClipId parClipId, FilmFrame parStartFrame, FilmFrame parEndFrame) {
  return commit(timeline_, [=](MovieTimeline& timeline) {
    const LocatedClip located = locateClip(timeline, parClipId);
    if (located.clip == nullptr) {
      return fail("Clip was not found");
    }
    located.clip->start_frame = parStartFrame;
    located.clip->end_frame = parEndFrame;
    return std::expected<void, std::string>{};
  });
}

std::expected<void, std::string> TimelineEditService::deleteClip(
    SequenceClipId parClipId) {
  return commit(timeline_, [parClipId](MovieTimeline& timeline) {
    const LocatedClip located = locateClip(timeline, parClipId);
    if (located.sequence == nullptr) {
      return fail("Clip was not found");
    }
    std::erase_if(located.sequence->clips, [parClipId](const SequenceClip& item) {
      return item.id == parClipId;
    });
    return std::expected<void, std::string>{};
  });
}

std::expected<void, std::string> TimelineEditService::setClipPayload(
    SequenceClipId parClipId, SequenceClipPayload parPayload) {
  return commit(timeline_, [parClipId, payload = std::move(parPayload)](
                                   MovieTimeline& timeline) mutable {
    const LocatedClip located = locateClip(timeline, parClipId);
    if (located.sequence == nullptr || located.clip == nullptr) {
      return fail("Clip was not found");
    }
    if (!isPayloadCompatibleWithTarget(located.sequence->target.kind, payload)) {
      return fail("Clip is not authorable on this target type");
    }
    located.clip->payload = std::move(payload);
    return std::expected<void, std::string>{};
  });
}

std::expected<void, std::string> TimelineEditService::setMovementStartMode(
    SequenceClipId parClipId, MovementStartMode parMode,
    std::optional<math::Transform> parExplicitStart) {
  return commit(timeline_, [=](MovieTimeline& timeline) {
    const LocatedClip located = locateClip(timeline, parClipId);
    MovementClip* movement =
        located.clip == nullptr ? nullptr
                                : std::get_if<MovementClip>(&located.clip->payload);
    if (movement == nullptr) {
      return fail("Movement clip was not found");
    }
    if (parMode == MovementStartMode::ExplicitPosition && !parExplicitStart.has_value()) {
      return fail("Explicit movement starts require a transform");
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
  return commit(timeline_, [parClipId, transition = std::move(parTransition)](
                                   MovieTimeline& timeline) mutable {
    const LocatedClip located = locateClip(timeline, parClipId);
    MovementClip* movement =
        located.clip == nullptr ? nullptr
                                : std::get_if<MovementClip>(&located.clip->payload);
    if (movement == nullptr) {
      return fail("Movement clip was not found");
    }
    movement->transition_before = std::move(transition);
    return std::expected<void, std::string>{};
  });
}

std::expected<SequenceInstanceId, std::string> TimelineEditService::placeSequence(
    TargetSequenceId parSequenceId, FilmFrame parStartFrame) {
  return commit(timeline_, [=](MovieTimeline& timeline) {
    const TargetSequence* sequence = timeline.findSequence(parSequenceId);
    if (sequence == nullptr || sequence->durationFrames() <= 0) {
      return fail<SequenceInstanceId>(
          "Only non-empty sequences can be placed");
    }
    if (!fitsWithinFilm(parStartFrame, sequence->durationFrames())) {
      return fail<SequenceInstanceId>("Instance exceeds the film frame limit");
    }
    const auto id = allocateInstanceId(timeline);
    if (!id.has_value()) {
      return fail<SequenceInstanceId>("No instance IDs remain");
    }
    timeline.instances.push_back({*id, parSequenceId, parStartFrame});
    return std::expected<SequenceInstanceId, std::string>{*id};
  });
}

std::expected<SequenceInstanceId, std::string> TimelineEditService::duplicateInstance(
    SequenceInstanceId parInstanceId) {
  return commit(timeline_, [=](MovieTimeline& timeline) {
    const SequenceInstance* source = timeline.findInstance(parInstanceId);
    if (source == nullptr) {
      return fail<SequenceInstanceId>("Instance was not found");
    }
    // An instance duplicate deliberately reuses the same target sequence; only
    // the placement ID and start frame are new.
    const TargetSequenceId source_sequence_id = source->sequence_id;
    const TargetSequence* sequence = timeline.findSequence(source_sequence_id);
    if (sequence == nullptr || sequence->durationFrames() <= 0) {
      return fail<SequenceInstanceId>(
          "Only non-empty sequences can be duplicated");
    }
    const FilmFrame duration = sequence->durationFrames();
    if (!fitsWithinFilm(source->start_frame, duration)) {
      return fail<SequenceInstanceId>(
          "Source instance has an invalid frame range");
    }
    const FilmFrame first_frame = source->start_frame + duration;
    const FilmFrame last_frame = MAX_FILM_FRAMES - duration;
    std::optional<FilmFrame> start_frame;
    for (FilmFrame candidate = first_frame; candidate <= last_frame; ++candidate) {
      if (canPlaceInstanceAt(timeline, *sequence, candidate)) {
        start_frame = candidate;
        break;
      }
    }
    if (!start_frame.has_value()) {
      return fail<SequenceInstanceId>(
          "No valid frame remains for instance duplicate");
    }
    const auto id = allocateInstanceId(timeline);
    if (!id.has_value()) {
      return fail<SequenceInstanceId>("No instance IDs remain");
    }
    timeline.instances.push_back({*id, source_sequence_id, *start_frame});
    return std::expected<SequenceInstanceId, std::string>{*id};
  });
}

std::expected<void, std::string> TimelineEditService::moveInstance(
    SequenceInstanceId parInstanceId, FilmFrame parStartFrame) {
  return commit(timeline_, [=](MovieTimeline& timeline) {
    SequenceInstance* instance = timeline.findInstance(parInstanceId);
    if (instance == nullptr) {
      return fail("Instance was not found");
    }
    const TargetSequence* sequence = timeline.findSequence(instance->sequence_id);
    if (sequence == nullptr ||
        !fitsWithinFilm(parStartFrame, sequence->durationFrames())) {
      return fail("Instance exceeds the film frame limit");
    }
    instance->start_frame = parStartFrame;
    return std::expected<void, std::string>{};
  });
}

std::expected<void, std::string> TimelineEditService::deleteInstance(
    SequenceInstanceId parInstanceId) {
  return commit(timeline_, [=](MovieTimeline& timeline) {
    const std::size_t previous_size = timeline.instances.size();
    std::erase_if(timeline.instances, [=](const SequenceInstance& item) {
      return item.id == parInstanceId;
    });
    if (timeline.instances.size() == previous_size) {
      return fail("Instance was not found");
    }
    return std::expected<void, std::string>{};
  });
}

}  // namespace kage::film
