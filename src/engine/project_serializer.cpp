#include "engine/project_serializer.hpp"

#include "engine/engine_core.hpp"
#include "camera/session_camera.hpp"

#include <glm/gtc/quaternion.hpp>

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
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace {

using json = nlohmann::json;

[[nodiscard]] json toJson(const glm::vec3& parValue) {
  return json::array({parValue.x, parValue.y, parValue.z});
}

[[nodiscard]] json toJson(const glm::vec4& parValue) {
  return json::array({parValue.x, parValue.y, parValue.z, parValue.w});
}

[[nodiscard]] json toJson(const glm::quat& parValue) {
  return json::array({parValue.w, parValue.x, parValue.y, parValue.z});
}

[[nodiscard]] kage::assets::AssetId readAssetId(const json& parJson) {
  kage::assets::AssetId id;
  id.value = parJson.value("asset_id", id.value);
  return id;
}

[[nodiscard]] glm::vec3 readVec3(const json& parJson,
                                 const glm::vec3& parFallback) {
  if (!parJson.is_array() || parJson.size() != 3) {
    return parFallback;
  }

  return glm::vec3(parJson[0].get<float>(), parJson[1].get<float>(),
                   parJson[2].get<float>());
}

[[nodiscard]] glm::vec4 readVec4(const json& parJson,
                                 const glm::vec4& parFallback) {
  if (!parJson.is_array() || parJson.size() != 4) {
    return parFallback;
  }
  return glm::vec4(parJson[0].get<float>(), parJson[1].get<float>(),
                   parJson[2].get<float>(), parJson[3].get<float>());
}

[[nodiscard]] glm::vec3 normalizedOrFallback(const glm::vec3& parValue,
                                             const glm::vec3& parFallback) {
  const float length = glm::length(parValue);
  return std::isfinite(length) && length > 0.0001f ? parValue / length
                                                   : parFallback;
}

[[nodiscard]] glm::quat readQuat(const json& parJson,
                                 const glm::quat& parFallback) {
  if (!parJson.is_array() || parJson.size() != 4) {
    return parFallback;
  }

  const glm::quat value(parJson[0].get<float>(), parJson[1].get<float>(),
                        parJson[2].get<float>(), parJson[3].get<float>());
  const float length = glm::length(value);
  if (!std::isfinite(length) || length <= 0.00001f) {
    return parFallback;
  }
  return glm::normalize(value);
}

[[nodiscard]] kage::math::Transform readTransform(const json& parJson) {
  kage::math::Transform transform;
  if (!parJson.is_object()) {
    return transform;
  }

  transform.translation =
      readVec3(parJson.value("position", json::array()), transform.translation);
  transform.rotation =
      readQuat(parJson.value("rotation", json::array()), transform.rotation);
  transform.scale =
      readVec3(parJson.value("scale", json::array()), transform.scale);
  return transform;
}

[[nodiscard]] json toJson(const kage::math::Transform& parTransform) {
  return {
      {"position", toJson(parTransform.translation)},
      {"rotation", toJson(parTransform.rotation)},
      {"scale", toJson(parTransform.scale)},
  };
}

[[nodiscard]] kage::scene::LightComponent readPointLight(
    const json& parJson) {
  kage::scene::LightComponent light;
  light.type = kage::scene::LightType::Point;
  light.enabled = parJson.value("enabled", true);
  light.color = readVec3(parJson.value("color", json::array()), light.color);
  light.intensity = parJson.value("intensity", light.intensity);
  light.range = parJson.value("range", light.range);
  light.casts_shadows = parJson.value("casts_shadows", true);
  return light;
}

[[nodiscard]] json writeSunJson(
    const kage::scene::SunLightSettings& parSunLight) {
  return {
      {"enabled", parSunLight.enabled},
      {"direction_to_sun", toJson(parSunLight.direction_to_sun)},
      {"color", toJson(parSunLight.color)},
      {"intensity", parSunLight.intensity},
  };
}

[[nodiscard]] kage::scene::SunLightSettings readSunLight(
    const json& parJson) {
  kage::scene::SunLightSettings sun;
  if (!parJson.is_object()) {
    return sun;
  }

  sun.enabled = parJson.value("enabled", sun.enabled);
  if (parJson.contains("direction_to_sun")) {
    sun.direction_to_sun = normalizedOrFallback(
        readVec3(parJson.value("direction_to_sun", json::array()),
                 sun.direction_to_sun),
        sun.direction_to_sun);
  } else if (parJson.contains("direction")) {
    const glm::vec3 incoming_ray =
        readVec3(parJson.value("direction", json::array()),
                 -sun.direction_to_sun);
    sun.direction_to_sun =
        normalizedOrFallback(-incoming_ray, sun.direction_to_sun);
  }
  sun.color = readVec3(parJson.value("color", json::array()), sun.color);
  sun.intensity = parJson.value("intensity", sun.intensity);
  return sun;
}

void readMeshComponent(kage::engine::EngineCore& parEngine,
                       kage::scene::SceneManager::SceneRecord& parScene,
                       kage::scene::EntityId parEntity,
                       const json& parEntityJson) {
  if (!parEntityJson.contains("mesh")) {
    return;
  }

  const json& mesh_json = parEntityJson["mesh"];
  std::size_t asset_index = kage::scene::INVALID_ASSET_LIBRARY_INDEX;
  const kage::assets::AssetId asset_id = readAssetId(mesh_json);
  if (const std::optional<std::size_t> index =
          parEngine.getAssetRegistry().getAssetIndexById(asset_id);
      index.has_value()) {
    asset_index = *index;
  } else {
    asset_index =
        mesh_json.value("asset_index", kage::scene::INVALID_ASSET_LIBRARY_INDEX);
  }

  const auto* asset = parEngine.getAssetRegistry().getAssetLibraryEntry(
      asset_index);
  if (asset == nullptr) {
    return;
  }

  const kage::assets::ModelAsset* document =
      parEngine.getAssetRegistry().getLoadedAsset(asset_index);
  if (document == nullptr) {
    parEngine.requestAssetLoad(asset_index);
  }

  kage::scene::StaticMeshComponent mesh;
  mesh.mesh_handle = asset->mesh_handle;
  mesh.asset_library_index = asset_index;
  mesh.local_bounds =
      document != nullptr ? document->static_model.bounds
                          : kage::math::makeAssetPlaceholderBounds();
  mesh.visible = mesh_json.value("visible", true);
  parScene.world.setStaticMesh(parEntity, mesh);

  if (document != nullptr && !document->skins.empty()) {
    kage::scene::RigComponent rig;
    rig.primitive_skin_matrices.resize(
        document->primitive_skin_bindings.size());
    parScene.world.setRig(parEntity, std::move(rig));
  }
}

[[nodiscard]] kage::render::MaterialDebugMode readMaterialDebugMode(
    const json& parJson,
    kage::render::MaterialDebugMode parFallback) {
  const int raw_value = parJson.is_number_integer()
                            ? parJson.get<int>()
                            : static_cast<int>(parFallback);
  constexpr int MIN_MODE = static_cast<int>(kage::render::MaterialDebugMode::Lit);
  constexpr int MAX_MODE = static_cast<int>(kage::render::MaterialDebugMode::Uv);
  if (raw_value < MIN_MODE || raw_value > MAX_MODE) {
    return kage::render::MaterialDebugMode::Lit;
  }
  return static_cast<kage::render::MaterialDebugMode>(raw_value);
}

[[nodiscard]] kage::film::FilmSequence readFilmSequence(const json& parJson) {
  kage::film::FilmSequence sequence;
  if (!parJson.is_object()) {
    return sequence;
  }
  sequence.name = parJson.value("name", sequence.name);
  sequence.duration_frames =
      std::max(parJson.value("duration_frames", sequence.duration_frames), 1);

  const auto read_cut = [&](const json& cut_json) {
    kage::film::CameraCut cut;
    cut.id = cut_json.value("id", sequence.next_cut_id++);
    cut.start_frame = cut_json.value("start_frame", 0);
    cut.end_frame = cut_json.value("end_frame", cut.start_frame + 1);
    cut.camera.value =
        cut_json.value("camera", kage::scene::EntityId{}.value);
    sequence.next_cut_id = std::max(sequence.next_cut_id, cut.id + 1);
    sequence.camera_cuts.push_back(cut);
  };
  for (const json& cut_json :
       parJson.value("camera_cuts", json::array())) {
    read_cut(cut_json);
  }
  for (const json& track_json : parJson.value("tracks", json::array())) {
    kage::film::FilmTrack track;
    track.target.value =
        track_json.value("target", kage::scene::EntityId{}.value);
    for (const json& clip_json : track_json.value("clips", json::array())) {
      kage::film::FilmClip clip;
      clip.id = clip_json.value("id", sequence.next_clip_id++);
      clip.start_frame = clip_json.value("start_frame", 0);
      clip.end_frame = clip_json.value("end_frame", clip.start_frame + 1);
      const std::string type = clip_json.value("type", "movement");
      if (type == "rig") {
        kage::film::RigAnimation value;
        value.clip_id = clip_json.value(
            "clip_id", kage::assets::AnimationClipId{0});
        value.legacy_clip_index =
            clip_json.value("clip_index", std::size_t{0});
        value.source_in = std::clamp(
            clip_json.value("source_in", 0.0f), 0.0f, 1.0f);
        value.source_out = std::clamp(
            clip_json.value("source_out", 1.0f), value.source_in, 1.0f);
        value.speed = clip_json.value("speed", 1.0f);
        value.looping = clip_json.value("looping", false);
        value.blend_in_seconds = clip_json.value("blend_in", 0.0f);
        value.blend_out_seconds = clip_json.value("blend_out", 0.0f);
        clip.payload = value;
      } else if (type == "property") {
        kage::film::FilmProperty value;
        value.kind = static_cast<kage::film::FilmPropertyKind>(
            clip_json.value("kind", 0));
        value.start_value = readVec4(
            clip_json.value("start_value", json::array()), glm::vec4(0.0f));
        value.control_1 = readVec4(
            clip_json.value("control_1", json::array()), value.start_value);
        value.control_2 = readVec4(
            clip_json.value("control_2", json::array()), value.start_value);
        value.end_value = readVec4(
            clip_json.value("end_value", json::array()), value.start_value);
        clip.payload = value;
      } else {
        kage::film::FilmMovement value;
        value.start = readTransform(clip_json.value("start", json::object()));
        value.end = readTransform(clip_json.value("end", json::object()));
        value.position_control_1 = readVec3(
            clip_json.value("control_1", json::array()), glm::vec3(0.0f));
        value.position_control_2 = readVec3(
            clip_json.value("control_2", json::array()), glm::vec3(0.0f));
        value.timing_control_1 = std::clamp(
            clip_json.value("timing_1", 1.0f / 3.0f), 0.0f, 1.0f);
        value.timing_control_2 = std::clamp(
            clip_json.value("timing_2", 2.0f / 3.0f),
            value.timing_control_1, 1.0f);
        value.automatic_position_controls =
            clip_json.value("automatic_controls", true);
        clip.payload = value;
      }
      sequence.next_clip_id = std::max(sequence.next_clip_id, clip.id + 1);
      track.clips.push_back(std::move(clip));
    }
    sequence.tracks.push_back(std::move(track));
  }

  // Schema-v3 arrays are converted in memory and are never kept as a second
  // runtime representation.
  if (sequence.camera_cuts.empty()) {
    for (const json& shot_json : parJson.value("shots", json::array())) {
      read_cut(shot_json);
    }
  }
  const auto migrate_key_tracks =
      [&](const json& legacy_tracks, const char* target_field,
          bool include_fov) {
        for (const json& track_json : legacy_tracks) {
          kage::scene::EntityId target;
          target.value = track_json.value(
              target_field, kage::scene::EntityId{}.value);
          const json& keys = track_json.value("keys", json::array());
          if (!keys.empty()) {
            const int first_frame = keys.front().value("frame", 0);
            if (first_frame > 0) {
              kage::film::FilmMovement held;
              held.start = held.end = readTransform(
                  keys.front().value("transform", json::object()));
              static_cast<void>(
                  sequence.addClip(target, 0, first_frame, held));
              if (include_fov) {
                kage::film::FilmProperty fov;
                fov.kind = kage::film::FilmPropertyKind::CameraFov;
                fov.start_value = fov.control_1 = fov.control_2 =
                    fov.end_value = glm::vec4(
                        keys.front().value("fov", 50.0f));
                static_cast<void>(
                    sequence.addClip(target, 0, first_frame, fov));
              }
            }
          }
          for (std::size_t index = 0; index < keys.size(); ++index) {
            const json& left = keys[index];
            const json& right = index + 1 < keys.size() ? keys[index + 1]
                                                        : left;
            const int start = left.value("frame", 0);
            const int end = index + 1 < keys.size()
                                ? right.value("frame", start + 1)
                                : sequence.duration_frames;
            if (end <= start) {
              continue;
            }
            kage::film::FilmMovement movement;
            movement.start =
                readTransform(left.value("transform", json::object()));
            movement.end =
                readTransform(right.value("transform", json::object()));
            movement.automatic_position_controls =
                left.value("automatic_handles", true) &&
                right.value("automatic_handles", true);
            movement.position_control_1 = movement.start.translation + readVec3(
                left.value("position_out", json::array()), glm::vec3(0.0f));
            movement.position_control_2 = movement.end.translation + readVec3(
                right.value("position_in", json::array()), glm::vec3(0.0f));
            static_cast<void>(sequence.addClip(target, start, end, movement));
            if (include_fov) {
              kage::film::FilmProperty fov;
              fov.kind = kage::film::FilmPropertyKind::CameraFov;
              fov.start_value.x = left.value("fov", 50.0f);
              fov.end_value.x = right.value("fov", fov.start_value.x);
              const float delta = fov.end_value.x - fov.start_value.x;
              fov.control_1.x = fov.start_value.x + delta / 3.0f;
              fov.control_2.x = fov.end_value.x - delta / 3.0f;
              static_cast<void>(sequence.addClip(target, start, end, fov));
            }
          }
        }
      };
  if (sequence.tracks.empty()) {
    migrate_key_tracks(parJson.value("camera_tracks", json::array()),
                       "camera", true);
    migrate_key_tracks(parJson.value("transform_tracks", json::array()),
                       "entity", false);
  }
  return sequence;
}

[[nodiscard]] json writeFilmSequenceJson(
    const kage::film::FilmSequence& parSequence) {
  json output = {
      {"name", parSequence.name},
      {"fps", kage::film::FILM_FRAMES_PER_SECOND},
      {"duration_frames", parSequence.duration_frames},
      {"camera_cuts", json::array()},
      {"tracks", json::array()},
  };
  for (const kage::film::CameraCut& cut : parSequence.camera_cuts) {
    output["camera_cuts"].push_back({
        {"id", cut.id},
        {"start_frame", cut.start_frame},
        {"end_frame", cut.end_frame},
        {"camera", cut.camera.value},
    });
  }
  for (const kage::film::FilmTrack& track : parSequence.tracks) {
    json track_json = {
        {"target", track.target.value},
        {"clips", json::array()},
    };
    for (const kage::film::FilmClip& clip : track.clips) {
      json clip_json = {{"id", clip.id},
                        {"start_frame", clip.start_frame},
                        {"end_frame", clip.end_frame}};
      if (const auto* movement =
              std::get_if<kage::film::FilmMovement>(&clip.payload)) {
        clip_json["type"] = "movement";
        clip_json["start"] = toJson(movement->start);
        clip_json["end"] = toJson(movement->end);
        clip_json["control_1"] = toJson(movement->position_control_1);
        clip_json["control_2"] = toJson(movement->position_control_2);
        clip_json["timing_1"] = movement->timing_control_1;
        clip_json["timing_2"] = movement->timing_control_2;
        clip_json["automatic_controls"] =
            movement->automatic_position_controls;
      } else if (const auto* animation =
                     std::get_if<kage::film::RigAnimation>(&clip.payload)) {
        clip_json.update({{"type", "rig"},
                          {"clip_id", animation->clip_id},
                          {"clip_index", animation->legacy_clip_index},
                          {"source_in", animation->source_in},
                          {"source_out", animation->source_out},
                          {"speed", animation->speed},
                          {"looping", animation->looping},
                          {"blend_in", animation->blend_in_seconds},
                          {"blend_out", animation->blend_out_seconds}});
      } else if (const auto* property =
                     std::get_if<kage::film::FilmProperty>(&clip.payload)) {
        clip_json.update({{"type", "property"},
                          {"kind", static_cast<int>(property->kind)},
                          {"start_value", toJson(property->start_value)},
                          {"control_1", toJson(property->control_1)},
                          {"control_2", toJson(property->control_2)},
                          {"end_value", toJson(property->end_value)}});
      }
      track_json["clips"].push_back(std::move(clip_json));
    }
    output["tracks"].push_back(std::move(track_json));
  }
  return output;
}

[[nodiscard]] bool loadJsonFile(const std::filesystem::path& parPath,
                                json& parDocument) {
  if (!std::filesystem::exists(parPath)) {
    return false;
  }

  std::ifstream input(parPath);
  if (!input) {
    return false;
  }

  try {
    input >> parDocument;
  } catch (const json::exception&) {
    return false;
  }
  return parDocument.is_object();
}

void writeJsonAtomically(const std::filesystem::path& parPath,
                         const json& parDocument) {
  const std::filesystem::path temporary = parPath.string() + ".tmp";
  {
    std::ofstream output(temporary, std::ios::trunc);
    output << parDocument.dump(2);
    output.flush();
    if (!output) {
      throw std::runtime_error("Failed to write " + temporary.string());
    }
  }
  std::error_code error;
  std::filesystem::rename(temporary, parPath, error);
  if (error) {
    std::filesystem::remove(parPath, error);
    error.clear();
    std::filesystem::rename(temporary, parPath, error);
  }
  if (error) {
    std::filesystem::remove(temporary);
    throw std::runtime_error("Failed to replace " + parPath.string());
  }
}

[[nodiscard]] bool readSceneJson(
    kage::engine::EngineCore& parEngine,
    kage::scene::SceneManager::SceneRecord& parScene,
    const json& parSceneJson) {
  parScene.sun_light =
      readSunLight(parSceneJson.value("sun", json::object()));
  parScene.film_sequence =
      readFilmSequence(parSceneJson.value("film", json::object()));
  const kage::scene::EntityId legacy_editor_camera{
      parSceneJson.value("editor_camera", kage::scene::EntityId{}.value)};

  for (const json& entity_json :
       parSceneJson.value("entities", json::array())) {
    const kage::scene::EntityId requested_id{
        entity_json.value("id", kage::scene::EntityId{}.value)};
    if (requested_id.isValid() &&
        parScene.world.findEntity(requested_id) != nullptr) {
      return false;
    }

    const kage::scene::EntityId entity = parScene.world.createEntityWithId(
        entity_json.value("name", "Entity"), requested_id);
    kage::scene::EntityRecord* record = parScene.world.findEntity(entity);
    if (record == nullptr) {
      continue;
    }
    record->transform.transform =
        readTransform(entity_json.value("transform", json::object()));
    if (entity == legacy_editor_camera) {
      parScene.world.deleteEntity(entity);
      continue;
    }
    readMeshComponent(parEngine, parScene, entity, entity_json);

    if (entity_json.contains("camera")) {
      const json& camera_json = entity_json["camera"];
      kage::scene::CameraComponent camera;
      camera.active = camera_json.value("active", false);
      camera.vertical_fov_degrees = camera_json.value("fov", 45.0f);
      camera.near_plane = camera_json.value("near", 0.01f);
      camera.far_plane = camera_json.value("far", 100.0f);
      parScene.world.setCamera(entity, camera);
    }
    if (entity_json.contains("light")) {
      parScene.world.setLight(entity, readPointLight(entity_json["light"]));
    }
  }

  return true;
}

[[nodiscard]] json writeSceneJson(
    const kage::scene::SceneManager::SceneRecord& parScene,
    const kage::assets::AssetRegistry& parAssetRegistry) {
  json scene_json;
  scene_json["name"] = parScene.name;
  scene_json["sun"] = writeSunJson(parScene.sun_light);
  scene_json["film"] = writeFilmSequenceJson(parScene.film_sequence);
  scene_json["entities"] = json::array();

  for (const kage::scene::EntityRecord& entity :
       parScene.world.getEntities()) {
    if (!entity.alive) {
      continue;
    }

    json entity_json;
    entity_json["id"] = entity.id.value;
    entity_json["name"] = entity.name.name;
    entity_json["transform"] = toJson(entity.transform.transform);
    if (entity.static_mesh.has_value()) {
      entity_json["mesh"] = {{"visible", entity.static_mesh->visible}};
      const auto* asset = parAssetRegistry.getAssetLibraryEntry(
          entity.static_mesh->asset_library_index);
      if (asset != nullptr) {
        entity_json["mesh"]["asset_id"] = asset->id.value;
      }
    }
    if (entity.camera.has_value()) {
      entity_json["camera"] = {
          {"active", entity.camera->active},
          {"fov", entity.camera->vertical_fov_degrees},
          {"near", entity.camera->near_plane},
          {"far", entity.camera->far_plane},
      };
    }
    if (entity.light.has_value() &&
        entity.light->type == kage::scene::LightType::Point) {
      entity_json["light"] = {
          {"type", "point"},
          {"enabled", entity.light->enabled},
          {"color", toJson(entity.light->color)},
          {"intensity", entity.light->intensity},
          {"range", entity.light->range},
          {"casts_shadows", entity.light->casts_shadows},
      };
    }
    scene_json["entities"].push_back(std::move(entity_json));
  }

  return scene_json;
}

}  // namespace

