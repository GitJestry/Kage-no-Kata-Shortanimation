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
#include <cmath>

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

[[nodiscard]] TimelineTargetKind readTargetKind(const json& parJson) {
  const int raw = parJson.get<int>();
  return raw >= static_cast<int>(TimelineTargetKind::RiggedEntity) &&
                 raw <= static_cast<int>(TimelineTargetKind::Sun)
             ? static_cast<TimelineTargetKind>(raw)
             : TimelineTargetKind::RiggedEntity;
}

[[nodiscard]] MovieTimeline readMovieTimeline(const json& parJson) {
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
        value.source_in = clip_json.value("source_in", 0.0f);
        value.source_out = clip_json.value("source_out", 1.0f);
        value.speed = clip_json.value("speed", 1.0f);
        value.looping = clip_json.value("looping", false);
        clip.payload = value;
      } else if (type == "property") {
        PropertyClip value;
        value.kind = static_cast<PropertyKind>(std::clamp(
            clip_json.value("kind", 0), 0,
            static_cast<int>(PropertyKind::SunColor)));
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

bool decodeMovieTimeline(std::string_view parJson, MovieTimeline& parTimeline,
                         std::string& parError) {
  parError.clear();
  try {
    const json document = parJson.empty() ? json::object() : json::parse(parJson);
    if (parJson.empty() || document.is_null()) {
      parTimeline = {};
      return true;
    }
    if (!document.is_object()) {
      parError = "Film data is not an object";
      return false;
    }
    const auto schema = document.find("schema_version");
    if (schema == document.end() || *schema != 2) {
      parError = "Unsupported Film schema version; expected 2";
      return false;
    }
    parTimeline = readMovieTimeline(document);
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
                          {"source_in", rig->source_in},
                          {"source_out", rig->source_out},
                          {"speed", rig->speed},
                          {"looping", rig->looping}});
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
