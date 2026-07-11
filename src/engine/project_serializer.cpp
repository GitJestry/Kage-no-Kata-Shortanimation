#include "engine/project_serializer.hpp"

#include "engine/engine_core.hpp"

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
#include <string>
#include <utility>

namespace {

using json = nlohmann::json;

[[nodiscard]] json toJson(const glm::vec3& parValue) {
  return json::array({parValue.x, parValue.y, parValue.z});
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

[[nodiscard]] kage::math::Bounds3 makePlaceholderAssetBounds() {
  kage::math::Bounds3 bounds;
  bounds.includePoint(glm::vec3(-0.5f, 0.0f, -0.5f));
  bounds.includePoint(glm::vec3(0.5f, 1.0f, 0.5f));
  return bounds;
}

[[nodiscard]] kage::scene::AnimationPlayerComponent readAnimationPlayer(
    const json& parJson);

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
                          : makePlaceholderAssetBounds();
  mesh.opacity = mesh_json.value("opacity", 1.0f);
  mesh.visible = mesh_json.value("visible", true);
  parScene.world.setStaticMesh(parEntity, mesh);

  if (document != nullptr && !document->skins.empty()) {
    kage::scene::RigComponent rig;
    rig.primitive_skin_matrices.resize(document->static_model.primitives.size());
    parScene.world.setRig(parEntity, std::move(rig));
  }
  if (parEntityJson.contains("animation_player")) {
    parScene.world.setAnimationPlayer(
        parEntity, readAnimationPlayer(parEntityJson["animation_player"]));
  } else if (parEntityJson.contains("animation")) {
    parScene.world.setAnimationPlayer(
        parEntity, readAnimationPlayer(parEntityJson["animation"]));
  }
}

[[nodiscard]] kage::scene::AnimationPlayerComponent readAnimationPlayer(
    const json& parJson) {
  kage::scene::AnimationPlayerComponent animation;
  animation.clip_index = parJson.value("clip_index", std::size_t{0});
  animation.blend_clip_index =
      parJson.value("blend_clip_index", std::size_t{0});
  animation.time_seconds = parJson.value("time_seconds", 0.0f);
  animation.blend_time_seconds =
      parJson.value("blend_time_seconds", 0.0f);
  animation.blend_duration_seconds =
      parJson.value("blend_duration_seconds", 0.0f);
  animation.playback_speed = parJson.value("playback_speed", 1.0f);
  animation.playing = parJson.value("playing", false);
  animation.looping = parJson.value("looping", true);
  return animation;
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

[[nodiscard]] json writeAnimationPlayerJson(
    const kage::scene::AnimationPlayerComponent& parAnimation) {
  return {
      {"clip_index", parAnimation.clip_index},
      {"blend_clip_index", parAnimation.blend_clip_index},
      {"time_seconds", parAnimation.time_seconds},
      {"blend_time_seconds", parAnimation.blend_time_seconds},
      {"blend_duration_seconds", parAnimation.blend_duration_seconds},
      {"playback_speed", parAnimation.playback_speed},
      {"playing", parAnimation.playing},
      {"looping", parAnimation.looping},
  };
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

[[nodiscard]] json writeSceneJson(
    const kage::scene::SceneManager::SceneRecord& parScene,
    const kage::assets::AssetRegistry& parAssetRegistry) {
  json scene_json;
  scene_json["name"] = parScene.name;
  scene_json["editor_camera"] = parScene.editor_camera_entity.value;
  scene_json["sun"] = writeSunJson(parScene.sun_light);
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
      entity_json["mesh"] = {
          {"opacity", entity.static_mesh->opacity},
          {"visible", entity.static_mesh->visible},
      };
      const auto* asset = parAssetRegistry.getAssetLibraryEntry(
          entity.static_mesh->asset_library_index);
      if (asset != nullptr) {
        entity_json["mesh"]["asset_id"] = asset->id.value;
      }
    }
    if (entity.animation_player.has_value()) {
      entity_json["animation_player"] =
          writeAnimationPlayerJson(*entity.animation_player);
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
    const json& render_json = document.value("render", json::object());
    parEngine.m_render_settings.sky_preset = static_cast<render::SkyPreset>(
        render_json.value(
            "sky", static_cast<int>(parEngine.m_render_settings.sky_preset)));
    parEngine.m_render_settings.floor_grid_visible =
        render_json.value("floor_grid_visible", true);
    parEngine.m_render_settings.floor_grid_radius =
        render_json.value("floor_grid_radius", 80);
    parEngine.m_render_settings.material_debug_mode = readMaterialDebugMode(
        render_json.value("material_debug_mode", json{}),
        parEngine.m_render_settings.material_debug_mode);
    parEngine.m_render_settings.gizmo_axis_space =
        static_cast<render::GizmoAxisSpace>(render_json.value(
            "gizmo_axis_space",
            static_cast<int>(parEngine.m_render_settings.gizmo_axis_space)));
    lighting::LightingState& lighting =
        parEngine.m_lighting_system.getState();
    lighting.ambient_diffuse =
        readVec3(render_json.value("ambient_diffuse", json::array()),
                 lighting.ambient_diffuse);
    lighting.ambient_specular =
        readVec3(render_json.value("ambient_specular", json::array()),
                 lighting.ambient_specular);
    lighting.exposure = render_json.value("exposure", lighting.exposure);

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
      scene_record->sun_light =
          readSunLight(scene_json.value("sun", json::object()));

      const json& entities_json = scene_json.value("entities", json::array());
      for (const json& entity_json : entities_json) {
        const scene::EntityId entity_id{
            entity_json.value("id", scene::EntityId{}.value)};
        if (entity_id.isValid() &&
            scene_record->world.findEntity(entity_id) != nullptr) {
          return false;
        }
        const scene::EntityId entity = scene_record->world.createEntityWithId(
            entity_json.value("name", "Entity"), entity_id);
        scene::EntityRecord* record = scene_record->world.findEntity(entity);
        if (record == nullptr) {
          continue;
        }

        record->transform.transform =
            readTransform(entity_json.value("transform", json::object()));

        readMeshComponent(parEngine, *scene_record, entity, entity_json);
        if (entity_json.contains("camera")) {
          const json& camera_json = entity_json["camera"];
          scene::CameraComponent camera;
          camera.active = camera_json.value("active", false);
          camera.vertical_fov_degrees = camera_json.value("fov", 45.0f);
          camera.near_plane = camera_json.value("near", 0.01f);
          camera.far_plane = camera_json.value("far", 100.0f);
          scene_record->world.setCamera(entity, camera);
        }
        if (entity_json.contains("light")) {
          const json& light_json = entity_json["light"];
          scene_record->world.setLight(entity, readPointLight(light_json));
        }
      }

      const scene::EntityId editor_camera{
          scene_json.value("editor_camera", scene::EntityId{}.value)};
      if (scene_record->world.findEntity(editor_camera) != nullptr) {
        scene_record->editor_camera_entity = editor_camera;
      }
      scene_record->selected_entity = {};

      if (!scene_record->editor_camera_entity.isValid() ||
          scene_record->world.findEntity(scene_record->editor_camera_entity) ==
              nullptr) {
        parEngine.createDefaultSceneEntities(*scene_record);
      }
    }
  } catch (const json::exception&) {
    parEngine.m_scene_manager.clearScenes();
    return false;
  }

  if (parEngine.m_scene_manager.getScenes().empty()) {
    return false;
  }

  parEngine.m_scene_manager.setActiveScene(
      document.value("active_scene", std::size_t{0}));
  parEngine.syncCameraFromEditorEntity();
  parEngine.rebuildAssetInstanceCounts();
  parEngine.m_project_dirty = false;
  parEngine.m_local_session_autosave_timer_seconds = 0.0f;
  return true;
}

void ProjectSerializer::saveProject(EngineCore& parEngine) {
  parEngine.syncEditorCameraEntity();
  const std::filesystem::path save_path =
      parEngine.m_runtime_paths.getProjectWorldPath();
  std::filesystem::create_directories(save_path.parent_path());

  json document;
  document["version"] = 1;
  document["active_scene"] = parEngine.m_scene_manager.getActiveSceneIndex();
  document["render"] = {
      {"sky", static_cast<int>(parEngine.m_render_settings.sky_preset)},
      {"floor_grid_visible",
       parEngine.m_render_settings.floor_grid_visible},
      {"floor_grid_radius", parEngine.m_render_settings.floor_grid_radius},
      {"material_debug_mode",
       static_cast<int>(parEngine.m_render_settings.material_debug_mode)},
      {"gizmo_axis_space",
       static_cast<int>(parEngine.m_render_settings.gizmo_axis_space)},
      {"ambient_diffuse",
       toJson(parEngine.m_lighting_system.getState().ambient_diffuse)},
      {"ambient_specular",
       toJson(parEngine.m_lighting_system.getState().ambient_specular)},
      {"exposure", parEngine.m_lighting_system.getState().exposure},
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

  std::ofstream output(save_path);
  output << document.dump(2);
  parEngine.m_project_dirty = false;
  parEngine.m_local_session_autosave_timer_seconds = 0.0f;
}

bool ProjectSerializer::loadLocalSession(EngineCore& parEngine) {
  json document;
  if (!loadJsonFile(parEngine.m_runtime_paths.getLocalSessionPath(), document)) {
    return false;
  }

  try {
    const float fly_speed = document.value(
        "fly_speed", parEngine.m_camera_system.getFlyMoveSpeed());
    parEngine.m_camera_system.setFlyMoveSpeed(fly_speed);
    const int viewport_mode = document.value(
        "viewport_mode", static_cast<int>(render::ViewportMode::Material));
    if (viewport_mode >= static_cast<int>(render::ViewportMode::Bounds) &&
        viewport_mode <= static_cast<int>(render::ViewportMode::Final)) {
      parEngine.m_render_settings.viewport.mode =
          static_cast<render::ViewportMode>(viewport_mode);
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
      scene_record->sun_light =
          readSunLight(scene_json.value("sun", json::object()));

      const json& entities_json = scene_json.value("entities", json::array());
      for (const json& entity_json : entities_json) {
        const scene::EntityId entity_id{
            entity_json.value("id", scene::EntityId{}.value)};
        const scene::EntityId entity = scene_record->world.createEntityWithId(
            entity_json.value("name", "Entity"), entity_id);
        scene::EntityRecord* record = scene_record->world.findEntity(entity);
        if (record == nullptr) {
          continue;
        }

        record->transform.transform =
            readTransform(entity_json.value("transform", json::object()));

        readMeshComponent(parEngine, *scene_record, entity, entity_json);
        if (entity_json.contains("camera")) {
          const json& camera_json = entity_json["camera"];
          scene::CameraComponent camera;
          camera.active = camera_json.value("active", false);
          camera.vertical_fov_degrees = camera_json.value("fov", 45.0f);
          camera.near_plane = camera_json.value("near", 0.01f);
          camera.far_plane = camera_json.value("far", 100.0f);
          scene_record->world.setCamera(entity, camera);
        }
        if (entity_json.contains("light")) {
          const json& light_json = entity_json["light"];
          scene_record->world.setLight(entity, readPointLight(light_json));
        }
      }

      const scene::EntityId editor_camera{
          scene_json.value("editor_camera", scene::EntityId{}.value)};
      if (scene_record->world.findEntity(editor_camera) != nullptr) {
        scene_record->editor_camera_entity = editor_camera;
      }
      if (!scene_record->editor_camera_entity.isValid() ||
          scene_record->world.findEntity(scene_record->editor_camera_entity) ==
              nullptr) {
        parEngine.createDefaultSceneEntities(*scene_record);
      }
    }

    const std::size_t active_scene =
        document.value("active_scene", parEngine.m_scene_manager
                                           .getActiveSceneIndex());
    if (active_scene < parEngine.m_scene_manager.getScenes().size()) {
      parEngine.m_scene_manager.setActiveScene(active_scene);
      parEngine.syncCameraFromEditorEntity();
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
  document["version"] = 2;
  document["fly_speed"] = parEngine.m_camera_system.getFlyMoveSpeed();
  document["selected_entity"] =
      parEngine.m_scene_manager.getSelectedEntity().value;
  document["active_scene"] = parEngine.m_scene_manager.getActiveSceneIndex();
  document["viewport_mode"] =
      static_cast<int>(parEngine.m_render_settings.viewport.mode);
  document["local_scenes"] = json::array();

  for (const scene::SceneManager::SceneRecord& scene :
       parEngine.m_scene_manager.getScenes()) {
    if (!scene.local_only) {
      continue;
    }

    document["local_scenes"].push_back(
        writeSceneJson(scene, parEngine.m_asset_registry));
  }

  std::ofstream output(save_path);
  output << document.dump(2);
}

}  // namespace kage::engine