namespace kage::engine {

bool ProjectSerializer::loadProject(EngineCore& parEngine) {
  json document;
  if (!loadJsonFile(parEngine.m_runtime_paths.getProjectWorldPath(), document)) {
    return false;
  }

  try {
    parEngine.m_scene_manager.clearScenes();
    const int version = document.value("version", 1);
    const json& render_json = document.value("render", json::object());
    parEngine.m_render_settings.scene.sky_preset = static_cast<render::SkyPreset>(
        render_json.value(
            "sky",
            static_cast<int>(parEngine.m_render_settings.scene.sky_preset)));
    const json& environment_json =
        render_json.value("environment", json::object());
    parEngine.m_render_settings.scene.environment.asset_id.value =
        environment_json.value("asset_id", assets::AssetId{}.value);
    const std::filesystem::path legacy_environment_path =
        environment_json.value("path", std::string{});
    if (!parEngine.m_render_settings.scene.environment.asset_id.isValid() &&
        !legacy_environment_path.empty()) {
      const assets::AssetId migrated =
          assets::makeStableAssetId("environment", legacy_environment_path);
      parEngine.m_environment_assets.push_back(
          {migrated, legacy_environment_path.stem().string(),
           legacy_environment_path,
           legacy_environment_path.extension() == ".hdr"});
      parEngine.m_render_settings.scene.environment.asset_id = migrated;
    }
    parEngine.m_render_settings.scene.environment.intensity =
        environment_json.value("intensity", 1.0f);
    parEngine.m_render_settings.scene.environment.yaw_degrees =
        environment_json.value("yaw_degrees", 0.0f);
    parEngine.m_render_settings.scene.environment.visible =
        environment_json.value("visible", true);
    const assets::AssetId environment_id =
        parEngine.m_render_settings.scene.environment.asset_id;
    const auto environment_asset = std::find_if(
        parEngine.m_environment_assets.begin(),
        parEngine.m_environment_assets.end(),
        [environment_id](const assets::EnvironmentAsset& item) {
          return item.id == environment_id;
        });
    parEngine.m_world_renderer.requestEnvironment(
        environment_id,
        environment_asset != parEngine.m_environment_assets.end()
            ? environment_asset->path
            : std::filesystem::path{});
    if (version < 2) {
      parEngine.m_render_settings.viewport.floor_grid_visible =
          render_json.value("floor_grid_visible", true);
      parEngine.m_render_settings.viewport.floor_grid_radius =
          render_json.value("floor_grid_radius", 80);
      parEngine.m_render_settings.viewport.material_debug_mode =
          readMaterialDebugMode(
              render_json.value("material_debug_mode", json{}),
              parEngine.m_render_settings.viewport.material_debug_mode);
      parEngine.m_render_settings.viewport.gizmo_axis_space =
          static_cast<render::GizmoAxisSpace>(render_json.value(
              "gizmo_axis_space",
              static_cast<int>(
                  parEngine.m_render_settings.viewport.gizmo_axis_space)));
    }
    parEngine.m_lighting_state.exposure =
        render_json.value("exposure", parEngine.m_lighting_state.exposure);

    const json& scenes_json = document.value("scenes", json::array());
    for (const json& scene_json : scenes_json) {
      const std::size_t scene_index = parEngine.m_scene_manager.createScene(
          scene_json.value("name", "Scene " +
                                       std::to_string(parEngine.m_scene_manager
                                                          .getScenes()
                                                          .size() +
                                                      1)));
      scene::SceneManager::SceneRecord* scene_record =
          parEngine.m_scene_manager.getScene(scene_index);
      if (scene_record == nullptr) {
        continue;
      }
      if (!readSceneJson(parEngine, *scene_record, scene_json)) {
        return false;
      }
      scene_record->selected_entity = {};

    }
  } catch (const json::exception&) {
    parEngine.m_scene_manager.clearScenes();
    return false;
  }

  if (parEngine.m_scene_manager.getScenes().empty()) {
    return false;
  }

  parEngine.m_scene_manager.setActiveScene(
      document.value("version", 1) < 2
          ? document.value("active_scene", std::size_t{0})
          : std::size_t{0});
  parEngine.rebuildAssetInstanceCounts();
  parEngine.m_project_dirty = false;
  parEngine.m_local_session_autosave_timer_seconds = 0.0f;
  return true;
}

void ProjectSerializer::saveProject(EngineCore& parEngine) {
  const std::filesystem::path save_path =
      parEngine.m_runtime_paths.getProjectWorldPath();
  std::filesystem::create_directories(save_path.parent_path());

  json document;
  document["version"] = 5;
  document["render"] = {
      {"sky", static_cast<int>(parEngine.m_render_settings.scene.sky_preset)},
      {"exposure", parEngine.m_lighting_state.exposure},
      {"environment",
       {{"asset_id",
         parEngine.m_render_settings.scene.environment.asset_id.value},
        {"intensity", parEngine.m_render_settings.scene.environment.intensity},
        {"yaw_degrees",
         parEngine.m_render_settings.scene.environment.yaw_degrees},
        {"visible", parEngine.m_render_settings.scene.environment.visible}}},
  };

  json scenes_json = json::array();
  for (std::size_t scene_index = 0;
       scene_index < parEngine.m_scene_manager.getScenes().size();
       ++scene_index) {
    const scene::SceneManager::SceneRecord* scene =
        parEngine.m_scene_manager.getScene(scene_index);
    if (scene == nullptr) {
      continue;
    }
    if (scene->local_only) {
      continue;
    }

    scenes_json.push_back(writeSceneJson(*scene, parEngine.m_asset_registry));
  }
  document["scenes"] = std::move(scenes_json);

  writeJsonAtomically(save_path, document);
  parEngine.m_project_dirty = false;
  parEngine.m_local_session_autosave_timer_seconds = 0.0f;
}

bool ProjectSerializer::loadLocalSession(EngineCore& parEngine) {
  json document;
  if (!loadJsonFile(parEngine.m_runtime_paths.getLocalSessionPath(), document)) {
    return false;
  }

  try {
    const int version = document.value("version", 1);
    const float fly_speed = document.value(
        "fly_speed", parEngine.m_camera_system.getFlyMoveSpeed());
    parEngine.m_camera_system.setFlyMoveSpeed(fly_speed);
    if (version >= 4 && document.contains("editor_camera")) {
      const json& camera_json = document["editor_camera"];
      const math::Transform transform =
          readTransform(camera_json.value("transform", json::object()));
      const float fov = camera_json.value("fov", 50.0f);
      const float near_plane = camera_json.value("near", 0.03f);
      const float far_plane = camera_json.value("far", 500.0f);
      const json& stored_rotation =
          camera_json.value("transform", json::object())
              .value("rotation", json::array());
      float stored_rotation_length = 0.0f;
      if (stored_rotation.is_array() && stored_rotation.size() == 4) {
        const float w = stored_rotation[0].get<float>();
        const float x = stored_rotation[1].get<float>();
        const float y = stored_rotation[2].get<float>();
        const float z = stored_rotation[3].get<float>();
        stored_rotation_length = std::sqrt(w * w + x * x + y * y + z * z);
      }
      const bool stored_rotation_normalized =
          std::isfinite(stored_rotation_length) &&
          std::abs(stored_rotation_length - 1.0f) <= 0.01f;
      if (stored_rotation_normalized &&
          camera::isValidSessionCamera(transform, fov, near_plane,
                                       far_plane)) {
        camera::Camera& camera = parEngine.m_camera_system.getEditorCamera();
        camera.position = transform.translation;
        camera.orientation = glm::normalize(transform.rotation);
        camera.vertical_fov_degrees = fov;
        camera.near_plane = near_plane;
        camera.far_plane = far_plane;
        parEngine.m_camera_system.syncFlyControllerFromCamera();
      }
    }
    parEngine.m_film_playback.playhead_frame =
        std::max(document.value("film_playhead", 0.0), 0.0);
    parEngine.m_render_settings.viewport.floor_grid_visible = document.value(
        "floor_grid_visible",
        parEngine.m_render_settings.viewport.floor_grid_visible);
    parEngine.m_render_settings.viewport.floor_grid_radius = std::clamp(
        document.value("floor_grid_radius",
                       parEngine.m_render_settings.viewport.floor_grid_radius),
        8, 1000);
    parEngine.m_render_settings.viewport.material_debug_mode =
        readMaterialDebugMode(
            document.value("material_debug_mode", json{}),
            parEngine.m_render_settings.viewport.material_debug_mode);
    const int gizmo_axis_space = document.value(
        "gizmo_axis_space",
        static_cast<int>(
            parEngine.m_render_settings.viewport.gizmo_axis_space));
    if (gizmo_axis_space >= static_cast<int>(render::GizmoAxisSpace::Local) &&
        gizmo_axis_space <= static_cast<int>(render::GizmoAxisSpace::World)) {
      parEngine.m_render_settings.viewport.gizmo_axis_space =
          static_cast<render::GizmoAxisSpace>(gizmo_axis_space);
    }

    const json& local_scenes_json =
        document.value("local_scenes", json::array());
    for (const json& scene_json : local_scenes_json) {
      const std::size_t scene_index = parEngine.m_scene_manager.createScene(
          scene_json.value("name", "Local Test " +
                                       std::to_string(parEngine.m_scene_manager
                                                          .getScenes()
                                                          .size() +
                                                      1)),
          true);
      scene::SceneManager::SceneRecord* scene_record =
          parEngine.m_scene_manager.getScene(scene_index);
      if (scene_record == nullptr) {
        continue;
      }
      if (!readSceneJson(parEngine, *scene_record, scene_json)) {
        return false;
      }
    }

    const std::size_t active_scene =
        document.value("active_scene", parEngine.m_scene_manager
                                           .getActiveSceneIndex());
    if (active_scene < parEngine.m_scene_manager.getScenes().size()) {
      parEngine.m_scene_manager.setActiveScene(active_scene);
    }
    if (version < 4) {
      parEngine.frameWorld();
      parEngine.m_local_session_dirty = true;
    }

    const scene::EntityId selected{
        document.value("selected_entity", scene::EntityId{}.value)};
    if (parEngine.getActiveScene().world.findEntity(selected) != nullptr) {
      parEngine.m_scene_manager.selectEntity(selected);
    } else {
      parEngine.m_scene_manager.selectEntity({});
    }
  } catch (const json::exception&) {
    return false;
  }
  return true;
}

void ProjectSerializer::saveLocalSession(const EngineCore& parEngine) {
  const std::filesystem::path save_path =
      parEngine.m_runtime_paths.getLocalSessionPath();
  std::filesystem::create_directories(save_path.parent_path());

  json document;
  document["version"] = 4;
  document["fly_speed"] = parEngine.m_camera_system.getFlyMoveSpeed();
  const camera::Camera& editor_camera =
      parEngine.m_camera_system.getEditorCamera();
  math::Transform editor_transform;
  editor_transform.translation = editor_camera.position;
  editor_transform.rotation = editor_camera.orientation;
  document["editor_camera"] = {
      {"transform", toJson(editor_transform)},
      {"fov", editor_camera.vertical_fov_degrees},
      {"near", editor_camera.near_plane},
      {"far", editor_camera.far_plane},
  };
  document["film_playhead"] = parEngine.m_film_playback.playhead_frame;
  document["selected_entity"] =
      parEngine.m_scene_manager.getSelectedEntity().value;
  document["active_scene"] = parEngine.m_scene_manager.getActiveSceneIndex();
  document["floor_grid_visible"] =
      parEngine.m_render_settings.viewport.floor_grid_visible;
  document["floor_grid_radius"] =
      parEngine.m_render_settings.viewport.floor_grid_radius;
  document["material_debug_mode"] = static_cast<int>(
      parEngine.m_render_settings.viewport.material_debug_mode);
  document["gizmo_axis_space"] =
      static_cast<int>(parEngine.m_render_settings.viewport.gizmo_axis_space);
  document["local_scenes"] = json::array();

  for (const scene::SceneManager::SceneRecord& scene :
       parEngine.m_scene_manager.getScenes()) {
    if (!scene.local_only) {
      continue;
    }

    document["local_scenes"].push_back(
        writeSceneJson(scene, parEngine.m_asset_registry));
  }

  writeJsonAtomically(save_path, document);
}

}  // namespace kage::engine
