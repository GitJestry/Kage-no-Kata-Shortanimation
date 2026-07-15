#include "film/movie_timeline.hpp"

#include "math/cubic_bezier.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace {

using namespace kage;
using namespace kage::film;
using kage::math::cubicBezier;

[[nodiscard]] float clipT(FilmFrame parFrame, FilmFrame parStart,
                          FilmFrame parEnd) {
  if (parEnd <= parStart) {
    return 1.0f;
  }
  return std::clamp(static_cast<float>(parFrame - parStart) /
                        static_cast<float>(parEnd - parStart),
                    0.0f, 1.0f);
}

[[nodiscard]] math::Transform sampleMovementCurve(
    const math::Transform& parStart, const math::Transform& parEnd,
    const MovementCurve& parCurve, float parT) {
  const float linear_t = std::clamp(parT, 0.0f, 1.0f);
  const float timing_1 = std::clamp(parCurve.timing_control_1, 0.0f, 1.0f);
  const float timing_2 =
      std::clamp(parCurve.timing_control_2, timing_1, 1.0f);
  const float t = cubicBezier(0.0f, timing_1, timing_2, 1.0f, linear_t);
  const glm::vec3 delta = parEnd.translation - parStart.translation;
  const glm::vec3 control_1 = parCurve.automatic_position_controls
                                  ? parStart.translation + delta / 3.0f
                                  : parCurve.position_control_1;
  const glm::vec3 control_2 = parCurve.automatic_position_controls
                                  ? parEnd.translation - delta / 3.0f
                                  : parCurve.position_control_2;
  math::Transform result;
  result.translation = cubicBezier(parStart.translation, control_1, control_2,
                                   parEnd.translation, t);
  glm::quat end_rotation = parEnd.rotation;
  if (glm::dot(parStart.rotation, end_rotation) < 0.0f) {
    end_rotation = -end_rotation;
  }
  result.rotation = glm::normalize(glm::slerp(parStart.rotation, end_rotation, t));
  result.scale = glm::mix(parStart.scale, parEnd.scale, t);
  return result;
}

[[nodiscard]] bool overlaps(FilmFrame parStart, FilmFrame parEnd,
                            FilmFrame parOtherStart, FilmFrame parOtherEnd) {
  return parStart < parOtherEnd && parEnd > parOtherStart;
}

[[nodiscard]] bool sameTarget(const TimelineTarget& parLeft,
                              const TimelineTarget& parRight) {
  return parLeft == parRight;
}

void setTransform(FilmFrameState& parState, scene::EntityId parEntity,
                  const math::Transform& parTransform) {
  auto found = std::find_if(parState.transforms.begin(), parState.transforms.end(),
                            [parEntity](const TransformOverride& item) {
                              return item.entity == parEntity;
                            });
  if (found == parState.transforms.end()) {
    parState.transforms.push_back({parEntity, parTransform});
  } else {
    found->transform = parTransform;
  }
}

EvaluatedPointLightState& setPointLight(
    FilmFrameState& parState, const EvaluatedPointLightState& parLight) {
  auto found = std::find_if(
      parState.point_lights.begin(), parState.point_lights.end(),
      [entity = parLight.source_entity](const EvaluatedPointLightState& item) {
        return item.source_entity == entity;
      });
  if (found == parState.point_lights.end()) {
    parState.point_lights.push_back(parLight);
    return parState.point_lights.back();
  }
  *found = parLight;
  return *found;
}

[[nodiscard]] RigAnimationPlayback asPlaybackAnimation(
    const RigAnimationClip& parClip) {
  return {parClip.clip_id, parClip.source_in, parClip.source_out, parClip.speed,
          parClip.looping};
}

[[nodiscard]] FilmFrame saturatedFrameEnd(FilmFrame parStart,
                                          FilmFrame parDuration) {
  const std::int64_t end = static_cast<std::int64_t>(parStart) +
                           static_cast<std::int64_t>(parDuration);
  return static_cast<FilmFrame>(std::clamp(
      end, static_cast<std::int64_t>(std::numeric_limits<FilmFrame>::min()),
      static_cast<std::int64_t>(std::numeric_limits<FilmFrame>::max())));
}

