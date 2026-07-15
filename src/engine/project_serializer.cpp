#include "engine/project_serializer.hpp"

#include "engine/engine_core.hpp"
#include "camera/session_camera.hpp"
#include "film/movie_timeline_serializer.hpp"
#include "serialization/transform_json.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace {

using json = nlohmann::json;
using kage::serialization::readTransform;
using kage::serialization::readVec3;
using kage::serialization::writeTransform;
using kage::serialization::writeVec3;

[[nodiscard]] kage::assets::AssetId readAssetId(const json& parJson) {
  kage::assets::AssetId id;
  id.value = parJson.value("asset_id", id.value);
  return id;
}

[[nodiscard]] glm::vec3 normalizedOrFallback(const glm::vec3& parValue,
                                             const glm::vec3& parFallback) {
  const float length = glm::length(parValue);
  return std::isfinite(length) && length > 0.0001f ? parValue / length
                                                   : parFallback;
}

[[nodiscard]] kage::scene::LightComponent readPointLight(
    const json& parJson) {
  kage::scene::LightComponent light;
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
      {"direction_to_sun", writeVec3(parSunLight.direction_to_sun)},
      {"color", writeVec3(parSunLight.color)},
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
  const kage::assets::AssetId asset_id = readAssetId(mesh_json);
  const std::optional<std::size_t> asset_index =
      parEngine.getAssetRegistry().getAssetIndexById(asset_id);
  if (!asset_index.has_value()) {
    return;
  }

  const kage::assets::ModelAsset* document =
      parEngine.getAssetRegistry().getLoadedAsset(*asset_index);
  if (document == nullptr) {
    parEngine.requestAssetLoad(*asset_index);
  }

  kage::scene::StaticMeshComponent mesh;
  mesh.asset_library_index = *asset_index;
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
  if (!parJson.is_number_integer()) {
    return parFallback;
  }
  const int raw_value = parJson.get<int>();
  constexpr int MIN_MODE = static_cast<int>(kage::render::MaterialDebugMode::Lit);
  constexpr int MAX_MODE = static_cast<int>(kage::render::MaterialDebugMode::Uv);
  return raw_value < MIN_MODE || raw_value > MAX_MODE
             ? kage::render::MaterialDebugMode::Lit
             : static_cast<kage::render::MaterialDebugMode>(raw_value);
}

enum class JsonFileStatus { Missing, Loaded, Invalid };

[[nodiscard]] JsonFileStatus loadJsonFile(
    const std::filesystem::path& parPath, json& parDocument) {
  std::error_code error;
  if (!std::filesystem::exists(parPath, error)) {
    return error ? JsonFileStatus::Invalid : JsonFileStatus::Missing;
  }
  std::ifstream input(parPath);
  if (!input) {
    return JsonFileStatus::Invalid;
  }

  try {
    input >> parDocument;
  } catch (const json::exception&) {
    return JsonFileStatus::Invalid;
  }
  return parDocument.is_object() ? JsonFileStatus::Loaded
                                 : JsonFileStatus::Invalid;
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
    readMeshComponent(parEngine, parScene, entity, entity_json);

    if (entity_json.contains("camera")) {
      const json& camera_json = entity_json["camera"];
      kage::scene::CameraComponent camera;
      camera.vertical_fov_degrees = camera_json.value("fov", 45.0f);
      camera.near_plane = camera_json.value("near", 0.01f);
      camera.far_plane = camera_json.value("far", 100.0f);
      parScene.world.setCamera(entity, camera);
    }
    if (entity_json.contains("light")) {
      parScene.world.setLight(entity, readPointLight(entity_json["light"]));
    }
  }

  std::string film_error;
  const auto film = parSceneJson.find("film");
  if (!kage::film::decodeMovieTimeline(
          film == parSceneJson.end() ? std::string_view{} : film->dump(),
          parScene.movie_timeline, film_error)) {
    return false;
  }

  return true;
}

