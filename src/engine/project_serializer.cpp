#include "engine/project_serializer.hpp"

#include "engine/engine_core.hpp"

#include <glm/gtc/matrix_transform.hpp>
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
#include <string>
#include <utility>

namespace {

using json = nlohmann::json;

constexpr const char* PROJECT_WORLD_PATH =
    "projects/kage_no_kata_world.kage.json";
constexpr const char* LEGACY_LOCAL_PROJECT_PATH =
    ".kage_local/editor_project.json";
constexpr const char* LOCAL_SESSION_PATH = ".kage_local/editor_session.json";

[[nodiscard]] json toJson(const glm::vec3& parValue) {
  return json::array({parValue.x, parValue.y, parValue.z});
}

[[nodiscard]] json toJson(const glm::quat& parValue) {
  return json::array({parValue.w, parValue.x, parValue.y, parValue.z});
}

[[nodiscard]] glm::vec3 readVec3(const json& parJson,
                                 const glm::vec3& parFallback) {
  if (!parJson.is_array() || parJson.size() != 3) {
    return parFallback;
  }

  return glm::vec3(parJson[0].get<float>(), parJson[1].get<float>(),
                   parJson[2].get<float>());
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

[[nodiscard]] glm::quat lookRotation(const glm::vec3& parPosition,
                                     const glm::vec3& parTarget) {
  if (glm::length(parTarget - parPosition) <= 0.0001f) {
    return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
  }

  const glm::mat4 view =
      glm::lookAt(parPosition, parTarget, glm::vec3(0.0f, 1.0f, 0.0f));
  return glm::normalize(glm::quat_cast(glm::inverse(view)));
}

[[nodiscard]] bool nearlyEqual(float parLeft, float parRight) {
  return std::abs(parLeft - parRight) <= 0.0001f;
}

[[nodiscard]] bool nearlyEqual(const glm::vec3& parLeft,
                               const glm::vec3& parRight) {
  return glm::length(parLeft - parRight) <= 0.0001f;
}

[[nodiscard]] bool nearlyEqual(const glm::quat& parLeft,
                               const glm::quat& parRight) {
  return glm::length(glm::vec4(parLeft.w - parRight.w, parLeft.x - parRight.x,
                               parLeft.y - parRight.y,
                               parLeft.z - parRight.z)) <= 0.0001f;
}

void migrateOldDefaultEditorCamera(
    kage::scene::SceneManager::SceneRecord& parScene) {
  kage::scene::EntityRecord* entity =
      parScene.world.findEntity(parScene.editor_camera_entity);
  if (entity == nullptr || !entity->camera.has_value()) {
    return;
  }

  const bool old_transform =
      nearlyEqual(entity->transform.transform.translation,
                  glm::vec3(0.0f, 1.2f, 4.0f)) &&
      nearlyEqual(entity->transform.transform.rotation,
                  glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
  const bool old_camera = nearlyEqual(entity->camera->vertical_fov_degrees,
                                      45.0f) &&
                          nearlyEqual(entity->camera->near_plane, 0.01f) &&
                          nearlyEqual(entity->camera->far_plane, 100.0f);
  if (!old_transform || !old_camera) {
    return;
  }

  entity->transform.transform.translation = glm::vec3(4.0f, 2.6f, 6.0f);
  entity->transform.transform.rotation =
      lookRotation(entity->transform.transform.translation,
                   glm::vec3(0.0f, 0.7f, 0.0f));
  entity->camera->vertical_fov_degrees = 50.0f;
  entity->camera->near_plane = 0.03f;
  entity->camera->far_plane = 500.0f;
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

[[nodiscard]] bool loadProjectDocument(json& parDocument) {
  const std::filesystem::path project_path =
      std::filesystem::current_path() / PROJECT_WORLD_PATH;
  if (std::filesystem::exists(project_path)) {
    return loadJsonFile(project_path, parDocument);
  }

  return loadJsonFile(std::filesystem::current_path() /
                          LEGACY_LOCAL_PROJECT_PATH,
                      parDocument);
}

}  // namespace

namespace kage::engine {

bool ProjectSerializer::loadProject(EngineCore& parEngine) {
  json document;
  if (!loadProjectDocument(document)) {
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
    parEngine.m_render_settings.material_debug_mode =
        static_cast<render::MaterialDebugMode>(render_json.value(
            "material_debug_mode",
            static_cast<int>(
                parEngine.m_render_settings.material_debug_mode)));
    parEngine.m_render_settings.gizmo_axis_space =
        static_cast<render::GizmoAxisSpace>(render_json.value(
            "gizmo_axis_space",
            static_cast<int>(parEngine.m_render_settings.gizmo_axis_space)));

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

        if (entity_json.contains("mesh")) {
          const json& mesh_json = entity_json["mesh"];
          const std::size_t asset_index = mesh_json.value(
              "asset_index", kage::scene::INVALID_ASSET_LIBRARY_INDEX);
          const auto* asset =
              parEngine.m_asset_registry.getAssetLibraryEntry(asset_index);
          if (asset != nullptr) {
            scene::StaticMeshComponent mesh;
            mesh.mesh_handle = asset->mesh_handle;
            mesh.asset_library_index = asset_index;
            mesh.local_bounds = asset->document.static_model.bounds;
            mesh.opacity = mesh_json.value("opacity", 1.0f);
            mesh.visible = mesh_json.value("visible", true);
            scene_record->world.setStaticMesh(entity, mesh);
          }
        }
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
          scene::DirectionalLightComponent light;
          light.enabled = light_json.value("enabled", true);
          light.color =
              readVec3(light_json.value("color", json::array()), light.color);
          light.intensity = light_json.value("intensity", 1.0f);
          scene_record->world.setDirectionalLight(entity, light);
        }
        if (entity_json.contains("point_light")) {
          const json& light_json = entity_json["point_light"];
          scene::PointLightComponent light;
          light.enabled = light_json.value("enabled", true);
          light.color =
              readVec3(light_json.value("color", json::array()), light.color);
          light.intensity = light_json.value("intensity", light.intensity);
          light.range = light_json.value("range", light.range);
          scene_record->world.setPointLight(entity, light);
        }
      }

      const scene::EntityId editor_camera{
          scene_json.value("editor_camera", scene::EntityId{}.value)};
      const scene::EntityId primary_light{
          scene_json.value("primary_light", scene::EntityId{}.value)};
      if (scene_record->world.findEntity(editor_camera) != nullptr) {
        scene_record->editor_camera_entity = editor_camera;
      }
      if (scene_record->world.findEntity(primary_light) != nullptr) {
        scene_record->primary_light_entity = primary_light;
      }
      scene_record->selected_entity = {};

      if (!scene_record->editor_camera_entity.isValid() ||
          scene_record->world.findEntity(scene_record->editor_camera_entity) ==
              nullptr) {
        parEngine.createDefaultSceneEntities(*scene_record);
      }
      migrateOldDefaultEditorCamera(*scene_record);
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
  const std::filesystem::path save_path = getProjectPath();
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

    json scene_json;
    scene_json["name"] = scene->name;
    scene_json["editor_camera"] = scene->editor_camera_entity.value;
    scene_json["primary_light"] = scene->primary_light_entity.value;
    scene_json["entities"] = json::array();

    for (const scene::EntityRecord& entity : scene->world.getEntities()) {
      if (!entity.alive) {
        continue;
      }

      json entity_json;
      entity_json["id"] = entity.id.value;
      entity_json["name"] = entity.name.name;
      entity_json["transform"] = toJson(entity.transform.transform);
      if (entity.static_mesh.has_value()) {
        entity_json["mesh"] = {
            {"asset_index", entity.static_mesh->asset_library_index},
            {"opacity", entity.static_mesh->opacity},
            {"visible", entity.static_mesh->visible},
        };
      }
      if (entity.camera.has_value()) {
        entity_json["camera"] = {
            {"active", entity.camera->active},
            {"fov", entity.camera->vertical_fov_degrees},
            {"near", entity.camera->near_plane},
            {"far", entity.camera->far_plane},
        };
      }
      if (entity.directional_light.has_value()) {
        entity_json["light"] = {
            {"enabled", entity.directional_light->enabled},
            {"color", toJson(entity.directional_light->color)},
            {"intensity", entity.directional_light->intensity},
        };
      }
      if (entity.point_light.has_value()) {
        entity_json["point_light"] = {
            {"enabled", entity.point_light->enabled},
            {"color", toJson(entity.point_light->color)},
            {"intensity", entity.point_light->intensity},
            {"range", entity.point_light->range},
        };
      }

      scene_json["entities"].push_back(std::move(entity_json));
    }

    scenes_json.push_back(std::move(scene_json));
  }
  document["scenes"] = std::move(scenes_json);

  std::ofstream output(save_path);
  output << document.dump(2);
  parEngine.m_project_dirty = false;
  parEngine.m_local_session_autosave_timer_seconds = 0.0f;
}

bool ProjectSerializer::loadLocalSession(EngineCore& parEngine) {
  json document;
  if (!loadJsonFile(getLocalSessionPath(), document)) {
    return false;
  }

  try {
    const float fly_speed = document.value(
        "fly_speed", parEngine.m_camera_system.getFlyMoveSpeed());
    parEngine.m_camera_system.setFlyMoveSpeed(fly_speed);

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
  const std::filesystem::path save_path = getLocalSessionPath();
  std::filesystem::create_directories(save_path.parent_path());

  json document;
  document["version"] = 1;
  document["fly_speed"] = parEngine.m_camera_system.getFlyMoveSpeed();
  document["selected_entity"] =
      parEngine.m_scene_manager.getSelectedEntity().value;

  std::ofstream output(save_path);
  output << document.dump(2);
}

std::filesystem::path ProjectSerializer::getProjectPath() {
  return std::filesystem::current_path() / PROJECT_WORLD_PATH;
}

std::filesystem::path ProjectSerializer::getLocalSessionPath() {
  return std::filesystem::current_path() / LOCAL_SESSION_PATH;
}

}  // namespace kage::engine