void addDiagnostic(TimelineValidation& parValidation,
                   TimelineDiagnostic::Severity parSeverity,
                   std::string parMessage) {
  parValidation.diagnostics.push_back({parSeverity, std::move(parMessage)});
}

[[nodiscard]] bool validCurve(const MovementCurve& parCurve) {
  return std::isfinite(parCurve.timing_control_1) &&
         std::isfinite(parCurve.timing_control_2) &&
         parCurve.timing_control_1 >= 0.0f &&
         parCurve.timing_control_2 >= parCurve.timing_control_1 &&
         parCurve.timing_control_2 <= 1.0f;
}

}  // namespace

namespace kage::film {

int laneFor(const SequenceClipPayload& parPayload) {
  if (std::holds_alternative<MovementClip>(parPayload)) {
    return 0;
  }
  if (std::holds_alternative<RigAnimationClip>(parPayload)) {
    return 1;
  }
  return 2 + static_cast<int>(std::get<PropertyClip>(parPayload).kind);
}

namespace {

[[nodiscard]] MovementPathSegment resolveMovementSegment(
    const glm::vec3& parStart, const glm::vec3& parEnd,
    const MovementCurve& parCurve) {
  const glm::vec3 delta = parEnd - parStart;
  return {
      parStart,
      parCurve.automatic_position_controls
          ? parStart + delta / 3.0f
          : parCurve.position_control_1,
      parCurve.automatic_position_controls
          ? parEnd - delta / 3.0f
          : parCurve.position_control_2,
      parEnd,
  };
}

[[nodiscard]] std::vector<const SequenceClip*> orderedMovementClips(
    const TargetSequence& parSequence) {
  std::vector<const SequenceClip*> clips;
  clips.reserve(parSequence.clips.size());
  for (const SequenceClip& clip : parSequence.clips) {
    if (std::holds_alternative<MovementClip>(clip.payload)) {
      clips.push_back(&clip);
    }
  }
  std::sort(clips.begin(), clips.end(),
            [](const SequenceClip* parLeft, const SequenceClip* parRight) {
              if (parLeft->start_frame != parRight->start_frame) {
                return parLeft->start_frame < parRight->start_frame;
              }
              return parLeft->id < parRight->id;
            });
  return clips;
}

}  // namespace

FilmFrame TargetSequence::durationFrames() const {
  FilmFrame duration = 0;
  for (const SequenceClip& clip : clips) {
    duration = std::max(duration, clip.end_frame);
  }
  return duration;
}

FilmFrame MovieTimeline::durationFrames() const {
  FilmFrame duration = 0;
  for (const SequenceInstance& instance : instances) {
    const TargetSequence* sequence = findSequence(instance.sequence_id);
    if (sequence != nullptr) {
      duration = std::max(
          duration,
          saturatedFrameEnd(instance.start_frame, sequence->durationFrames()));
    }
  }
  return duration;
}

std::optional<ResolvedMovementPath> resolveMovementPath(
    const TargetSequence& parSequence, SequenceClipId parClipId) {
  const auto* captured =
      std::get_if<CapturedEntityBaseState>(&parSequence.captured_base);
  if (captured == nullptr || parClipId == 0) {
    return std::nullopt;
  }

  const std::vector<const SequenceClip*> movements =
      orderedMovementClips(parSequence);

  math::Transform current = captured->transform;
  const SequenceClip* previous = nullptr;
  for (const SequenceClip* clip : movements) {
    const MovementClip& movement = std::get<MovementClip>(clip->payload);
    const math::Transform start =
        movement.start_mode == MovementStartMode::ExplicitPosition &&
                movement.explicit_start.has_value()
            ? *movement.explicit_start
            : current;
    if (clip->id == parClipId) {
      ResolvedMovementPath result;
      result.movement = resolveMovementSegment(
          start.translation, movement.end.translation, movement.curve);
      if (previous != nullptr &&
          previous->end_frame < clip->start_frame &&
          movement.start_mode == MovementStartMode::ExplicitPosition &&
          movement.transition_before.enabled) {
        result.transition_before = resolveMovementSegment(
            current.translation, start.translation,
            movement.transition_before.curve);
      }
      return result;
    }
    current = movement.end;
    previous = clip;
  }
  return std::nullopt;
}

TargetSequence* MovieTimeline::findSequence(TargetSequenceId parId) {
  const auto found = std::find_if(sequences.begin(), sequences.end(),
                                  [parId](const TargetSequence& item) {
                                    return item.id == parId;
                                  });
  return found == sequences.end() ? nullptr : &*found;
}

const TargetSequence* MovieTimeline::findSequence(TargetSequenceId parId) const {
  const auto found = std::find_if(sequences.begin(), sequences.end(),
                                  [parId](const TargetSequence& item) {
                                    return item.id == parId;
                                  });
  return found == sequences.end() ? nullptr : &*found;
}

SequenceClip* MovieTimeline::findClip(SequenceClipId parId) {
  for (TargetSequence& sequence : sequences) {
    const auto found = std::find_if(sequence.clips.begin(), sequence.clips.end(),
                                    [parId](const SequenceClip& item) {
                                      return item.id == parId;
                                    });
    if (found != sequence.clips.end()) {
      return &*found;
    }
  }
  return nullptr;
}

SequenceInstance* MovieTimeline::findInstance(SequenceInstanceId parId) {
  const auto found = std::find_if(instances.begin(), instances.end(),
                                  [parId](const SequenceInstance& item) {
                                    return item.id == parId;
                                  });
  return found == instances.end() ? nullptr : &*found;
}

bool TimelineValidation::hasErrors() const {
  return std::any_of(diagnostics.begin(), diagnostics.end(),
                     [](const TimelineDiagnostic& item) {
                       return item.severity == TimelineDiagnostic::Severity::Error;
                     });
}

bool isCameraSequence(const TargetSequence& parSequence) {
  return parSequence.target.kind == TimelineTargetKind::Camera;
}

namespace {

[[nodiscard]] bool isValidTimelineTarget(const TimelineTarget& parTarget) {
  return parTarget.kind == TimelineTargetKind::Sun ? !parTarget.entity.isValid()
                                                    : parTarget.entity.isValid();
}

}  // namespace

bool isPayloadCompatibleWithTarget(TimelineTargetKind parTargetKind,
                                   const SequenceClipPayload& parPayload) {
  if (std::holds_alternative<MovementClip>(parPayload)) {
    return parTargetKind == TimelineTargetKind::RiggedEntity ||
           parTargetKind == TimelineTargetKind::Camera ||
           parTargetKind == TimelineTargetKind::PointLight;
  }
  if (std::holds_alternative<RigAnimationClip>(parPayload)) {
    return parTargetKind == TimelineTargetKind::RiggedEntity;
  }
  const PropertyKind kind = std::get<PropertyClip>(parPayload).kind;
  switch (parTargetKind) {
    case TimelineTargetKind::RiggedEntity:
      return false;
    case TimelineTargetKind::Camera:
      return kind == PropertyKind::CameraFov;
    case TimelineTargetKind::PointLight:
      return kind == PropertyKind::PointLightIntensity ||
             kind == PropertyKind::PointLightColor;
    case TimelineTargetKind::Sun:
      return kind == PropertyKind::SunDirection ||
             kind == PropertyKind::SunIntensity || kind == PropertyKind::SunColor;
  }
  return false;
}

namespace {

void evaluateTargetSequence(const TargetSequence& parSequence,
                            FilmFrame parLocalFrame, FilmFrameState& parState,
                            bool parEmitCamera) {
  EvaluatedPointLightState* evaluated_light = nullptr;
  if (const auto* entity = std::get_if<CapturedEntityBaseState>(&parSequence.captured_base)) {
    setTransform(parState, parSequence.target.entity, entity->transform);
    if (parEmitCamera && entity->camera.has_value()) {
      parState.camera =
          EvaluatedCameraState{parSequence.target.entity, entity->transform,
                               entity->camera->vertical_fov_degrees,
                               entity->camera->near_plane,
                               entity->camera->far_plane};
    }
    if (entity->point_light.has_value()) {
      evaluated_light = &setPointLight(
          parState, {parSequence.target.entity, entity->point_light->enabled,
                     entity->point_light->color, entity->point_light->intensity,
                     entity->point_light->range,
                     entity->point_light->casts_shadows});
    }
  } else if (const auto* sun = std::get_if<CapturedSunBaseState>(&parSequence.captured_base)) {
    parState.sun = {{sun->direction_to_sun, sun->color, sun->intensity}};
  }

  const std::vector<const SequenceClip*> movements =
      orderedMovementClips(parSequence);
  if (!movements.empty() &&
      std::holds_alternative<CapturedEntityBaseState>(parSequence.captured_base)) {
    math::Transform current =
        std::get<CapturedEntityBaseState>(parSequence.captured_base).transform;
    FilmFrame previous_end = 0;
    for (const SequenceClip* clip : movements) {
      const MovementClip& movement = std::get<MovementClip>(clip->payload);
      const math::Transform start =
          movement.start_mode == MovementStartMode::ExplicitPosition &&
                  movement.explicit_start.has_value()
              ? *movement.explicit_start
              : current;
      if (parLocalFrame < clip->start_frame) {
        if (movement.transition_before.enabled && parLocalFrame >= previous_end &&
            clip->start_frame > previous_end) {
          current = sampleMovementCurve(
              current, start, movement.transition_before.curve,
              clipT(parLocalFrame, previous_end, clip->start_frame));
        }
        break;
      }
      if (parLocalFrame < clip->end_frame) {
        current = sampleMovementCurve(start, movement.end, movement.curve,
                                      clipT(parLocalFrame, clip->start_frame,
                                            clip->end_frame));
        break;
      }
      current = movement.end;
      previous_end = clip->end_frame;
    }
    setTransform(parState, parSequence.target.entity, current);
    if (parEmitCamera && parState.camera.has_value() &&
        parState.camera->source_entity == parSequence.target.entity) {
      parState.camera->transform = current;
    }
  }

  for (int lane = 2; lane < 2 + static_cast<int>(PropertyKind::SunColor) + 1;
       ++lane) {
    const SequenceClip* selected = nullptr;
    for (const SequenceClip& clip : parSequence.clips) {
      if (laneFor(clip.payload) != lane || parLocalFrame < clip.start_frame) {
        continue;
      }
      if (selected == nullptr || clip.start_frame > selected->start_frame) {
        selected = &clip;
      }
    }
    if (selected == nullptr) {
      continue;
    }
    const PropertyClip& property = std::get<PropertyClip>(selected->payload);
    const glm::vec4 value = parLocalFrame < selected->end_frame
                                ? cubicBezier(property.start_value, property.control_1,
                                              property.control_2, property.end_value,
                                              clipT(parLocalFrame, selected->start_frame,
                                                    selected->end_frame))
                                : property.end_value;
    switch (property.kind) {
      case PropertyKind::CameraFov:
        if (parEmitCamera && parState.camera.has_value() &&
            parState.camera->source_entity == parSequence.target.entity) {
          parState.camera->vertical_fov_degrees = value.x;
        }
        break;
      case PropertyKind::PointLightIntensity:
        if (evaluated_light != nullptr) {
          evaluated_light->intensity = value.x;
        }
        break;
      case PropertyKind::PointLightColor:
        if (evaluated_light != nullptr) {
          evaluated_light->color = glm::vec3(value);
        }
        break;
      case PropertyKind::SunDirection:
      case PropertyKind::SunIntensity:
      case PropertyKind::SunColor:
        if (!parState.sun.has_value()) {
          parState.sun = EvaluatedSunState{};
        }
        if (property.kind == PropertyKind::SunDirection) {
          parState.sun->direction_to_sun = glm::vec3(value);
        } else if (property.kind == PropertyKind::SunColor) {
          parState.sun->color = glm::vec3(value);
        } else {
          parState.sun->intensity = value.x;
        }
        break;
    }
  }

  const SequenceClip* animation_clip = nullptr;
  for (const SequenceClip& clip : parSequence.clips) {
    if (!std::holds_alternative<RigAnimationClip>(clip.payload) ||
        parLocalFrame < clip.start_frame) {
      continue;
    }
    if (animation_clip == nullptr || clip.start_frame > animation_clip->start_frame) {
      animation_clip = &clip;
    }
  }
  if (animation_clip != nullptr &&
      std::holds_alternative<CapturedEntityBaseState>(parSequence.captured_base)) {
    const RigAnimationClip& animation =
        std::get<RigAnimationClip>(animation_clip->payload);
    const FilmFrame sample_frame =
        std::min(parLocalFrame, animation_clip->end_frame);
    const FilmFrame elapsed_frames =
        std::max(sample_frame - animation_clip->start_frame, FilmFrame{0});
    const bool final_pose = parLocalFrame >= animation_clip->end_frame;
    RigAnimationOverride override{parSequence.target.entity,
                                  asPlaybackAnimation(animation),
                                  static_cast<float>(elapsed_frames) /
                                      static_cast<float>(FILM_FPS) * animation.speed,
                                  1.0f, final_pose};
    auto found = std::find_if(parState.rig_animations.begin(),
                              parState.rig_animations.end(),
                              [&override](const RigAnimationOverride& item) {
                                return item.entity == override.entity;
                              });
    if (found == parState.rig_animations.end()) {
      parState.rig_animations.push_back(override);
    } else {
      *found = override;
    }
  }
}

}  // namespace

std::optional<FilmFrameState> evaluateTargetSequencePreview(
    const MovieTimeline& parTimeline, TargetSequenceId parSequenceId,
    FilmFrame parFrame) {
  const TargetSequence* sequence = parTimeline.findSequence(parSequenceId);
  if (sequence == nullptr) {
    return std::nullopt;
  }
  FilmFrameState state;
  evaluateTargetSequence(
      *sequence,
      std::clamp(parFrame, FilmFrame{0}, sequence->durationFrames()), state,
      isCameraSequence(*sequence));
  return state;
}

FilmFrameState evaluateMovieTimeline(const MovieTimeline& parTimeline,
                                     FilmFrame parFrame) {
  FilmFrameState state;
  struct Candidate final {
    const SequenceInstance* instance;
    const TargetSequence* sequence;
  };

  std::optional<Candidate> camera;
  for (const SequenceInstance& instance : parTimeline.instances) {
    const TargetSequence* sequence = parTimeline.findSequence(instance.sequence_id);
    if (sequence == nullptr || !isCameraSequence(*sequence) ||
        instance.start_frame > parFrame) {
      continue;
    }
    const bool active =
        parFrame < saturatedFrameEnd(instance.start_frame, sequence->durationFrames());
    if (!active && parTimeline.camera_gap_mode == CameraGapMode::Black) {
      continue;
    }
    if (!camera.has_value() || instance.start_frame > camera->instance->start_frame ||
        (instance.start_frame == camera->instance->start_frame &&
         instance.id > camera->instance->id)) {
      camera = {&instance, sequence};
    }
  }

  std::vector<TimelineTarget> targets;
  for (const SequenceInstance& instance : parTimeline.instances) {
    const TargetSequence* sequence = parTimeline.findSequence(instance.sequence_id);
    if (sequence != nullptr &&
        std::find(targets.begin(), targets.end(), sequence->target) == targets.end()) {
      targets.push_back(sequence->target);
    }
  }

  for (const TimelineTarget& target : targets) {
    std::vector<Candidate> candidates;
    for (const SequenceInstance& instance : parTimeline.instances) {
      const TargetSequence* sequence = parTimeline.findSequence(instance.sequence_id);
      if (sequence != nullptr && sameTarget(sequence->target, target)) {
        candidates.push_back({&instance, sequence});
      }
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& left,
                                                        const Candidate& right) {
      if (left.instance->start_frame != right.instance->start_frame) {
        return left.instance->start_frame < right.instance->start_frame;
      }
      return left.instance->id < right.instance->id;
    });
    Candidate selected = candidates.front();
    FilmFrame local_frame = 0;
    bool found_started = false;
    for (const Candidate& candidate : candidates) {
      if (candidate.instance->start_frame > parFrame) {
        break;
      }
      const FilmFrame end = saturatedFrameEnd(
          candidate.instance->start_frame, candidate.sequence->durationFrames());
      if (!found_started || parFrame < end ||
          candidate.instance->start_frame > selected.instance->start_frame ||
          (candidate.instance->start_frame == selected.instance->start_frame &&
           candidate.instance->id > selected.instance->id)) {
        selected = candidate;
        found_started = true;
      }
    }
    if (found_started) {
      local_frame = std::min(parFrame - selected.instance->start_frame,
                             selected.sequence->durationFrames());
    }
    if (camera.has_value() && camera->sequence->target == target) {
      selected = *camera;
      local_frame = std::min(parFrame - selected.instance->start_frame,
                             selected.sequence->durationFrames());
    }
    const bool emit_camera =
        camera.has_value() && selected.instance == camera->instance;
    evaluateTargetSequence(*selected.sequence, local_frame, state, emit_camera);
  }
  return state;
}

