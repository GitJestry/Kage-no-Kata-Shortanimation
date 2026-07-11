#include "engine/engine_core.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace {

constexpr float ENTITY_HANDLE_EXTENT = 0.35f;

[[nodiscard]] kage::math::Bounds3 makePlaceholderAssetBounds() {
  kage::math::Bounds3 bounds;
  bounds.includePoint(glm::vec3(-0.5f, 0.0f, -0.5f));
  bounds.includePoint(glm::vec3(0.5f, 1.0f, 0.5f));
  return bounds;
}

}  // namespace

namespace kage::engine {

std::size_t EngineCore::createScene(std::string parName) {
  syncEditorCameraEntity();
  const std::size_t index = m_scene_manager.createScene(std::move(parName));
  scene::SceneManager::SceneRecord* scene = m_scene_manager.getScene(index);
  if (scene != nullptr) {
    createDefaultSceneEntities(*scene);
  }
  syncCameraFromEditorEntity();
  markProjectDirty();
  return index;
}

std::size_t EngineCore::createLocalScene(std::string parName) {
  syncEditorCameraEntity();
  const std::size_t index =
      m_scene_manager.createScene(std::move(parName), true);
  scene::SceneManager::SceneRecord* scene = m_scene_manager.getScene(index);
  if (scene != nullptr) {
    createDefaultSceneEntities(*scene);
  }
  syncCameraFromEditorEntity();
  saveLocalSession();
  return index;
}

bool EngineCore::deleteScene(std::size_t parSceneIndex) {
  syncEditorCameraEntity();
  const scene::SceneManager::SceneRecord* scene =
      m_scene_manager.getScene(parSceneIndex);
  const bool local_only = scene != nullptr && scene->local_only;
  const bool deleted = m_scene_manager.deleteScene(parSceneIndex);
  if (deleted) {
    syncCameraFromEditorEntity();
    rebuildAssetInstanceCounts();
    if (local_only) {
      saveLocalSession();
    } else {
      markProjectDirty();
    }
  }
  return deleted;
}

void EngineCore::setActiveScene(std::size_t parSceneIndex) {
  syncEditorCameraEntity();
  const std::size_t previous_scene = m_scene_manager.getActiveSceneIndex();
  m_scene_manager.setActiveScene(parSceneIndex);
  syncCameraFromEditorEntity();
  if (previous_scene != m_scene_manager.getActiveSceneIndex()) {
    markProjectDirty();
  }
}

void EngineCore::renameScene(std::size_t parSceneIndex, std::string parName) {
  m_scene_manager.renameScene(parSceneIndex, std::move(parName));
  markProjectDirty();
}

scene::EntityId EngineCore::instantiateAssetAt(std::size_t parAssetIndex,
                                               const glm::vec3& parPosition,
                                               float parAlpha) {
  const assets::AssetRegistry::AssetLibraryEntry* asset =
      m_asset_registry.getAssetLibraryEntry(parAssetIndex);
  if (asset == nullptr) {
    throw std::runtime_error("Asset library index is out of range");
  }
  const assets::ModelAsset* document =
      m_asset_registry.getLoadedAsset(parAssetIndex);
  if (document == nullptr) {
    requestAssetLoad(parAssetIndex);
  }

  scene::EntityId entity = getActiveScene().world.createEntity(
      m_asset_registry.reserveInstanceName(parAssetIndex));
  scene::StaticMeshComponent static_mesh;
  static_mesh.mesh_handle = asset->mesh_handle;
  static_mesh.asset_library_index = parAssetIndex;
  static_mesh.local_bounds =
      document != nullptr ? document->static_model.bounds
                          : makePlaceholderAssetBounds();
  static_mesh.opacity = std::clamp(parAlpha, 0.05f, 1.0f);
  getActiveScene().world.setStaticMesh(entity, static_mesh);
  if (document != nullptr && !document->skins.empty()) {
    scene::RigComponent rig;
    rig.primitive_skin_matrices.resize(
        document->static_model.primitives.size());
    getActiveScene().world.setRig(entity, std::move(rig));
  }
  setEntityPosition(entity, parPosition);
  selectEntity(entity);
  markProjectDirty();
  return entity;
}

scene::EntityId EngineCore::createCameraEntityAt(
    std::string parName, const glm::vec3& parPosition) {
  scene::EntityId entity =
      getActiveScene().world.createEntity(std::move(parName));
  scene::EntityRecord* record = getActiveScene().world.findEntity(entity);
  if (record != nullptr) {
    record->transform.transform.translation = parPosition;
    record->transform.transform.rotation =
        m_camera_system.getCamera().orientation;
  }

  scene::CameraComponent camera;
  camera.active = false;
  camera.vertical_fov_degrees = m_camera_system.getCamera().vertical_fov_degrees;
  camera.near_plane = m_camera_system.getCamera().near_plane;
  camera.far_plane = m_camera_system.getCamera().far_plane;
  getActiveScene().world.setCamera(entity, camera);
  selectEntity(entity);
  markProjectDirty();
  return entity;
}

scene::EntityId EngineCore::createPointLightEntityAt(
    std::string parName, const glm::vec3& parPosition) {
  scene::EntityId entity =
      getActiveScene().world.createEntity(std::move(parName));
  scene::EntityRecord* record = getActiveScene().world.findEntity(entity);
  if (record != nullptr) {
    record->transform.transform.translation = parPosition;
  }

  scene::LightComponent light;
  light.type = scene::LightType::Point;
  getActiveScene().world.setLight(entity, light);
  selectEntity(entity);
  markProjectDirty();
  return entity;
}

bool EngineCore::deleteEntity(scene::EntityId parEntity) {
  scene::SceneManager::SceneRecord& scene_record = getActiveScene();
  if (parEntity == scene_record.editor_camera_entity) {
    return false;
  }

  const scene::EntityRecord* entity = scene_record.world.findEntity(parEntity);
  if (entity != nullptr && entity->static_mesh.has_value() &&
      entity->static_mesh->asset_library_index !=
          scene::INVALID_ASSET_LIBRARY_INDEX) {
    m_asset_registry.releaseInstance(entity->static_mesh->asset_library_index);
  }

  const bool deleted = scene_record.world.deleteEntity(parEntity);
  if (!deleted) {
    return false;
  }

  if (parEntity == scene_record.selected_entity) {
    scene_record.selected_entity = {};
  }
  markProjectDirty();
  return true;
}

void EngineCore::selectEntity(scene::EntityId parEntity) {
  m_scene_manager.selectEntity(parEntity);
}

void EngineCore::clearSelection() {
  selectEntity({});
}

void EngineCore::frameEntity(scene::EntityId parEntity) {
  const scene::EntityRecord* entity =
      getActiveScene().world.findEntity(parEntity);
  if (entity == nullptr) {
    return;
  }

  const math::Bounds3 world_bounds = getEntityWorldBounds(parEntity);
  const float far_plane = m_camera_system.getCamera().far_plane;
  if (world_bounds.is_valid) {
    m_camera_system.frameBounds(world_bounds);
  } else {
    math::Bounds3 point_bounds;
    point_bounds.includePoint(entity->transform.transform.translation -
                              glm::vec3(ENTITY_HANDLE_EXTENT));
    point_bounds.includePoint(entity->transform.transform.translation +
                              glm::vec3(ENTITY_HANDLE_EXTENT));
    m_camera_system.frameBounds(point_bounds);
  }
  m_camera_system.getCamera().far_plane = far_plane;
  syncEditorCameraEntity();
}

void EngineCore::setEntityName(scene::EntityId parEntity,
                               std::string parName) {
  scene::EntityRecord* entity = getActiveScene().world.findEntity(parEntity);
  if (entity == nullptr || parName.empty()) {
    return;
  }

  entity->name.name = std::move(parName);
  markProjectDirty();
}

void EngineCore::setEntityPosition(scene::EntityId parEntity,
                                   const glm::vec3& parPosition) {
  scene::EntityRecord* entity = getActiveScene().world.findEntity(parEntity);
  if (entity == nullptr) {
    return;
  }

  entity->transform.transform.translation = parPosition;
  if (parEntity == getActiveScene().editor_camera_entity) {
    m_camera_system.getCamera().position = parPosition;
  }
  markProjectDirty();
}

void EngineCore::setEntityTransform(scene::EntityId parEntity,
                                    const math::Transform& parTransform) {
  scene::EntityRecord* entity = getActiveScene().world.findEntity(parEntity);
  if (entity == nullptr) {
    return;
  }

  entity->transform.transform = parTransform;
  if (parEntity == getActiveScene().editor_camera_entity) {
    camera::Camera& camera = m_camera_system.getCamera();
    camera.position = parTransform.translation;
    camera.orientation = glm::normalize(parTransform.rotation);
    m_camera_system.syncFlyControllerFromCamera();
  }
  markProjectDirty();
}

void EngineCore::setEntityCamera(scene::EntityId parEntity,
                                 const scene::CameraComponent& parCamera) {
  scene::EntityRecord* entity = getActiveScene().world.findEntity(parEntity);
  if (entity == nullptr || !entity->camera.has_value()) {
    return;
  }

  entity->camera = parCamera;
  if (parEntity == getActiveScene().editor_camera_entity) {
    camera::Camera& camera = m_camera_system.getCamera();
    camera.vertical_fov_degrees = parCamera.vertical_fov_degrees;
    camera.near_plane = parCamera.near_plane;
    camera.far_plane = parCamera.far_plane;
  }
  markProjectDirty();
}

void EngineCore::setStaticMeshVisible(scene::EntityId parEntity,
                                      bool parVisible) {
  scene::EntityRecord* entity = getActiveScene().world.findEntity(parEntity);
  if (entity == nullptr || !entity->static_mesh.has_value()) {
    return;
  }

  entity->static_mesh->visible = parVisible;
  markProjectDirty();
}

void EngineCore::setStaticMeshOpacity(scene::EntityId parEntity,
                                      float parOpacity) {
  scene::EntityRecord* entity = getActiveScene().world.findEntity(parEntity);
  if (entity == nullptr || !entity->static_mesh.has_value()) {
    return;
  }

  entity->static_mesh->opacity = std::clamp(parOpacity, 0.05f, 1.0f);
  markProjectDirty();
}

void EngineCore::setAnimationPlayer(
    scene::EntityId parEntity,
    const scene::AnimationPlayerComponent& parAnimationPlayer) {
  scene::EntityRecord* entity = getActiveScene().world.findEntity(parEntity);
  if (entity == nullptr || !entity->rig.has_value()) {
    return;
  }

  getActiveScene().world.setAnimationPlayer(parEntity, parAnimationPlayer);
  markProjectDirty();
}

void EngineCore::clearAnimationPlayer(scene::EntityId parEntity) {
  scene::EntityRecord* entity = getActiveScene().world.findEntity(parEntity);
  if (entity == nullptr) {
    return;
  }

  getActiveScene().world.clearAnimationPlayer(parEntity);
  markProjectDirty();
}

void EngineCore::setLight(scene::EntityId parEntity,
                          const scene::LightComponent& parLight) {
  scene::EntityRecord* entity = getActiveScene().world.findEntity(parEntity);
  if (entity == nullptr || !entity->light.has_value()) {
    return;
  }

  entity->light = parLight;
  markProjectDirty();
}

void EngineCore::setSunLightSettings(
    const scene::SunLightSettings& parSunLight) {
  scene::SunLightSettings sun_light = parSunLight;
  const float length = glm::length(sun_light.direction_to_sun);
  sun_light.direction_to_sun =
      length > 0.0001f ? sun_light.direction_to_sun / length
                       : glm::vec3(0.35f, 0.85f, 0.45f);
  sun_light.intensity = std::max(sun_light.intensity, 0.0f);
  getActiveScene().sun_light = sun_light;
  markProjectDirty();
}

void EngineCore::setAmbientDiffuse(const glm::vec3& parColor) {
  m_lighting_system.getState().ambient_diffuse = parColor;
  markProjectDirty();
}

void EngineCore::setAmbientSpecular(const glm::vec3& parColor) {
  m_lighting_system.getState().ambient_specular = parColor;
  markProjectDirty();
}

void EngineCore::setExposure(float parExposure) {
  m_lighting_system.getState().exposure = std::max(parExposure, 0.0f);
  markProjectDirty();
}

void EngineCore::setPlacementGhost(render::PlacementGhost parGhost) {
  m_placement_ghost = parGhost;
}

void EngineCore::clearPlacementGhost() {
  m_placement_ghost = {};
}

void EngineCore::setGizmoGuide(render::GizmoGuide parGuide) {
  m_gizmo_guide = parGuide;
}

void EngineCore::clearGizmoGuide() {
  m_gizmo_guide = {};
}

void EngineCore::setSkyPreset(render::SkyPreset parPreset) {
  m_render_settings.sky_preset = parPreset;
  markProjectDirty();
}

void EngineCore::setFloorGridVisible(bool parVisible) {
  m_render_settings.floor_grid_visible = parVisible;
  markProjectDirty();
}

void EngineCore::setFloorGridRadius(int parRadius) {
  m_render_settings.floor_grid_radius = std::clamp(parRadius, 8, 1000);
  markProjectDirty();
}

void EngineCore::setEditorViewDistance(float parFarPlane) {
  const float far_plane = std::clamp(parFarPlane, 5.0f, 5000.0f);
  m_camera_system.getCamera().far_plane = far_plane;
  scene::EntityRecord* entity =
      getActiveScene().world.findEntity(getActiveScene().editor_camera_entity);
  if (entity != nullptr && entity->camera.has_value()) {
    entity->camera->far_plane = far_plane;
  }
  markProjectDirty();
}

void EngineCore::setMaterialDebugMode(render::MaterialDebugMode parMode) {
  m_render_settings.material_debug_mode = parMode;
  markProjectDirty();
}

void EngineCore::setGizmoAxisSpace(render::GizmoAxisSpace parAxisSpace) {
  if (m_render_settings.gizmo_axis_space == parAxisSpace) {
    return;
  }
  m_render_settings.gizmo_axis_space = parAxisSpace;
  markProjectDirty();
}

void EngineCore::setViewportMode(render::ViewportMode parMode) {
  if (m_render_settings.viewport.mode == parMode) {
    return;
  }
  m_render_settings.viewport.mode = parMode;
  saveLocalSession();
}

}  // namespace kage::engine
