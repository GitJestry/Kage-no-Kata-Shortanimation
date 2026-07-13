#include "film/movie_timeline_serializer.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-literal-operator"
#endif
#include <json.hpp>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <variant>

namespace {

using json = nlohmann::json;
using namespace kage;
using namespace kage::film;

[[nodiscard]] glm::vec3 readVec3(const json& parJson,
                                 const glm::vec3& parFallback = {}) {
  if (!parJson.is_array() || parJson.size() != 3) {
    return parFallback;
  }
  return {parJson[0].get<float>(), parJson[1].get<float>(),
          parJson[2].get<float>()};
}

[[nodiscard]] glm::vec4 readVec4(const json& parJson,
                                 const glm::vec4& parFallback = {}) {
  if (!parJson.is_array() || parJson.size() != 4) {
    return parFallback;
  }
  return {parJson[0].get<float>(), parJson[1].get<float>(),
          parJson[2].get<float>(), parJson[3].get<float>()};
}

[[nodiscard]] glm::quat readQuat(const json& parJson,
                                 const glm::quat& parFallback = {}) {
  if (!parJson.is_array() || parJson.size() != 4) {
    return parFallback;
  }
  const glm::quat value(parJson[0].get<float>(), parJson[1].get<float>(),
                        parJson[2].get<float>(), parJson[3].get<float>());
  const float length = glm::length(value);
  return std::isfinite(length) && length > 0.00001f ? glm::normalize(value)
                                                    : parFallback;
}

[[nodiscard]] math::Transform readTransform(const json& parJson) {
  math::Transform value;
  if (!parJson.is_object()) {
    return value;
  }
  value.translation =
      readVec3(parJson.value("position", json::array()), value.translation);
  value.rotation =
      readQuat(parJson.value("rotation", json::array()), value.rotation);
  value.scale = readVec3(parJson.value("scale", json::array()), value.scale);
  return value;
}

[[nodiscard]] json writeVec3(const glm::vec3& parValue) {
  return json::array({parValue.x, parValue.y, parValue.z});
}

[[nodiscard]] json writeVec4(const glm::vec4& parValue) {
  return json::array({parValue.x, parValue.y, parValue.z, parValue.w});
}

[[nodiscard]] json writeQuat(const glm::quat& parValue) {
  return json::array({parValue.w, parValue.x, parValue.y, parValue.z});
}

[[nodiscard]] json writeTransform(const math::Transform& parValue) {
  return {{"position", writeVec3(parValue.translation)},
          {"rotation", writeQuat(parValue.rotation)},
          {"scale", writeVec3(parValue.scale)}};
}

struct LegacyMovementDto final {
  math::Transform start;
  math::Transform end;
  glm::vec3 position_control_1{0.0f};
  glm::vec3 position_control_2{0.0f};
  float timing_control_1 = 1.0f / 3.0f;
  float timing_control_2 = 2.0f / 3.0f;
  bool automatic_position_controls = true;
};

struct LegacyRigDto final {
  assets::AnimationClipId clip_id = 0;
  std::size_t legacy_clip_index = 0;
  float source_in = 0.0f;
  float source_out = 1.0f;
  float speed = 1.0f;
  bool looping = false;
  float blend_in_seconds = 0.0f;
  float blend_out_seconds = 0.0f;
};

struct LegacyPropertyDto final {
  int kind = 0;
  glm::vec4 start_value{0.0f};
  glm::vec4 control_1{0.0f};
  glm::vec4 control_2{0.0f};
  glm::vec4 end_value{0.0f};
};

using LegacyPayload =
    std::variant<LegacyMovementDto, LegacyRigDto, LegacyPropertyDto>;

struct LegacyClipDto final {
  std::uint64_t id = 0;
  FilmFrame start_frame = 0;
  FilmFrame end_frame = 1;
  LegacyPayload payload;
};

struct LegacyTrackDto final {
  scene::EntityId target;
  std::vector<LegacyClipDto> clips;
};

struct LegacyCutDto final {
  std::uint64_t id = 0;
  FilmFrame start_frame = 0;
  FilmFrame end_frame = 1;
  scene::EntityId camera;
};

struct LegacyFilmDto final {
  std::string name = "Kage no Kata";
  FilmFrame duration_frames = 300;
  std::vector<LegacyTrackDto> tracks;
  std::vector<LegacyCutDto> cuts;
};

[[nodiscard]] LegacyFilmDto parseLegacy(const json& parJson) {
  LegacyFilmDto result;
  result.name = parJson.value("name", result.name);
  result.duration_frames = parJson.value("duration_frames", result.duration_frames);
  const json cuts = parJson.contains("camera_cuts")
                        ? parJson.value("camera_cuts", json::array())
                        : parJson.value("shots", json::array());
  for (const json& item : cuts) {
    LegacyCutDto cut;
    cut.id = item.value("id", std::uint64_t{0});
    cut.start_frame = item.value("start_frame", 0);
    cut.end_frame = item.value("end_frame", cut.start_frame + 1);
    cut.camera.value = item.value("camera", scene::EntityId{}.value);
    result.cuts.push_back(cut);
  }
  for (const json& track_json : parJson.value("tracks", json::array())) {
    LegacyTrackDto track;
    track.target.value = track_json.value("target", scene::EntityId{}.value);
    for (const json& clip_json : track_json.value("clips", json::array())) {
      LegacyClipDto clip;
      clip.id = clip_json.value("id", std::uint64_t{0});
      clip.start_frame = clip_json.value("start_frame", 0);
      clip.end_frame = clip_json.value("end_frame", clip.start_frame + 1);
      const std::string type = clip_json.value("type", "movement");
      if (type == "rig") {
        LegacyRigDto value;
        value.clip_id = clip_json.value("clip_id", assets::AnimationClipId{0});
        value.legacy_clip_index = clip_json.value("clip_index", std::size_t{0});
        value.source_in = clip_json.value("source_in", 0.0f);
        value.source_out = clip_json.value("source_out", 1.0f);
        value.speed = clip_json.value("speed", 1.0f);
        value.looping = clip_json.value("looping", false);
        value.blend_in_seconds = clip_json.value("blend_in", 0.0f);
        value.blend_out_seconds = clip_json.value("blend_out", 0.0f);
        clip.payload = value;
      } else if (type == "property") {
        LegacyPropertyDto value;
        value.kind = clip_json.value("kind", 0);
        value.start_value = readVec4(clip_json.value("start_value", json::array()));
        value.control_1 = readVec4(clip_json.value("control_1", json::array()),
                                   value.start_value);
        value.control_2 = readVec4(clip_json.value("control_2", json::array()),
                                   value.start_value);
        value.end_value = readVec4(clip_json.value("end_value", json::array()),
                                   value.start_value);
        clip.payload = value;
      } else {
        LegacyMovementDto value;
        value.start = readTransform(clip_json.value("start", json::object()));
        value.end = readTransform(clip_json.value("end", json::object()));
        value.position_control_1 =
            readVec3(clip_json.value("control_1", json::array()));
        value.position_control_2 =
            readVec3(clip_json.value("control_2", json::array()));
        value.timing_control_1 = clip_json.value("timing_1", 1.0f / 3.0f);
        value.timing_control_2 = clip_json.value("timing_2", 2.0f / 3.0f);
        value.automatic_position_controls =
            clip_json.value("automatic_controls", true);
        clip.payload = value;
      }
      track.clips.push_back(std::move(clip));
    }
    result.tracks.push_back(std::move(track));
  }
  if (result.tracks.empty()) {
    std::uint64_t next_clip_id = 1;
    const auto append_key_tracks = [&](const json& tracks,
                                       const char* target_field,
                                       bool include_fov) {
      for (const json& track_json : tracks) {
        LegacyTrackDto track;
        track.target.value =
            track_json.value(target_field, scene::EntityId{}.value);
        const json keys = track_json.value("keys", json::array());
        if (!keys.empty()) {
          const FilmFrame first_frame = keys.front().value("frame", 0);
          if (first_frame > 0) {
            LegacyMovementDto held;
            held.start = held.end =
                readTransform(keys.front().value("transform", json::object()));
            track.clips.push_back(
                {next_clip_id++, 0, first_frame, std::move(held)});
            if (include_fov) {
              LegacyPropertyDto fov;
              fov.kind = 0;
              fov.start_value = fov.control_1 = fov.control_2 = fov.end_value =
                  glm::vec4(keys.front().value("fov", 50.0f));
              track.clips.push_back(
                  {next_clip_id++, 0, first_frame, std::move(fov)});
            }
          }
        }
        for (std::size_t index = 0; index < keys.size(); ++index) {
          const json& left = keys[index];
          const json& right = index + 1 < keys.size() ? keys[index + 1] : left;
          const FilmFrame start = left.value("frame", 0);
          const FilmFrame end = index + 1 < keys.size()
                                    ? right.value("frame", start + 1)
                                    : result.duration_frames;
          if (end <= start) {
            continue;
          }
          LegacyMovementDto movement;
          movement.start = readTransform(left.value("transform", json::object()));
          movement.end = readTransform(right.value("transform", json::object()));
          movement.automatic_position_controls =
              left.value("automatic_handles", true) &&
              right.value("automatic_handles", true);
          movement.position_control_1 = movement.start.translation +
                                        readVec3(left.value("position_out", json::array()));
          movement.position_control_2 = movement.end.translation +
                                        readVec3(right.value("position_in", json::array()));
          track.clips.push_back(
              {next_clip_id++, start, end, std::move(movement)});
          if (include_fov) {
            LegacyPropertyDto fov;
            fov.kind = 0;
            fov.start_value = glm::vec4(left.value("fov", 50.0f));
            fov.end_value = glm::vec4(right.value("fov", fov.start_value.x));
            fov.control_1 = glm::mix(fov.start_value, fov.end_value, 1.0f / 3.0f);
            fov.control_2 = glm::mix(fov.start_value, fov.end_value, 2.0f / 3.0f);
            track.clips.push_back(
                {next_clip_id++, start, end, std::move(fov)});
          }
        }
        result.tracks.push_back(std::move(track));
      }
    };
    append_key_tracks(parJson.value("camera_tracks", json::array()), "camera",
                      true);
    append_key_tracks(parJson.value("transform_tracks", json::array()),
                      "entity", false);
  }
  return result;
}

[[nodiscard]] CapturedEntityBaseState captureEntityBase(
    const scene::World& parWorld, scene::EntityId parEntity,
    TimelineTargetKind parKind) {
  CapturedEntityBaseState base;
  const scene::EntityRecord* entity = parWorld.findEntity(parEntity);
  if (entity != nullptr) {
    base.transform = entity->transform.transform;
  }
  if (parKind == TimelineTargetKind::Camera) {
    CapturedCameraState camera;
    if (entity != nullptr && entity->camera.has_value()) {
      camera = {entity->camera->vertical_fov_degrees, entity->camera->near_plane,
                entity->camera->far_plane};
    }
    base.camera = camera;
  } else if (parKind == TimelineTargetKind::PointLight) {
    CapturedPointLightState light;
    if (entity != nullptr && entity->light.has_value()) {
      light = {entity->light->enabled, entity->light->color,
               entity->light->intensity, entity->light->range,
               entity->light->casts_shadows};
    }
    base.point_light = light;
  }
  return base;
}

[[nodiscard]] TimelineTargetKind targetKind(const scene::World& parWorld,
                                            scene::EntityId parEntity) {
  const scene::EntityRecord* entity = parWorld.findEntity(parEntity);
  if (entity != nullptr && entity->camera.has_value()) {
    return TimelineTargetKind::Camera;
  }
  if (entity != nullptr && entity->light.has_value() &&
      entity->light->type == scene::LightType::Point) {
    return TimelineTargetKind::PointLight;
  }
  return TimelineTargetKind::RiggedEntity;
}

template <typename Value>
[[nodiscard]] std::array<Value, 4> cubicSegment(const Value& parP0,
                                                const Value& parP1,
                                                const Value& parP2,
                                                const Value& parP3,
                                                float parStart, float parEnd) {
  const auto split = [](const std::array<Value, 4>& p, float t) {
    const Value a = glm::mix(p[0], p[1], t);
    const Value b = glm::mix(p[1], p[2], t);
    const Value c = glm::mix(p[2], p[3], t);
    const Value d = glm::mix(a, b, t);
    const Value e = glm::mix(b, c, t);
    const Value f = glm::mix(d, e, t);
    return std::pair{std::array<Value, 4>{p[0], a, d, f},
                     std::array<Value, 4>{f, e, c, p[3]}};
  };
  const std::array<Value, 4> source{parP0, parP1, parP2, parP3};
  const auto [left, ignored] = split(source, std::clamp(parEnd, 0.0f, 1.0f));
  static_cast<void>(ignored);
  if (parStart <= 0.0f || parEnd <= 0.0f) {
    return left;
  }
  const auto [discarded, segment] =
      split(left, std::clamp(parStart / parEnd, 0.0f, 1.0f));
  static_cast<void>(discarded);
  return segment;
}

[[nodiscard]] float cubic(float p0, float p1, float p2, float p3, float t) {
  const float inverse = 1.0f - t;
  return inverse * inverse * inverse * p0 +
         3.0f * inverse * inverse * t * p1 +
         3.0f * inverse * t * t * p2 + t * t * t * p3;
}

[[nodiscard]] MovementClip clipMovement(const LegacyMovementDto& parValue,
                                        float parStart, float parEnd) {
  const float timing_start = cubic(0.0f, parValue.timing_control_1,
                                   parValue.timing_control_2, 1.0f, parStart);
  const float timing_end = cubic(0.0f, parValue.timing_control_1,
                                 parValue.timing_control_2, 1.0f, parEnd);
  const auto timing = cubicSegment(0.0f, parValue.timing_control_1,
                                   parValue.timing_control_2, 1.0f, parStart,
                                   parEnd);
  const glm::vec3 delta = parValue.end.translation - parValue.start.translation;
  const glm::vec3 control_1 = parValue.automatic_position_controls
                                  ? parValue.start.translation + delta / 3.0f
                                  : parValue.position_control_1;
  const glm::vec3 control_2 = parValue.automatic_position_controls
                                  ? parValue.end.translation - delta / 3.0f
                                  : parValue.position_control_2;
  const auto position = cubicSegment(parValue.start.translation, control_1,
                                     control_2, parValue.end.translation,
                                     timing_start, timing_end);
  MovementClip result;
  result.start_mode = MovementStartMode::ExplicitPosition;
  math::Transform start = parValue.start;
  math::Transform end = parValue.end;
  start.translation = position[0];
  end.translation = position[3];
  glm::quat target_rotation = parValue.end.rotation;
  if (glm::dot(parValue.start.rotation, target_rotation) < 0.0f) {
    target_rotation = -target_rotation;
  }
  start.rotation = glm::normalize(
      glm::slerp(parValue.start.rotation, target_rotation, timing_start));
  end.rotation = glm::normalize(
      glm::slerp(parValue.start.rotation, target_rotation, timing_end));
  start.scale = glm::mix(parValue.start.scale, parValue.end.scale, timing_start);
  end.scale = glm::mix(parValue.start.scale, parValue.end.scale, timing_end);
  result.explicit_start = start;
  result.end = end;
  result.curve.position_control_1 = position[1];
  result.curve.position_control_2 = position[2];
  result.curve.automatic_position_controls = false;
  const float timing_span = timing[3] - timing[0];
  if (std::abs(timing_span) > 0.000001f) {
    result.curve.timing_control_1 = (timing[1] - timing[0]) / timing_span;
    result.curve.timing_control_2 = (timing[2] - timing[0]) / timing_span;
  }
  return result;
}

[[nodiscard]] std::optional<PropertyKind> propertyKindFromLegacy(int parKind) {
  switch (parKind) {
    case 0:
      return PropertyKind::CameraFov;
    case 1:
      return PropertyKind::LegacyPointLightEnabled;
    case 2:
      return PropertyKind::PointLightIntensity;
    case 3:
      return PropertyKind::PointLightColor;
    case 4:
      return PropertyKind::LegacyPointLightRange;
    default:
      return std::nullopt;
  }
}

[[nodiscard]] PropertyClip clipProperty(const LegacyPropertyDto& parValue,
                                        float parStart, float parEnd,
                                        PropertyKind parKind) {
  const auto values = cubicSegment(parValue.start_value, parValue.control_1,
                                   parValue.control_2, parValue.end_value,
                                   parStart, parEnd);
  return {parKind, values[0], values[1], values[2], values[3]};
}

[[nodiscard]] SequenceClipId allocateClipId(MovieTimeline& parTimeline,
                                            SequenceClipId parPreferred,
                                            std::set<SequenceClipId>& parUsed) {
  if (parPreferred != 0 && !parUsed.contains(parPreferred)) {
    parUsed.insert(parPreferred);
    parTimeline.next_clip_id = std::max(parTimeline.next_clip_id, parPreferred + 1);
    return parPreferred;
  }
  while (parTimeline.next_clip_id == 0 ||
         parUsed.contains(parTimeline.next_clip_id)) {
    ++parTimeline.next_clip_id;
  }
  const SequenceClipId result = parTimeline.next_clip_id++;
  parUsed.insert(result);
  return result;
}

void addConstantCameraGaps(TargetSequence& parSequence, FilmFrame parDuration,
                           bool parMovement, MovieTimeline& parTimeline,
                           std::set<SequenceClipId>& parUsed) {
  std::vector<std::pair<FilmFrame, FilmFrame>> occupied;
  for (const SequenceClip& clip : parSequence.clips) {
    const bool matches = parMovement
                             ? std::holds_alternative<MovementClip>(clip.payload)
                             : (std::get_if<PropertyClip>(&clip.payload) != nullptr &&
                                std::get<PropertyClip>(clip.payload).kind ==
                                    PropertyKind::CameraFov);
    if (matches) {
      occupied.emplace_back(clip.start_frame, clip.end_frame);
    }
  }
  std::sort(occupied.begin(), occupied.end());
  const auto& base = std::get<CapturedEntityBaseState>(parSequence.captured_base);
  FilmFrame cursor = 0;
  const auto add_gap = [&](FilmFrame start, FilmFrame end) {
    if (end <= start) {
      return;
    }
    SequenceClip clip;
    clip.id = allocateClipId(parTimeline, 0, parUsed);
    clip.start_frame = start;
    clip.end_frame = end;
    if (parMovement) {
      MovementClip movement;
      movement.start_mode = MovementStartMode::ExplicitPosition;
      movement.explicit_start = base.transform;
      movement.end = base.transform;
      clip.payload = movement;
    } else {
      const float fov = base.camera->vertical_fov_degrees;
      PropertyClip property;
      property.kind = PropertyKind::CameraFov;
      property.start_value = property.control_1 = property.control_2 =
          property.end_value = glm::vec4(fov);
      clip.payload = property;
    }
    parSequence.clips.push_back(std::move(clip));
  };
  for (const auto [start, end] : occupied) {
    add_gap(cursor, std::min(start, parDuration));
    cursor = std::max(cursor, std::min(end, parDuration));
  }
  add_gap(cursor, parDuration);
}

[[nodiscard]] bool appendLegacyCameraClip(
    TargetSequence& parSequence, const LegacyClipDto& parLegacyClip,
    FilmFrame parStart, FilmFrame parEnd, FilmFrame parLocalOrigin,
    MovieTimeline& parTimeline, std::set<SequenceClipId>& parUsed) {
  if (parEnd <= parStart ||
      parLegacyClip.end_frame <= parLegacyClip.start_frame) {
    return false;
  }
  const float u0 = static_cast<float>(parStart - parLegacyClip.start_frame) /
                   static_cast<float>(parLegacyClip.end_frame -
                                      parLegacyClip.start_frame);
  const float u1 = static_cast<float>(parEnd - parLegacyClip.start_frame) /
                   static_cast<float>(parLegacyClip.end_frame -
                                      parLegacyClip.start_frame);
  SequenceClipPayload payload;
  if (const auto* movement =
          std::get_if<LegacyMovementDto>(&parLegacyClip.payload)) {
    payload = clipMovement(*movement, u0, u1);
  } else if (const auto* property =
                 std::get_if<LegacyPropertyDto>(&parLegacyClip.payload);
             property != nullptr && property->kind == 0) {
    payload = clipProperty(*property, u0, u1, PropertyKind::CameraFov);
  } else {
    return false;
  }
  SequenceClip clip;
  clip.id = allocateClipId(parTimeline, parLegacyClip.id, parUsed);
  clip.start_frame = parStart - parLocalOrigin;
  clip.end_frame = parEnd - parLocalOrigin;
  clip.payload = std::move(payload);
  parSequence.clips.push_back(std::move(clip));
  return true;
}

[[nodiscard]] MovieTimeline migrateLegacy(const LegacyFilmDto& parLegacy,
                                          const scene::World& parWorld) {
  MovieTimeline result;
  result.name = parLegacy.name;
  result.camera_gap_mode = CameraGapMode::Black;
  std::set<SequenceClipId> used_clip_ids;
  const auto make_sequence = [&](std::string name, TimelineTarget target,
                                 CapturedTargetBaseState base) -> TargetSequence& {
    TargetSequence sequence;
    sequence.id = result.next_sequence_id++;
    sequence.name = std::move(name);
    sequence.target = target;
    sequence.captured_base = std::move(base);
    result.sequences.push_back(std::move(sequence));
    return result.sequences.back();
  };
  const auto place = [&](TargetSequenceId sequence, FilmFrame start) {
    result.instances.push_back({result.next_instance_id++, sequence, start});
  };

  for (const LegacyTrackDto& track : parLegacy.tracks) {
    const TimelineTargetKind kind = targetKind(parWorld, track.target);
    if (kind == TimelineTargetKind::Camera) {
      continue;
    }
    TargetSequence& sequence = make_sequence(
        "Migrated Track", {kind, track.target},
        captureEntityBase(parWorld, track.target, kind));
    for (const LegacyClipDto& legacy_clip : track.clips) {
      SequenceClip clip;
      clip.id = allocateClipId(result, legacy_clip.id, used_clip_ids);
      clip.start_frame = legacy_clip.start_frame;
      clip.end_frame = legacy_clip.end_frame;
      if (const auto* movement = std::get_if<LegacyMovementDto>(&legacy_clip.payload)) {
        clip.payload = clipMovement(*movement, 0.0f, 1.0f);
      } else if (const auto* rig = std::get_if<LegacyRigDto>(&legacy_clip.payload)) {
        clip.payload = RigAnimationClip{rig->clip_id, rig->legacy_clip_index,
                                        rig->source_in, rig->source_out,
                                        rig->speed, rig->looping,
                                        rig->blend_in_seconds,
                                        rig->blend_out_seconds};
      } else if (const auto kind_value = propertyKindFromLegacy(
                     std::get<LegacyPropertyDto>(legacy_clip.payload).kind);
                 kind_value.has_value()) {
        clip.payload = clipProperty(std::get<LegacyPropertyDto>(legacy_clip.payload),
                                    0.0f, 1.0f, *kind_value);
      } else {
        continue;
      }
      sequence.clips.push_back(std::move(clip));
    }
    if (sequence.durationFrames() > 0) {
      place(sequence.id, 0);
    }
  }

  bool has_valid_cut = false;
  for (const LegacyCutDto& cut : parLegacy.cuts) {
    if (cut.end_frame <= cut.start_frame) {
      continue;
    }
    has_valid_cut = true;
    TargetSequence& sequence = make_sequence(
        "Migrated Camera Cut", {TimelineTargetKind::Camera, cut.camera},
        captureEntityBase(parWorld, cut.camera, TimelineTargetKind::Camera));
    for (const LegacyTrackDto& track : parLegacy.tracks) {
      if (track.target != cut.camera) {
        continue;
      }
      for (const LegacyClipDto& legacy_clip : track.clips) {
        const FilmFrame start = std::max(legacy_clip.start_frame, cut.start_frame);
        const FilmFrame end = std::min(legacy_clip.end_frame, cut.end_frame);
        static_cast<void>(appendLegacyCameraClip(
            sequence, legacy_clip, start, end, cut.start_frame, result,
            used_clip_ids));
      }
    }
    const FilmFrame duration = cut.end_frame - cut.start_frame;
    addConstantCameraGaps(sequence, duration, true, result, used_clip_ids);
    addConstantCameraGaps(sequence, duration, false, result, used_clip_ids);
    place(sequence.id, cut.start_frame);
  }

  for (const LegacyTrackDto& track : parLegacy.tracks) {
    if (targetKind(parWorld, track.target) != TimelineTargetKind::Camera) {
      continue;
    }
    TargetSequence& sequence = make_sequence(
        has_valid_cut ? "Migrated Camera Remainder" : "Migrated Camera Track",
        {TimelineTargetKind::Camera, track.target},
        captureEntityBase(parWorld, track.target, TimelineTargetKind::Camera));
    for (const LegacyClipDto& legacy_clip : track.clips) {
      if (legacy_clip.end_frame <= legacy_clip.start_frame) {
        continue;
      }
      if (!has_valid_cut) {
        static_cast<void>(appendLegacyCameraClip(
            sequence, legacy_clip, legacy_clip.start_frame,
            legacy_clip.end_frame, 0, result, used_clip_ids));
        continue;
      }
      std::vector<std::pair<FilmFrame, FilmFrame>> covered;
      for (const LegacyCutDto& cut : parLegacy.cuts) {
        if (cut.camera != track.target || cut.end_frame <= cut.start_frame) {
          continue;
        }
        const FilmFrame start = std::max(legacy_clip.start_frame, cut.start_frame);
        const FilmFrame end = std::min(legacy_clip.end_frame, cut.end_frame);
        if (end > start) {
          covered.emplace_back(start, end);
        }
      }
      std::sort(covered.begin(), covered.end());
      FilmFrame cursor = legacy_clip.start_frame;
      for (const auto [start, end] : covered) {
        if (start > cursor) {
          static_cast<void>(appendLegacyCameraClip(
              sequence, legacy_clip, cursor, start, 0, result, used_clip_ids));
        }
        cursor = std::max(cursor, end);
      }
      if (cursor < legacy_clip.end_frame) {
        static_cast<void>(appendLegacyCameraClip(
            sequence, legacy_clip, cursor, legacy_clip.end_frame, 0, result,
            used_clip_ids));
      }
    }
    if (!has_valid_cut && sequence.durationFrames() > 0) {
      place(sequence.id, 0);
    }
  }
  return result;
}

[[nodiscard]] TimelineTargetKind readTargetKind(const json& parJson) {
  const int raw = parJson.get<int>();
  return raw >= static_cast<int>(TimelineTargetKind::RiggedEntity) &&
                 raw <= static_cast<int>(TimelineTargetKind::Sun)
             ? static_cast<TimelineTargetKind>(raw)
             : TimelineTargetKind::RiggedEntity;
}

[[nodiscard]] MovieTimeline readNewTimeline(const json& parJson) {
  MovieTimeline result;
  result.name = parJson.value("name", result.name);
  result.camera_gap_mode = static_cast<CameraGapMode>(
      std::clamp(parJson.value("camera_gap_mode", 0), 0, 1));
  result.next_sequence_id = parJson.value("next_sequence_id", std::uint64_t{1});
  result.next_instance_id = parJson.value("next_instance_id", std::uint64_t{1});
  result.next_clip_id = parJson.value("next_clip_id", std::uint64_t{1});
  for (const json& sequence_json : parJson.value("sequences", json::array())) {
    TargetSequence sequence;
    sequence.id = sequence_json.value("id", std::uint64_t{0});
    sequence.name = sequence_json.value("name", std::string{});
    const json target_json = sequence_json.value("target", json::object());
    sequence.target.kind = readTargetKind(target_json.value("kind", 0));
    sequence.target.entity.value =
        target_json.value("entity", scene::EntityId{}.value);
    const json base_json = sequence_json.value("captured_base", json::object());
    if (sequence.target.kind == TimelineTargetKind::Sun) {
      CapturedSunBaseState base;
      base.direction_to_sun = readVec3(
          base_json.value("direction_to_sun", json::array()),
          base.direction_to_sun);
      base.color = readVec3(base_json.value("color", json::array()), base.color);
      base.intensity = base_json.value("intensity", base.intensity);
      sequence.captured_base = base;
    } else {
      CapturedEntityBaseState base;
      base.transform = readTransform(base_json.value("transform", json::object()));
      if (base_json.contains("camera")) {
        const json camera_json = base_json["camera"];
        base.camera = CapturedCameraState{
            camera_json.value("fov", 45.0f), camera_json.value("near", 0.1f),
            camera_json.value("far", 1000.0f)};
      }
      if (base_json.contains("point_light")) {
        const json light_json = base_json["point_light"];
        CapturedPointLightState light;
        light.enabled = light_json.value("enabled", light.enabled);
        light.color = readVec3(light_json.value("color", json::array()), light.color);
        light.intensity = light_json.value("intensity", light.intensity);
        light.range = light_json.value("range", light.range);
        light.casts_shadows = light_json.value("casts_shadows", light.casts_shadows);
        base.point_light = light;
      }
      sequence.captured_base = base;
    }
    for (const json& clip_json : sequence_json.value("clips", json::array())) {
      SequenceClip clip;
      clip.id = clip_json.value("id", std::uint64_t{0});
      clip.start_frame = clip_json.value("start_frame", 0);
      clip.end_frame = clip_json.value("end_frame", clip.start_frame + 1);
      const std::string type = clip_json.value("type", "movement");
      if (type == "rig") {
        RigAnimationClip value;
        value.clip_id = clip_json.value("clip_id", assets::AnimationClipId{0});
        value.legacy_clip_index = clip_json.value("legacy_clip_index", std::size_t{0});
        value.source_in = clip_json.value("source_in", 0.0f);
        value.source_out = clip_json.value("source_out", 1.0f);
        value.speed = clip_json.value("speed", 1.0f);
        value.looping = clip_json.value("looping", false);
        value.blend_in_seconds = clip_json.value("blend_in", 0.0f);
        value.blend_out_seconds = clip_json.value("blend_out", 0.0f);
        clip.payload = value;
      } else if (type == "property") {
        PropertyClip value;
        value.kind = static_cast<PropertyKind>(std::clamp(
            clip_json.value("kind", 0), 0,
            static_cast<int>(PropertyKind::LegacyPointLightRange)));
        value.start_value = readVec4(clip_json.value("start_value", json::array()));
        value.control_1 = readVec4(clip_json.value("control_1", json::array()),
                                   value.start_value);
        value.control_2 = readVec4(clip_json.value("control_2", json::array()),
                                   value.start_value);
        value.end_value = readVec4(clip_json.value("end_value", json::array()),
                                   value.start_value);
        clip.payload = value;
      } else {
        MovementClip value;
        value.start_mode = static_cast<MovementStartMode>(
            std::clamp(clip_json.value("start_mode", 0), 0, 1));
        if (clip_json.contains("explicit_start")) {
          value.explicit_start = readTransform(clip_json["explicit_start"]);
        }
        value.end = readTransform(clip_json.value("end", json::object()));
        const json curve = clip_json.value("curve", json::object());
        value.curve.position_control_1 =
            readVec3(curve.value("position_control_1", json::array()));
        value.curve.position_control_2 =
            readVec3(curve.value("position_control_2", json::array()));
        value.curve.timing_control_1 = curve.value("timing_control_1", 1.0f / 3.0f);
        value.curve.timing_control_2 = curve.value("timing_control_2", 2.0f / 3.0f);
        value.curve.automatic_position_controls =
            curve.value("automatic_position_controls", true);
        const json transition = clip_json.value("transition_before", json::object());
        value.transition_before.enabled = transition.value("enabled", false);
        if (transition.contains("curve")) {
          const json transition_curve = transition["curve"];
          value.transition_before.curve.position_control_1 = readVec3(
              transition_curve.value("position_control_1", json::array()));
          value.transition_before.curve.position_control_2 = readVec3(
              transition_curve.value("position_control_2", json::array()));
          value.transition_before.curve.timing_control_1 =
              transition_curve.value("timing_control_1", 1.0f / 3.0f);
          value.transition_before.curve.timing_control_2 =
              transition_curve.value("timing_control_2", 2.0f / 3.0f);
          value.transition_before.curve.automatic_position_controls =
              transition_curve.value("automatic_position_controls", true);
        }
        clip.payload = value;
      }
      sequence.clips.push_back(std::move(clip));
    }
    result.sequences.push_back(std::move(sequence));
  }
  for (const json& instance_json : parJson.value("instances", json::array())) {
    result.instances.push_back(
        {instance_json.value("id", std::uint64_t{0}),
         instance_json.value("sequence_id", std::uint64_t{0}),
         instance_json.value("start_frame", 0)});
  }
  return result;
}

[[nodiscard]] json writeCurve(const MovementCurve& parCurve) {
  return {{"position_control_1", writeVec3(parCurve.position_control_1)},
          {"position_control_2", writeVec3(parCurve.position_control_2)},
          {"timing_control_1", parCurve.timing_control_1},
          {"timing_control_2", parCurve.timing_control_2},
          {"automatic_position_controls", parCurve.automatic_position_controls}};
}

}  // namespace