TimelineValidation validateMovieTimeline(const MovieTimeline& parTimeline,
                                         bool parForBake) {
  TimelineValidation validation;
  std::vector<SequenceClipId> clip_ids;
  for (const TargetSequence& sequence : parTimeline.sequences) {
    if (sequence.id == 0 || !isValidTimelineTarget(sequence.target)) {
      addDiagnostic(validation, TimelineDiagnostic::Severity::Error,
                    "A sequence has an invalid target or ID");
    }
    const auto* entity_base =
        std::get_if<CapturedEntityBaseState>(&sequence.captured_base);
    const bool captured_base_matches_target =
        (sequence.target.kind == TimelineTargetKind::Sun &&
         std::holds_alternative<CapturedSunBaseState>(sequence.captured_base)) ||
        (sequence.target.kind == TimelineTargetKind::RiggedEntity &&
         entity_base != nullptr && !entity_base->camera.has_value() &&
         !entity_base->point_light.has_value()) ||
        (sequence.target.kind == TimelineTargetKind::Camera && entity_base != nullptr &&
         entity_base->camera.has_value() && !entity_base->point_light.has_value()) ||
        (sequence.target.kind == TimelineTargetKind::PointLight && entity_base != nullptr &&
         entity_base->point_light.has_value() && !entity_base->camera.has_value());
    if (!captured_base_matches_target) {
      addDiagnostic(validation, TimelineDiagnostic::Severity::Error,
                    "A sequence target does not match its captured base state");
    }
    for (const SequenceClip& clip : sequence.clips) {
      if (clip.id == 0 || clip.start_frame < 0 || clip.end_frame <= clip.start_frame ||
          clip.end_frame > MAX_FILM_FRAMES ||
          std::find(clip_ids.begin(), clip_ids.end(), clip.id) != clip_ids.end()) {
        addDiagnostic(validation, TimelineDiagnostic::Severity::Error,
                      "A sequence clip has an invalid ID or frame range");
      }
      clip_ids.push_back(clip.id);
      if (const auto* movement = std::get_if<MovementClip>(&clip.payload)) {
        if (!isPayloadCompatibleWithTarget(sequence.target.kind, clip.payload) ||
            !validCurve(movement->curve) ||
            !validCurve(movement->transition_before.curve) ||
            (movement->start_mode == MovementStartMode::ExplicitPosition &&
             !movement->explicit_start.has_value())) {
          addDiagnostic(validation, TimelineDiagnostic::Severity::Error,
                        "A movement clip is incompatible or has an invalid curve");
        }
      } else if (const auto* animation = std::get_if<RigAnimationClip>(&clip.payload)) {
        if (!isPayloadCompatibleWithTarget(sequence.target.kind, clip.payload) ||
            !std::isfinite(animation->source_in) ||
            !std::isfinite(animation->source_out) || animation->source_in < 0.0f ||
            animation->source_out <= animation->source_in || animation->source_out > 1.0f ||
            !std::isfinite(animation->speed) || animation->speed < 0.0f) {
          addDiagnostic(validation, TimelineDiagnostic::Severity::Error,
                        "A rig animation clip has an invalid source range or speed");
        }
      } else {
        if (!isPayloadCompatibleWithTarget(sequence.target.kind, clip.payload)) {
          addDiagnostic(validation, TimelineDiagnostic::Severity::Error,
                        "A property clip is incompatible with its target");
        }
      }
    }
    for (std::size_t left = 0; left < sequence.clips.size(); ++left) {
      for (std::size_t right = left + 1; right < sequence.clips.size(); ++right) {
        if (laneFor(sequence.clips[left].payload) == laneFor(sequence.clips[right].payload) &&
            overlaps(sequence.clips[left].start_frame, sequence.clips[left].end_frame,
                     sequence.clips[right].start_frame, sequence.clips[right].end_frame)) {
          addDiagnostic(validation, TimelineDiagnostic::Severity::Error,
                        "Clips overlap on the same lane");
        }
      }
    }
  }

  for (const SequenceInstance& instance : parTimeline.instances) {
    const TargetSequence* sequence = parTimeline.findSequence(instance.sequence_id);
    const std::int64_t instance_end =
        sequence == nullptr
            ? 0
            : static_cast<std::int64_t>(instance.start_frame) +
                  static_cast<std::int64_t>(sequence->durationFrames());
    if (instance.id == 0 || instance.start_frame < 0 || sequence == nullptr ||
        sequence->durationFrames() <= 0 || instance_end > MAX_FILM_FRAMES) {
      addDiagnostic(validation, TimelineDiagnostic::Severity::Error,
                    "An instance has an invalid target sequence or frame range");
    }
  }
  for (std::size_t left = 0; left < parTimeline.instances.size(); ++left) {
    const SequenceInstance& first = parTimeline.instances[left];
    const TargetSequence* first_sequence = parTimeline.findSequence(first.sequence_id);
    if (first_sequence == nullptr) {
      continue;
    }
    for (std::size_t right = left + 1; right < parTimeline.instances.size(); ++right) {
      const SequenceInstance& second = parTimeline.instances[right];
      const TargetSequence* second_sequence = parTimeline.findSequence(second.sequence_id);
      if (second_sequence == nullptr ||
          !overlaps(first.start_frame,
                    saturatedFrameEnd(first.start_frame,
                                      first_sequence->durationFrames()),
                    second.start_frame,
                    saturatedFrameEnd(second.start_frame,
                                      second_sequence->durationFrames()))) {
        continue;
      }
      const bool camera_overlap = isCameraSequence(*first_sequence) &&
                                  isCameraSequence(*second_sequence);
      if (!camera_overlap &&
          !sameTarget(first_sequence->target, second_sequence->target)) {
        continue;
      }
      addDiagnostic(validation,
                    camera_overlap && !parForBake ? TimelineDiagnostic::Severity::Warning
                                                   : TimelineDiagnostic::Severity::Error,
                    camera_overlap ? "Camera instances overlap"
                                   : "Non-camera instances overlap");
    }
  }
  if (parTimeline.durationFrames() > MAX_FILM_FRAMES) {
    addDiagnostic(validation, TimelineDiagnostic::Severity::Error,
                  "Movie duration exceeds the frame limit");
  }
  if (parForBake) {
    if (parTimeline.durationFrames() <= 0) {
      addDiagnostic(validation, TimelineDiagnostic::Severity::Error,
                    "Movie has no playable frames");
    }
    const bool has_camera = std::any_of(
        parTimeline.instances.begin(), parTimeline.instances.end(),
        [&parTimeline](const SequenceInstance& instance) {
          const TargetSequence* sequence = parTimeline.findSequence(instance.sequence_id);
          return sequence != nullptr && isCameraSequence(*sequence) &&
                 sequence->durationFrames() > 0;
        });
    if (!has_camera) {
      addDiagnostic(validation, TimelineDiagnostic::Severity::Error,
                    "Movie has no valid camera output");
    }
  }
  return validation;
}

void FilmPlayback::stop() {
  playing = false;
  previewing = false;
}

void FilmPlayback::update(float parDeltaSeconds, FilmFrame parDuration) {
  if (parDuration <= 0) {
    playhead_frame = 0.0;
    stop();
    return;
  }
  if (!playing) {
    return;
  }
  playhead_frame += static_cast<double>(std::max(parDeltaSeconds, 0.0f)) *
                    static_cast<double>(FILM_FPS);
  if (playhead_frame < static_cast<double>(parDuration)) {
    return;
  }
  if (looping) {
    playhead_frame = std::fmod(playhead_frame, static_cast<double>(parDuration));
  } else {
    playhead_frame = static_cast<double>(parDuration - 1);
    playing = false;
  }
}

}  // namespace kage::film