[[nodiscard]] json writeSceneJson(
    const kage::scene::SceneManager::SceneRecord& parScene,
    const kage::assets::AssetRegistry& parAssetRegistry) {
  json scene_json;
  scene_json["name"] = parScene.name;
  scene_json["sun"] = writeSunJson(parScene.sun_light);
  scene_json["film"] =
      json::parse(kage::film::encodeMovieTimeline(parScene.movie_timeline));
  scene_json["entities"] = json::array();

  for (const kage::scene::EntityRecord& entity :
       parScene.world.getEntities()) {
    if (!entity.alive) {
      continue;
    }

    json entity_json;
    entity_json["id"] = entity.id.value;
    entity_json["name"] = entity.name.name;
    entity_json["transform"] = writeTransform(entity.transform.transform);
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
          {"fov", entity.camera->vertical_fov_degrees},
          {"near", entity.camera->near_plane},
          {"far", entity.camera->far_plane},
      };
    }
    if (entity.light.has_value()) {
      entity_json["light"] = {
          {"enabled", entity.light->enabled},
          {"color", writeVec3(entity.light->color)},
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
  const JsonFileStatus file_status =
      loadJsonFile(parEngine.m_runtime_paths.getProjectWorldPath(), document);
  if (file_status == JsonFileStatus::Missing) {
    return false;
  }
  if (file_status == JsonFileStatus::Invalid) {
    throw std::runtime_error("Could not read World");
  }

  try {
    if (document.value("version", 0) != 6) {
      throw std::runtime_error("Unsupported World schema version; expected 6");
    }
    parEngine.m_scene_manager.clearScenes();
    const json& render_json = document.value("render", json::object());
    parEngine.m_render_settings.scene.sky_preset = static_cast<render::SkyPreset>(
        render_json.value(
            "sky",
            static_cast<int>(parEngine.m_render_settings.scene.sky_preset)));
    const json& environment_json =
        render_json.value("environment", json::object());
    parEngine.m_render_settings.scene.environment.asset_id.value =
        environment_json.value("asset_id", assets::AssetId{}.value);
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
        throw std::runtime_error("Could not read World");
      }
      scene_record->selected_entity = {};

    }
  } catch (const json::exception&) {
    parEngine.m_scene_manager.clearScenes();
    throw std::runtime_error("Could not read World");
  }

  if (parEngine.m_scene_manager.getScenes().empty()) {
    throw std::runtime_error("Could not read World");
  }

  parEngine.m_scene_manager.setActiveScene(0);
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
  document["version"] = 6;
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
  const JsonFileStatus file_status =
      loadJsonFile(parEngine.m_runtime_paths.getLocalSessionPath(), document);
  if (file_status == JsonFileStatus::Missing) {
    return false;
  }
  if (file_status == JsonFileStatus::Invalid) {
    std::cerr << "Ignoring Local Session\n";
    return false;
  }

  const camera::Camera default_camera = parEngine.m_camera_system.getEditorCamera();
  const float default_fly_speed = parEngine.m_camera_system.getFlyMoveSpeed();
  const double default_playhead = parEngine.m_film_playback.playhead_frame;
  const auto default_viewport = parEngine.m_render_settings.viewport;
  const std::size_t default_scene_count = parEngine.m_scene_manager.getScenes().size();
  const std::size_t default_active_scene =
      parEngine.m_scene_manager.getActiveSceneIndex();
  const scene::EntityId default_selected = parEngine.m_scene_manager.getSelectedEntity();
  const auto ignore_session = [&]() {
    parEngine.m_camera_system.getEditorCamera() = default_camera;
    parEngine.m_camera_system.syncFlyControllerFromCamera();
    parEngine.m_camera_system.setFlyMoveSpeed(default_fly_speed);
    parEngine.m_film_playback.playhead_frame = default_playhead;
    parEngine.m_render_settings.viewport = default_viewport;
    while (parEngine.m_scene_manager.getScenes().size() > default_scene_count) {
      parEngine.m_scene_manager.deleteScene(
          parEngine.m_scene_manager.getScenes().size() - 1);
    }
    parEngine.m_scene_manager.setActiveScene(default_active_scene);
    parEngine.m_scene_manager.selectEntity(default_selected);
    std::cerr << "Ignoring Local Session\n";
    return false;
  };

  try {
    if (document.value("version", 0) != 4) {
      return ignore_session();
    }
    if (document.contains("editor_camera")) {
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
      if (!std::isfinite(stored_rotation_length) ||
          std::abs(stored_rotation_length - 1.0f) > 0.01f ||
          !camera::isValidSessionCamera(transform, fov, near_plane,
                                        far_plane)) {
        return ignore_session();
      }
      camera::Camera& camera = parEngine.m_camera_system.getEditorCamera();
      camera.position = transform.translation;
      camera.orientation = glm::normalize(transform.rotation);
      camera.vertical_fov_degrees = fov;
      camera.near_plane = near_plane;
      camera.far_plane = far_plane;
      parEngine.m_camera_system.syncFlyControllerFromCamera();
    }
    const float fly_speed = document.value(
        "fly_speed", parEngine.m_camera_system.getFlyMoveSpeed());
    parEngine.m_camera_system.setFlyMoveSpeed(fly_speed);
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
        return ignore_session();
      }
    }

    const std::size_t active_scene =
        document.value("active_scene", parEngine.m_scene_manager
                                           .getActiveSceneIndex());
    const scene::EntityId selected{
        document.value("selected_entity", scene::EntityId{}.value)};
    if (active_scene < parEngine.m_scene_manager.getScenes().size()) {
      parEngine.m_scene_manager.setActiveScene(active_scene);
    }
    if (parEngine.getActiveScene().world.findEntity(selected) != nullptr) {
      parEngine.m_scene_manager.selectEntity(selected);
    } else {
      parEngine.m_scene_manager.selectEntity({});
    }
  } catch (const json::exception&) {
    return ignore_session();
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
      {"transform", writeTransform(editor_transform)},
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