namespace kage::film {

bool decodeMovieTimeline(std::string_view parJson, const scene::World& parWorld,
                         const scene::SunLightSettings& parSun,
                         MovieTimeline& parTimeline, bool& parMigratedLegacy,
                         std::string& parError) {
  static_cast<void>(parSun);
  parError.clear();
  parMigratedLegacy = false;
  try {
    const json document = parJson.empty() ? json::object() : json::parse(parJson);
    if (document.empty() || document.is_null()) {
      parTimeline = {};
      return true;
    }
    if (!document.is_object()) {
      parError = "Film data is not an object";
      return false;
    }
    if (document.value("schema_version", 0) >= 2) {
      parTimeline = readNewTimeline(document);
    } else {
      parTimeline = migrateLegacy(parseLegacy(document), parWorld);
      parMigratedLegacy = true;
    }
    return true;
  } catch (const json::exception& error) {
    parError = error.what();
    return false;
  }
}

std::string encodeMovieTimeline(const MovieTimeline& parTimeline) {
  json output = {{"schema_version", 2},
                 {"name", parTimeline.name},
                 {"fps", FILM_FPS},
                 {"camera_gap_mode", static_cast<int>(parTimeline.camera_gap_mode)},
                 {"next_sequence_id", parTimeline.next_sequence_id},
                 {"next_instance_id", parTimeline.next_instance_id},
                 {"next_clip_id", parTimeline.next_clip_id},
                 {"sequences", json::array()},
                 {"instances", json::array()}};
  for (const TargetSequence& sequence : parTimeline.sequences) {
    json sequence_json = {
        {"id", sequence.id},
        {"name", sequence.name},
        {"target", {{"kind", static_cast<int>(sequence.target.kind)},
                    {"entity", sequence.target.entity.value}}},
        {"clips", json::array()}};
    if (const auto* entity =
            std::get_if<CapturedEntityBaseState>(&sequence.captured_base)) {
      json base = {{"transform", writeTransform(entity->transform)}};
      if (entity->camera.has_value()) {
        base["camera"] = {{"fov", entity->camera->vertical_fov_degrees},
                          {"near", entity->camera->near_plane},
                          {"far", entity->camera->far_plane}};
      }
      if (entity->point_light.has_value()) {
        base["point_light"] = {
            {"enabled", entity->point_light->enabled},
            {"color", writeVec3(entity->point_light->color)},
            {"intensity", entity->point_light->intensity},
            {"range", entity->point_light->range},
            {"casts_shadows", entity->point_light->casts_shadows}};
      }
      sequence_json["captured_base"] = std::move(base);
    } else {
      const auto& sun = std::get<CapturedSunBaseState>(sequence.captured_base);
      sequence_json["captured_base"] = {
          {"direction_to_sun", writeVec3(sun.direction_to_sun)},
          {"color", writeVec3(sun.color)}, {"intensity", sun.intensity}};
    }
    for (const SequenceClip& clip : sequence.clips) {
      json clip_json = {{"id", clip.id},
                        {"start_frame", clip.start_frame},
                        {"end_frame", clip.end_frame}};
      if (const auto* movement = std::get_if<MovementClip>(&clip.payload)) {
        clip_json["type"] = "movement";
        clip_json["start_mode"] = static_cast<int>(movement->start_mode);
        if (movement->explicit_start.has_value()) {
          clip_json["explicit_start"] = writeTransform(*movement->explicit_start);
        }
        clip_json["end"] = writeTransform(movement->end);
        clip_json["curve"] = writeCurve(movement->curve);
        clip_json["transition_before"] = {
            {"enabled", movement->transition_before.enabled},
            {"curve", writeCurve(movement->transition_before.curve)}};
      } else if (const auto* rig =
                     std::get_if<RigAnimationClip>(&clip.payload)) {
        clip_json.update({{"type", "rig"},
                          {"clip_id", rig->clip_id},
                          {"legacy_clip_index", rig->legacy_clip_index},
                          {"source_in", rig->source_in},
                          {"source_out", rig->source_out},
                          {"speed", rig->speed},
                          {"looping", rig->looping},
                          {"blend_in", rig->blend_in_seconds},
                          {"blend_out", rig->blend_out_seconds}});
      } else {
        const auto& property = std::get<PropertyClip>(clip.payload);
        clip_json.update({{"type", "property"},
                          {"kind", static_cast<int>(property.kind)},
                          {"start_value", writeVec4(property.start_value)},
                          {"control_1", writeVec4(property.control_1)},
                          {"control_2", writeVec4(property.control_2)},
                          {"end_value", writeVec4(property.end_value)}});
      }
      sequence_json["clips"].push_back(std::move(clip_json));
    }
    output["sequences"].push_back(std::move(sequence_json));
  }
  for (const SequenceInstance& instance : parTimeline.instances) {
    output["instances"].push_back({{"id", instance.id},
                                   {"sequence_id", instance.sequence_id},
                                   {"start_frame", instance.start_frame}});
  }
  return output.dump();
}

}  // namespace kage::film
