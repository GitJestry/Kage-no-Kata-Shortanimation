#include "engine/engine_core.hpp"

#include "engine/film_camera_creation.hpp"
#include "render/viewport_picking.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace {

template <typename Edit>
void editEntity(kage::engine::EngineCore& parEngine, kage::scene::EntityId parEntity,
                Edit&& parEdit) {
  kage::scene::EntityRecord* entity = parEngine.getWorld().findEntity(parEntity);
  if (entity != nullptr && parEdit(*entity)) {
    parEngine.markProjectDirty();
  }
}

} // namespace

namespace kage::engine {

std::size_t EngineCore::createScene(std::string parName) {
  const std::size_t index = m_scene_manager.createScene(std::move(parName));
  markProjectDirty();
  return index;
}

bool EngineCore::deleteScene(std::size_t parSceneIndex) {
  const bool deleted = m_scene_manager.deleteScene(parSceneIndex);
  if (deleted) {
    rebuildAssetInstanceCounts();
    markProjectDirty();
  }
  return deleted;
}

void EngineCore::setActiveScene(std::size_t parSceneIndex) {
  const std::size_t previous_scene = m_scene_manager.getActiveSceneIndex();
  m_scene_manager.setActiveScene(parSceneIndex);
  if (previous_scene != m_scene_manager.getActiveSceneIndex()) {
    m_local_session_dirty = true;
  }
}

void EngineCore::renameScene(std::size_t parSceneIndex, std::string parName) {
  m_scene_manager.renameScene(parSceneIndex, std::move(parName));
  m_project_dirty = true;
}

scene::EntityId EngineCore::instantiateAssetAt(std::size_t parAssetIndex,
                                               const glm::vec3& parPosition) {
  const assets::AssetRegistry::AssetLibraryEntry* asset =
      m_asset_registry.getAssetLibraryEntry(parAssetIndex);
  if (asset == nullptr) {
    throw std::runtime_error("Asset library index is out of range");
  }
  const assets::ModelAsset* document = m_asset_registry.getLoadedAsset(parAssetIndex);
  if (document == nullptr) {
    requestAssetLoad(parAssetIndex);
  }

  scene::World& world = getActiveScene().world;
  scene::EntityId entity = world.createEntity(m_asset_registry.reserveInstanceName(parAssetIndex));
  scene::StaticMeshComponent static_mesh{parAssetIndex, document != nullptr
                                                            ? document->static_model.bounds
                                                            : math::makeAssetPlaceholderBounds()};
  world.setStaticMesh(entity, static_mesh);
  if (document != nullptr && !document->skins.empty()) {
    scene::RigComponent rig;
    rig.primitive_skin_matrices.resize(document->primitive_skin_bindings.size());
    world.setRig(entity, std::move(rig));
  }
  setEntityPosition(entity, parPosition);
  selectEntity(entity);
  markProjectDirty();
  return entity;
}

scene::EntityId EngineCore::createCameraEntityAt(std::string parName,
                                                 const glm::vec3& parPosition) {
  scene::EntityId entity = getActiveScene().world.createEntity(std::move(parName));
  scene::EntityRecord* record = getActiveScene().world.findEntity(entity);
  if (record != nullptr) {
    record->transform.transform.translation = parPosition;
    record->transform.transform.rotation = m_camera_system.getEditorCamera().orientation;
  }

  const camera::Camera& editor_camera = m_camera_system.getEditorCamera();
  const scene::CameraComponent camera{editor_camera.vertical_fov_degrees, editor_camera.near_plane,
                                      editor_camera.far_plane};
  getActiveScene().world.setCamera(entity, camera);
  selectEntity(entity);
  markProjectDirty();
  return entity;
}

scene::EntityId EngineCore::createPointLightEntityAt(std::string parName,
                                                     const glm::vec3& parPosition) {
  scene::EntityId entity = getActiveScene().world.createEntity(std::move(parName));
  scene::EntityRecord* record = getActiveScene().world.findEntity(entity);
  if (record != nullptr) {
    record->transform.transform.translation = parPosition;
  }

  getActiveScene().world.setLight(entity, {});
  selectEntity(entity);
  markProjectDirty();
  return entity;
}

std::expected<EngineCore::CreateFilmCameraResult, std::string>
EngineCore::createFilmCameraFromView(film::FilmFrame parStartFrame) {
  const camera::Camera& editor_camera = m_camera_system.getEditorCamera();
  math::Transform transform;
  transform.translation = editor_camera.position;
  transform.rotation = editor_camera.orientation;
  const film::CapturedCameraState camera{editor_camera.vertical_fov_degrees,
                                         editor_camera.near_plane, editor_camera.far_plane};
  const auto created =
      createFilmCameraAtomically(getActiveScene(), transform, camera, parStartFrame);
  if (!created.has_value()) {
    return std::unexpected(created.error());
  }
  markProjectDirty();
  return CreateFilmCameraResult{created->entity, created->sequence_id, created->instance_id};
}

bool EngineCore::deleteEntity(scene::EntityId parEntity) {
  scene::SceneManager::SceneRecord& scene_record = getActiveScene();
  const scene::EntityRecord* entity = scene_record.world.findEntity(parEntity);
  if (entity != nullptr && entity->static_mesh.has_value() &&
      entity->static_mesh->asset_library_index != scene::INVALID_ASSET_LIBRARY_INDEX) {
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
  m_local_session_dirty = true;
}

void EngineCore::clearSelection() {
  selectEntity({});
}

void EngineCore::frameEntity(scene::EntityId parEntity) {
  const scene::EntityRecord* entity = getActiveScene().world.findEntity(parEntity);
  if (entity == nullptr) {
    return;
  }

  const math::Bounds3 world_bounds = getEntityWorldBounds(parEntity);
  const float far_plane = m_camera_system.getEditorCamera().far_plane;
  if (world_bounds.is_valid) {
    m_camera_system.frameBounds(world_bounds);
  } else {
    math::Bounds3 point_bounds;
    point_bounds.includePoint(entity->transform.transform.translation -
                              glm::vec3(render::VIEWPORT_ENTITY_HANDLE_EXTENT));
    point_bounds.includePoint(entity->transform.transform.translation +
                              glm::vec3(render::VIEWPORT_ENTITY_HANDLE_EXTENT));
    m_camera_system.frameBounds(point_bounds);
  }
  m_camera_system.getEditorCamera().far_plane = far_plane;
}

void EngineCore::resetEditorCamera() {
  camera::Camera& camera = m_camera_system.getEditorCamera();
  camera = {};
  camera.lookAt(glm::vec3(0.0f, 0.7f, 0.0f));
  m_camera_system.setFlyMoveSpeed(5.0f);
  m_camera_system.syncFlyControllerFromCamera();
  markLocalSessionDirty();
}

void EngineCore::frameWorld() {
  math::Bounds3 bounds;
  for (const scene::EntityRecord& entity : getActiveScene().world.getEntities()) {
    if (!entity.alive || !entity.static_mesh.has_value() || !entity.static_mesh->visible) {
      continue;
    }
    const math::Bounds3 entity_bounds = getEntityWorldBounds(entity.id);
    if (entity_bounds.is_valid) {
      bounds.includePoint(entity_bounds.min);
      bounds.includePoint(entity_bounds.max);
    }
  }
  if (bounds.is_valid) {
    m_camera_system.frameBounds(bounds);
    markLocalSessionDirty();
  } else {
    resetEditorCamera();
  }
}

void EngineCore::setEntityName(scene::EntityId parEntity, std::string parName) {
  editEntity(*this, parEntity, [&](scene::EntityRecord& entity) {
    if (parName.empty())
      return false;
    entity.name.name = std::move(parName);
    return true;
  });
}

void EngineCore::setEntityPosition(scene::EntityId parEntity, const glm::vec3& parPosition) {
  editEntity(*this, parEntity, [&](scene::EntityRecord& entity) {
    entity.transform.transform.translation = parPosition;
    return true;
  });
}

void EngineCore::setEntityTransform(scene::EntityId parEntity,
                                    const math::Transform& parTransform) {
  editEntity(*this, parEntity, [&](scene::EntityRecord& entity) {
    entity.transform.transform = parTransform;
    return true;
  });
}

void EngineCore::setEntityCamera(scene::EntityId parEntity,
                                 const scene::CameraComponent& parCamera) {
  editEntity(*this, parEntity, [&](scene::EntityRecord& entity) {
    if (!entity.camera)
      return false;
    entity.camera = parCamera;
    return true;
  });
}

void EngineCore::setStaticMeshVisible(scene::EntityId parEntity, bool parVisible) {
  editEntity(*this, parEntity, [&](scene::EntityRecord& entity) {
    if (!entity.static_mesh)
      return false;
    entity.static_mesh->visible = parVisible;
    return true;
  });
}

void EngineCore::setLight(scene::EntityId parEntity, const scene::LightComponent& parLight) {
  editEntity(*this, parEntity, [&](scene::EntityRecord& entity) {
    if (!entity.light)
      return false;
    entity.light = parLight;
    return true;
  });
}

void EngineCore::setSunLightSettings(const scene::SunLightSettings& parSunLight) {
  scene::SunLightSettings sun_light = parSunLight;
  const float length = glm::length(sun_light.direction_to_sun);
  sun_light.direction_to_sun =
      length > 0.0001f ? sun_light.direction_to_sun / length : glm::vec3(0.35f, 0.85f, 0.45f);
  sun_light.intensity = std::max(sun_light.intensity, 0.0f);
  getActiveScene().sun_light = sun_light;
  markProjectDirty();
}

void EngineCore::setExposure(float parExposure) {
  m_lighting_state.exposure = std::max(parExposure, 0.0f);
  markProjectDirty();
}

void EngineCore::setPlacementGhost(render::PlacementGhost parGhost) {
  m_placement_ghost = parGhost;
}

void EngineCore::clearPlacementGhost() {
  m_placement_ghost = {};
}

void EngineCore::setPaintbrushPreview(const glm::vec3& parPosition, float parRadius,
                                      int parDensity) {
  m_placement_ghost.paintbrush_active = true;
  m_placement_ghost.paintbrush_position = parPosition;
  m_placement_ghost.paintbrush_radius = std::max(parRadius, 0.1f);
  m_placement_ghost.paintbrush_density = std::max(parDensity, 8);
}

void EngineCore::clearPaintbrushPreview() {
  m_placement_ghost.paintbrush_active = false;
}

void EngineCore::setGizmoGuide(render::GizmoGuide parGuide) {
  m_gizmo_guide = parGuide;
}

void EngineCore::clearGizmoGuide() {
  m_gizmo_guide = {};
}

void EngineCore::setSkyPreset(render::SkyPreset parPreset) {
  m_render_settings.scene.sky_preset = parPreset;
  markProjectDirty();
}

void EngineCore::setEnvironmentSettings(render::EnvironmentSettings parSettings) {
  parSettings.intensity = std::max(parSettings.intensity, 0.0f);
  const bool asset_changed = parSettings.asset_id != m_render_settings.scene.environment.asset_id;
  m_render_settings.scene.environment = std::move(parSettings);
  if (asset_changed) {
    const assets::AssetId selected = m_render_settings.scene.environment.asset_id;
    const auto asset = std::find_if(
        m_environment_assets.begin(), m_environment_assets.end(),
        [selected](const assets::EnvironmentAsset& item) { return item.id == selected; });
    m_world_renderer.requestEnvironment(
        selected, asset != m_environment_assets.end() ? asset->path : std::filesystem::path{});
  }
  markProjectDirty();
}

void EngineCore::setFloorGridVisible(bool parVisible) {
  m_render_settings.viewport.floor_grid_visible = parVisible;
  m_local_session_dirty = true;
}

void EngineCore::setFloorGridRadius(int parRadius) {
  m_render_settings.viewport.floor_grid_radius =
      std::clamp(parRadius, render::MIN_FLOOR_GRID_RADIUS, render::MAX_FLOOR_GRID_RADIUS);
  m_local_session_dirty = true;
}

void EngineCore::setEditorViewDistance(float parFarPlane) {
  const float far_plane =
      std::clamp(parFarPlane, render::MIN_EDITOR_VIEW_DISTANCE, render::MAX_EDITOR_VIEW_DISTANCE);
  m_camera_system.getEditorCamera().far_plane = far_plane;
  markLocalSessionDirty();
}

void EngineCore::setMaterialDebugMode(render::MaterialDebugMode parMode) {
  m_render_settings.viewport.material_debug_mode = parMode;
  m_local_session_dirty = true;
}

void EngineCore::setGizmoAxisSpace(render::GizmoAxisSpace parAxisSpace) {
  if (m_render_settings.viewport.gizmo_axis_space == parAxisSpace) {
    return;
  }
  m_render_settings.viewport.gizmo_axis_space = parAxisSpace;
  m_local_session_dirty = true;
}

void EngineCore::setViewportMode(render::ViewportMode parMode) {
  const render::ViewportMode previous = m_render_settings.viewport.mode;
  if (previous == parMode) {
    return;
  }
  m_render_settings.viewport.mode = parMode;
  m_local_session_dirty = true;
}

} // namespace kage::engine
