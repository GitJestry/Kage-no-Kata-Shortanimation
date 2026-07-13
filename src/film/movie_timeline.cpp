#include "film/movie_timeline.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace {

using namespace kage;
using namespace kage::film;

template <typename Value>
[[nodiscard]] Value cubicBezier(const Value& parStart, const Value& parControl1,
                                const Value& parControl2, const Value& parEnd,
                                float parT) {
  const float inverse = 1.0f - parT;
  return inverse * inverse * inverse * parStart +
         3.0f * inverse * inverse * parT * parControl1 +
         3.0f * inverse * parT * parT * parControl2 +
         parT * parT * parT * parEnd;
}

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

[[nodiscard]] int laneFor(const SequenceClipPayload& parPayload) {
  if (std::holds_alternative<MovementClip>(parPayload)) {
    return 0;
  }
  if (std::holds_alternative<RigAnimationClip>(parPayload)) {
    return 1;
  }
  return 2 + static_cast<int>(std::get<PropertyClip>(parPayload).kind);
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

[[nodiscard]] std::optional<FilmPropertyKind> legacyPropertyKind(
    PropertyKind parKind) {
  switch (parKind) {
    case PropertyKind::CameraFov:
      return FilmPropertyKind::CameraFov;
    case PropertyKind::PointLightIntensity:
      return FilmPropertyKind::LightIntensity;
    case PropertyKind::PointLightColor:
      return FilmPropertyKind::LightColor;
    case PropertyKind::LegacyPointLightEnabled:
      return FilmPropertyKind::LightEnabled;
    case PropertyKind::LegacyPointLightRange:
      return FilmPropertyKind::LightRange;
    case PropertyKind::SunDirection:
    case PropertyKind::SunIntensity:
    case PropertyKind::SunColor:
      return std::nullopt;
  }
  return std::nullopt;
}

void setProperty(FilmFrameState& parState, scene::EntityId parEntity,
                 FilmPropertyKind parKind, const glm::vec4& parValue) {
  auto found = std::find_if(parState.properties.begin(), parState.properties.end(),
                            [parEntity, parKind](const PropertyOverride& item) {
                              return item.entity == parEntity && item.kind == parKind;
                            });
  if (found == parState.properties.end()) {
    parState.properties.push_back({parEntity, parKind, parValue});
  } else {
    found->value = parValue;
  }
}

[[nodiscard]] RigAnimation asLegacyAnimation(const RigAnimationClip& parClip) {
  return {parClip.clip_id, parClip.legacy_clip_index, parClip.source_in,
          parClip.source_out, parClip.speed, parClip.blend_in_seconds,
          parClip.blend_out_seconds, parClip.looping};
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

const SequenceClip* MovieTimeline::findClip(SequenceClipId parId) const {
  for (const TargetSequence& sequence : sequences) {
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

bool TimelineValidation::hasErrors() const {
  return std::any_of(diagnostics.begin(), diagnostics.end(),
                     [](const TimelineDiagnostic& item) {
                       return item.severity == TimelineDiagnostic::Severity::Error;
                     });
}

bool TimelineValidation::hasWarnings() const {
  return std::any_of(diagnostics.begin(), diagnostics.end(),
                     [](const TimelineDiagnostic& item) {
                       return item.severity == TimelineDiagnostic::Severity::Warning;
                     });
}

bool isCameraSequence(const TargetSequence& parSequence) {
  return parSequence.target.kind == TimelineTargetKind::Camera;
}

bool isValidTimelineTarget(const TimelineTarget& parTarget) {
  return parTarget.kind == TimelineTargetKind::Sun ? !parTarget.entity.isValid()
                                                    : parTarget.entity.isValid();
}

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
             kind == PropertyKind::PointLightColor ||
             kind == PropertyKind::LegacyPointLightEnabled ||
             kind == PropertyKind::LegacyPointLightRange;
    case TimelineTargetKind::Sun:
      return kind == PropertyKind::SunDirection ||
             kind == PropertyKind::SunIntensity || kind == PropertyKind::SunColor;
  }
  return false;
}

bool isAuthorablePayloadForTarget(TimelineTargetKind parTargetKind,
                                  const SequenceClipPayload& parPayload) {
  if (!isPayloadCompatibleWithTarget(parTargetKind, parPayload)) {
    return false;
  }
  if (const auto* property = std::get_if<PropertyClip>(&parPayload)) {
    return property->kind != PropertyKind::LegacyPointLightEnabled &&
           property->kind != PropertyKind::LegacyPointLightRange;
  }
  return true;
}

void evaluateTargetSequence(const TargetSequence& parSequence,
                            FilmFrame parLocalFrame, FilmFrameState& parState) {
  if (const auto* entity = std::get_if<CapturedEntityBaseState>(&parSequence.captured_base)) {
    setTransform(parState, parSequence.target.entity, entity->transform);
    if (entity->camera.has_value()) {
      setProperty(parState, parSequence.target.entity, FilmPropertyKind::CameraFov,
                  glm::vec4(entity->camera->vertical_fov_degrees));
      parState.camera_output = {FilmOutputKind::Camera,
                                 {{parSequence.target.entity, entity->transform,
                                   entity->camera->vertical_fov_degrees,
                                   entity->camera->near_plane,
                                   entity->camera->far_plane}}};
    }
    if (entity->point_light.has_value()) {
      setProperty(parState, parSequence.target.entity, FilmPropertyKind::LightEnabled,
                  glm::vec4(entity->point_light->enabled ? 1.0f : 0.0f));
      setProperty(parState, parSequence.target.entity, FilmPropertyKind::LightColor,
                  glm::vec4(entity->point_light->color, 1.0f));
      setProperty(parState, parSequence.target.entity, FilmPropertyKind::LightIntensity,
                  glm::vec4(entity->point_light->intensity));
      setProperty(parState, parSequence.target.entity, FilmPropertyKind::LightRange,
                  glm::vec4(entity->point_light->range));
    }
  } else if (const auto* sun = std::get_if<CapturedSunBaseState>(&parSequence.captured_base)) {
    parState.sun = {{sun->direction_to_sun, sun->color, sun->intensity}};
  }

  std::vector<const SequenceClip*> movements;
  for (const SequenceClip& clip : parSequence.clips) {
    if (std::holds_alternative<MovementClip>(clip.payload)) {
      movements.push_back(&clip);
    }
  }
  std::sort(movements.begin(), movements.end(), [](const SequenceClip* left,
                                                    const SequenceClip* right) {
    return left->start_frame < right->start_frame;
  });
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
  }

  for (int lane = 2; lane < 2 + static_cast<int>(PropertyKind::LegacyPointLightRange) + 1;
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
    if (const auto kind = legacyPropertyKind(property.kind); kind.has_value()) {
      setProperty(parState, parSequence.target.entity, *kind, value);
    } else {
      if (!parState.sun.has_value()) {
        parState.sun = EvaluatedSunState{};
      }
      if (property.kind == PropertyKind::SunDirection) {
        parState.sun->direction_to_sun = glm::vec3(value);
      } else if (property.kind == PropertyKind::SunColor) {
        parState.sun->color = glm::vec3(value);
      } else if (property.kind == PropertyKind::SunIntensity) {
        parState.sun->intensity = value.x;
      }
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
    RigAnimationOverride override{parSequence.target.entity, asLegacyAnimation(animation),
                                  static_cast<float>(elapsed_frames) /
                                      static_cast<float>(FILM_FPS) * animation.speed,
                                  1.0f};
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

  if (parState.camera_output.camera.has_value() &&
      parState.camera_output.camera->source_entity == parSequence.target.entity) {
    const auto transform = std::find_if(parState.transforms.begin(), parState.transforms.end(),
                                        [&parSequence](const TransformOverride& item) {
                                          return item.entity == parSequence.target.entity;
                                        });
    const auto fov = std::find_if(parState.properties.begin(), parState.properties.end(),
                                  [&parSequence](const PropertyOverride& item) {
                                    return item.entity == parSequence.target.entity &&
                                           item.kind == FilmPropertyKind::CameraFov;
                                  });
    if (transform != parState.transforms.end()) {
      parState.camera_output.camera->transform = transform->transform;
    }
    if (fov != parState.properties.end()) {
      parState.camera_output.camera->vertical_fov_degrees = fov->value.x;
    }
  }
}

FilmFrameState evaluateMovieTimeline(const MovieTimeline& parTimeline,
                                     FilmFrame parFrame) {
  FilmFrameState state;
  std::vector<TimelineTarget> targets;
  for (const SequenceInstance& instance : parTimeline.instances) {
    const TargetSequence* sequence = parTimeline.findSequence(instance.sequence_id);
    if (sequence != nullptr &&
        std::find(targets.begin(), targets.end(), sequence->target) == targets.end()) {
      targets.push_back(sequence->target);
    }
  }

  for (const TimelineTarget& target : targets) {
    struct Candidate final {
      const SequenceInstance* instance;
      const TargetSequence* sequence;
    };
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
    evaluateTargetSequence(*selected.sequence, local_frame, state);
  }

  struct CameraCandidate final {
    const SequenceInstance* instance;
    const TargetSequence* sequence;
  };
  std::optional<CameraCandidate> camera;
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
  state.camera_output = {};
  state.active_camera.reset();
  if (camera.has_value()) {
    FilmFrameState camera_state;
    const FilmFrame local = std::min(parFrame - camera->instance->start_frame,
                                     camera->sequence->durationFrames());
    evaluateTargetSequence(*camera->sequence, local, camera_state);
    state.camera_output = camera_state.camera_output;
    if (state.camera_output.camera.has_value()) {
      state.active_camera = state.camera_output.camera->source_entity;
    }
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
      if (second_sequence == nullptr || !sameTarget(first_sequence->target, second_sequence->target) ||
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

}  // namespace kage::film
