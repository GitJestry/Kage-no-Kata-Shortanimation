#include "engine/engine_core.hpp"

#include "engine/editor_camera_bridge.hpp"
#include "engine/project_serializer.hpp"
#include "math/screen_projection.hpp"
#include "scene/components.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr glm::vec3 LIGHT_LOCAL_FORWARD{0.0f, 0.0f, -1.0f};
constexpr float DEFAULT_PLACEMENT_DISTANCE = 4.0f;
constexpr float ENTITY_HANDLE_EXTENT = 0.35f;
constexpr float RAY_EPSILON = 0.00001f;
constexpr float AUTOSAVE_INTERVAL_SECONDS = 2.0f;
constexpr float ENTITY_SCREEN_PICK_PADDING = 24.0f;
constexpr float HANDLE_SCREEN_RADIUS = 28.0f;

[[nodiscard]] glm::quat lookRotation(const glm::vec3& parPosition,
                                     const glm::vec3& parTarget) {
  if (glm::length(parTarget - parPosition) <= 0.0001f) {
    return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
  }

  const glm::mat4 view =
      glm::lookAt(parPosition, parTarget, glm::vec3(0.0f, 1.0f, 0.0f));
  return glm::normalize(glm::quat_cast(glm::inverse(view)));
}

[[nodiscard]] kage::math::Bounds3 makePointBounds(const glm::vec3& parPoint,
                                                  float parExtent) {
  kage::math::Bounds3 bounds;
  const glm::vec3 extent(std::max(parExtent, 0.01f));
  bounds.includePoint(parPoint - extent);
  bounds.includePoint(parPoint + extent);
  return bounds;
}

[[nodiscard]] kage::math::Bounds3 transformBounds(
    const kage::math::Bounds3& parBounds, const glm::mat4& parTransform) {
  kage::math::Bounds3 transformed;
  if (!parBounds.is_valid) {
    return transformed;
  }

  const std::array<glm::vec3, 8> corners = {
      glm::vec3(parBounds.min.x, parBounds.min.y, parBounds.min.z),
      glm::vec3(parBounds.max.x, parBounds.min.y, parBounds.min.z),
      glm::vec3(parBounds.min.x, parBounds.max.y, parBounds.min.z),
      glm::vec3(parBounds.max.x, parBounds.max.y, parBounds.min.z),
      glm::vec3(parBounds.min.x, parBounds.min.y, parBounds.max.z),
      glm::vec3(parBounds.max.x, parBounds.min.y, parBounds.max.z),
      glm::vec3(parBounds.min.x, parBounds.max.y, parBounds.max.z),
      glm::vec3(parBounds.max.x, parBounds.max.y, parBounds.max.z),
  };

  for (const glm::vec3& corner : corners) {
    transformed.includePoint(
        glm::vec3(parTransform * glm::vec4(corner, 1.0f)));
  }

  return transformed;
}

[[nodiscard]] bool intersectBounds(
    const kage::engine::EngineCore::CameraRay& parRay,
    const kage::math::Bounds3& parBounds, float& parDistance) {
  if (!parBounds.is_valid) {
    return false;
  }

  float min_distance = 0.0f;
  float max_distance = std::numeric_limits<float>::max();
  for (int axis = 0; axis < 3; ++axis) {
    const float origin = parRay.origin[axis];
    const float direction = parRay.direction[axis];
    const float min_value = parBounds.min[axis];
    const float max_value = parBounds.max[axis];
    if (std::abs(direction) < RAY_EPSILON) {
      if (origin < min_value || origin > max_value) {
        return false;
      }
      continue;
    }

    float near_distance = (min_value - origin) / direction;
    float far_distance = (max_value - origin) / direction;
    if (near_distance > far_distance) {
      std::swap(near_distance, far_distance);
    }
    min_distance = std::max(min_distance, near_distance);
    max_distance = std::min(max_distance, far_distance);
    if (min_distance > max_distance) {
      return false;
    }
  }

  parDistance = min_distance;
  return true;
}

[[nodiscard]] bool intersectSphere(
    const kage::engine::EngineCore::CameraRay& parRay,
    const glm::vec3& parCenter, float parRadius) {
  const glm::vec3 to_center = parCenter - parRay.origin;
  const float projected = glm::dot(to_center, parRay.direction);
  if (projected < 0.0f) {
    return false;
  }

  const glm::vec3 closest = parRay.origin + parRay.direction * projected;
  const glm::vec3 delta = closest - parCenter;
  return glm::dot(delta, delta) <= parRadius * parRadius;
}

[[nodiscard]] kage::math::Bounds3 expandedBounds(
    const kage::math::Bounds3& parBounds, float parPadding) {
  if (!parBounds.is_valid) {
    return parBounds;
  }

  kage::math::Bounds3 bounds;
  bounds.includePoint(parBounds.min - glm::vec3(parPadding));
  bounds.includePoint(parBounds.max + glm::vec3(parPadding));
  return bounds;
}

[[nodiscard]] std::size_t readInstanceSuffix(std::string_view parName,
                                             std::string_view parLabel) {
  if (parName.size() <= parLabel.size() + 1 ||
      parName.substr(0, parLabel.size()) != parLabel ||
      parName[parLabel.size()] != ' ') {
    return 0;
  }

  std::size_t suffix = 0;
  const char* begin = parName.data() + parLabel.size() + 1;
  const char* end = parName.data() + parName.size();
  const std::from_chars_result result = std::from_chars(begin, end, suffix);
  return result.ec == std::errc{} && result.ptr == end ? suffix : 0;
}

}  // namespace

namespace kage::engine {

EngineCore::EngineCore() = default;

std::size_t EngineCore::registerStaticAsset(std::string parLabel,
                                            std::filesystem::path parPath) {
  const std::size_t asset_index = m_asset_registry.registerStaticAsset(
      std::move(parLabel), std::move(parPath));
  const assets::AssetRegistry::AssetLibraryEntry* asset =
      m_asset_registry.getAssetLibraryEntry(asset_index);
  if (asset != nullptr) {
    m_mesh_resource_cache.uploadStaticMesh(asset->mesh_handle,
                                           asset->document.static_model);
  }
  return asset_index;
}

std::size_t EngineCore::registerModelAsset(std::string parLabel,
                                           std::filesystem::path parPath,
                                           assets::ModelAsset parDocument) {
  const std::size_t asset_index = m_asset_registry.registerModelAsset(
      std::move(parLabel), std::move(parPath), std::move(parDocument));
  const assets::AssetRegistry::AssetLibraryEntry* asset =
      m_asset_registry.getAssetLibraryEntry(asset_index);
  if (asset != nullptr) {
    m_mesh_resource_cache.uploadStaticMesh(asset->mesh_handle,
                                           asset->document.static_model);
  }
  return asset_index;
}

void EngineCore::createDefaultProject() {
  m_scene_manager.clearScenes();
  const std::size_t scene_index =
      m_scene_manager.createScene("Kage no Kata World");
  scene::SceneManager::SceneRecord* scene =
      m_scene_manager.getScene(scene_index);
  if (scene == nullptr) {
    return;
  }

  createDefaultSceneEntities(*scene);
  if (m_asset_registry.getAssetLibrary().size() >= 2) {
    const scene::EntityId torii =
        instantiateAssetAt(1, glm::vec3(0.0f), 1.0f);
    instantiateAssetAt(0, glm::vec3(-1.4f, 0.0f, 0.0f), 1.0f);
    frameEntity(torii);
    clearSelection();
  }
  markProjectDirty();
}

bool EngineCore::loadProject() {
  return ProjectSerializer::loadProject(*this);
}

void EngineCore::saveProject() {
  ProjectSerializer::saveProject(*this);
}

bool EngineCore::loadLocalSession() {
  return ProjectSerializer::loadLocalSession(*this);
}

void EngineCore::saveLocalSession() const {
  ProjectSerializer::saveLocalSession(*this);
}

void EngineCore::markProjectDirty() {
  m_project_dirty = true;
}

bool EngineCore::isProjectDirty() const {
  return m_project_dirty;
}

std::filesystem::path EngineCore::getProjectSavePath() const {
  return ProjectSerializer::getProjectPath();
}

std::filesystem::path EngineCore::getLocalSessionSavePath() const {
  return ProjectSerializer::getLocalSessionPath();
}

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

bool EngineCore::deleteScene(std::size_t parSceneIndex) {
  syncEditorCameraEntity();
  const bool deleted = m_scene_manager.deleteScene(parSceneIndex);
  if (deleted) {
    syncCameraFromEditorEntity();
    rebuildAssetInstanceCounts();
    markProjectDirty();
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

  scene::EntityId entity =
      getActiveScene().world.createEntity(
          m_asset_registry.reserveInstanceName(parAssetIndex));
  scene::StaticMeshComponent static_mesh;
  static_mesh.mesh_handle = asset->mesh_handle;
  static_mesh.asset_library_index = parAssetIndex;
  static_mesh.local_bounds = asset->document.static_model.bounds;
  static_mesh.opacity = std::clamp(parAlpha, 0.05f, 1.0f);
  getActiveScene().world.setStaticMesh(entity, static_mesh);
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
    record->transform.transform.rotation = m_camera_system.getCamera().orientation;
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

scene::EntityId EngineCore::createDirectionalLightEntityAt(
    std::string parName, const glm::vec3& parPosition) {
  scene::EntityId entity =
      getActiveScene().world.createEntity(std::move(parName));
  scene::EntityRecord* record = getActiveScene().world.findEntity(entity);
  if (record != nullptr) {
    record->transform.transform.translation = parPosition;
    record->transform.transform.rotation =
        lookRotation(parPosition, glm::vec3(0.0f));
  }

  getActiveScene().world.setDirectionalLight(
      entity, scene::DirectionalLightComponent{});
  if (!getActiveScene().primary_light_entity.isValid()) {
    getActiveScene().primary_light_entity = entity;
  }
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

  getActiveScene().world.setPointLight(entity, scene::PointLightComponent{});
  if (!getActiveScene().primary_light_entity.isValid()) {
    getActiveScene().primary_light_entity = entity;
  }
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
          kage::scene::INVALID_ASSET_LIBRARY_INDEX) {
    m_asset_registry.releaseInstance(entity->static_mesh->asset_library_index);
  }

  const bool deleted = scene_record.world.deleteEntity(parEntity);
  if (!deleted) {
    return false;
  }

  if (parEntity == scene_record.selected_entity) {
    scene_record.selected_entity = {};
  }
  if (parEntity == scene_record.primary_light_entity) {
    scene_record.primary_light_entity = {};
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
  if (world_bounds.is_valid) {
    m_camera_system.focusBounds(world_bounds);
  } else {
    m_camera_system.focusPoint(entity->transform.transform.translation,
                               ENTITY_HANDLE_EXTENT);
  }
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

void EngineCore::resetEditorCameraRoll(scene::EntityId parEntity) {
  if (parEntity != getActiveScene().editor_camera_entity) {
    return;
  }

  m_camera_system.resetFlyCameraRoll();
  syncEditorCameraEntity();
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

void EngineCore::setDirectionalLight(
    scene::EntityId parEntity,
    const scene::DirectionalLightComponent& parLight) {
  scene::EntityRecord* entity = getActiveScene().world.findEntity(parEntity);
  if (entity == nullptr || !entity->directional_light.has_value()) {
    return;
  }

  entity->directional_light = parLight;
  markProjectDirty();
}

void EngineCore::setPointLight(scene::EntityId parEntity,
                               const scene::PointLightComponent& parLight) {
  scene::EntityRecord* entity = getActiveScene().world.findEntity(parEntity);
  if (entity == nullptr || !entity->point_light.has_value()) {
    return;
  }

  entity->point_light = parLight;
  markProjectDirty();
}

void EngineCore::setAmbientLight(const glm::vec3& parColor) {
  m_lighting_system.getState().ambient_color = parColor;
  markProjectDirty();
}

void EngineCore::setPlacementGhost(render::PlacementGhost parGhost) {
  m_placement_ghost = parGhost;
}

void EngineCore::clearPlacementGhost() {
  m_placement_ghost = {};
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

void EngineCore::update(float parDeltaSeconds) {
  m_camera_system.update(parDeltaSeconds);
  syncEditorCameraEntity();
  m_lighting_system.setState(buildLightingState());

  m_local_session_autosave_timer_seconds += parDeltaSeconds;
  if (m_local_session_autosave_timer_seconds >= AUTOSAVE_INTERVAL_SECONDS) {
    saveLocalSession();
    m_local_session_autosave_timer_seconds = 0.0f;
  }
}

void EngineCore::render(const glm::vec2& parViewportSize) {
  m_world_renderer.render(getActiveScene(), m_mesh_resource_cache,
                          m_camera_system,
                          m_lighting_system.getState(), m_placement_ghost,
                          m_render_settings, parViewportSize);
}

scene::World& EngineCore::getWorld() {
  return getActiveScene().world;
}

const scene::World& EngineCore::getWorld() const {
  return getActiveScene().world;
}

camera::CameraSystem& EngineCore::getCameraSystem() {
  return m_camera_system;
}

const camera::CameraSystem& EngineCore::getCameraSystem() const {
  return m_camera_system;
}

lighting::LightingSystem& EngineCore::getLightingSystem() {
  return m_lighting_system;
}

const lighting::LightingSystem& EngineCore::getLightingSystem() const {
  return m_lighting_system;
}

assets::AssetRegistry& EngineCore::getAssetRegistry() {
  return m_asset_registry;
}

const assets::AssetRegistry& EngineCore::getAssetRegistry() const {
  return m_asset_registry;
}

std::span<const assets::AssetRegistry::AssetLibraryEntry>
EngineCore::getAssetLibrary() const {
  return m_asset_registry.getAssetLibrary();
}

std::span<const scene::SceneManager::SceneRecord> EngineCore::getScenes() const {
  return m_scene_manager.getScenes();
}

const assets::StaticModel* EngineCore::getStaticMeshSource(
    assets::AssetRegistry::StaticMeshHandle parHandle) const {
  return m_asset_registry.getStaticMeshSource(parHandle);
}

const assets::AssetRegistry::AssetLibraryEntry*
EngineCore::getAssetLibraryEntry(std::size_t parAssetIndex) const {
  return m_asset_registry.getAssetLibraryEntry(parAssetIndex);
}

const render::PlacementGhost& EngineCore::getPlacementGhost() const {
  return m_placement_ghost;
}

scene::EntityId EngineCore::getSelectedEntity() const {
  return m_scene_manager.getSelectedEntity();
}

scene::EntityId EngineCore::getEditorCameraEntity() const {
  return m_scene_manager.getEditorCameraEntity();
}

scene::EntityId EngineCore::getPrimaryLightEntity() const {
  return m_scene_manager.getPrimaryLightEntity();
}

std::size_t EngineCore::getActiveSceneIndex() const {
  return m_scene_manager.getActiveSceneIndex();
}

render::SkyPreset EngineCore::getSkyPreset() const {
  return m_render_settings.sky_preset;
}

const char* EngineCore::getSkyPresetName() const {
  return render::WorldRenderer::getSkyPresetName(m_render_settings.sky_preset);
}

bool EngineCore::isFloorGridVisible() const {
  return m_render_settings.floor_grid_visible;
}

int EngineCore::getFloorGridRadius() const {
  return m_render_settings.floor_grid_radius;
}

float EngineCore::getEditorViewDistance() const {
  return m_camera_system.getCamera().far_plane;
}

render::MaterialDebugMode EngineCore::getMaterialDebugMode() const {
  return m_render_settings.material_debug_mode;
}

render::GizmoAxisSpace EngineCore::getGizmoAxisSpace() const {
  return m_render_settings.gizmo_axis_space;
}

math::Bounds3 EngineCore::getEntityWorldBounds(scene::EntityId parEntity) const {
  const scene::EntityRecord* entity =
      getActiveScene().world.findEntity(parEntity);
  if (entity == nullptr) {
    return {};
  }

  if (entity->static_mesh.has_value()) {
    return transformBounds(entity->static_mesh->local_bounds,
                           entity->transform.transform.toMatrix());
  }

  return makePointBounds(entity->transform.transform.translation,
                         ENTITY_HANDLE_EXTENT);
}

std::optional<scene::EntityId> EngineCore::pickEntity(
    const glm::vec2& parCursorPixel, const glm::vec2& parViewportSize) const {
  const CameraRay ray = makeCameraRay(parCursorPixel, parViewportSize);
  const camera::Camera& camera = m_camera_system.getCamera();
  const glm::mat4 view_projection =
      camera.getViewProjectionMatrix(parViewportSize);
  float closest_distance = std::numeric_limits<float>::max();
  std::optional<scene::EntityId> closest_entity;
  for (const scene::EntityRecord& entity :
       getActiveScene().world.getEntities()) {
    if (!entity.alive || entity.id == getActiveScene().editor_camera_entity) {
      continue;
    }

    float distance = 0.0f;
    const math::Bounds3 world_bounds = getEntityWorldBounds(entity.id);
    const math::Bounds3 ray_bounds = expandedBounds(
        world_bounds, entity.static_mesh.has_value() ? 0.16f : 0.35f);
    const bool ray_hit = intersectBounds(ray, ray_bounds, distance);
    const math::ScreenBounds screen_bounds =
        math::projectBounds(world_bounds, view_projection, parViewportSize);
    const math::ScreenPoint origin =
        math::projectPoint(entity.transform.transform.translation,
                           view_projection, parViewportSize);
    const glm::vec2 origin_delta = parCursorPixel - origin.pixel;
    const bool origin_hit =
        origin.valid &&
        glm::dot(origin_delta, origin_delta) <=
            HANDLE_SCREEN_RADIUS * HANDLE_SCREEN_RADIUS;
    const bool screen_hit =
        math::containsWithPadding(screen_bounds, parCursorPixel,
                                  entity.static_mesh.has_value()
                                      ? ENTITY_SCREEN_PICK_PADDING
                                      : HANDLE_SCREEN_RADIUS) ||
        origin_hit;

    if (!ray_hit && !screen_hit) {
      continue;
    }

    const glm::vec3 center = world_bounds.is_valid
                                 ? (world_bounds.min + world_bounds.max) * 0.5f
                                 : entity.transform.transform.translation;
    const float candidate_distance =
        ray_hit ? distance : glm::length(center - camera.position);
    if (candidate_distance < closest_distance) {
      closest_distance = candidate_distance;
      closest_entity = entity.id;
    }
  }

  return closest_entity;
}

bool EngineCore::isCursorOverEntityCore(
    scene::EntityId parEntity, const glm::vec2& parCursorPixel,
    const glm::vec2& parViewportSize) const {
  const scene::EntityRecord* entity =
      getActiveScene().world.findEntity(parEntity);
  if (entity == nullptr) {
    return false;
  }

  const math::Bounds3 bounds = getEntityWorldBounds(parEntity);
  const glm::mat4 view_projection =
      m_camera_system.getCamera().getViewProjectionMatrix(parViewportSize);
  const math::ScreenPoint origin =
      math::projectPoint(entity->transform.transform.translation,
                         view_projection, parViewportSize);
  if (origin.valid) {
    const glm::vec2 delta = parCursorPixel - origin.pixel;
    if (glm::dot(delta, delta) <= HANDLE_SCREEN_RADIUS * HANDLE_SCREEN_RADIUS) {
      return true;
    }
  }

  const float core_radius =
      std::max({bounds.getSize().x, bounds.getSize().y, bounds.getSize().z,
                1.0f}) *
      0.055f;
  return intersectSphere(makeCameraRay(parCursorPixel, parViewportSize),
                         entity->transform.transform.translation,
                         std::max(core_radius, 0.12f));
}

glm::vec3 EngineCore::getPlacementPointOnFloor(
    const glm::vec2& parCursorPixel, const glm::vec2& parViewportSize) const {
  const CameraRay ray = makeCameraRay(parCursorPixel, parViewportSize);
  if (std::abs(ray.direction.y) > RAY_EPSILON) {
    const float distance = -ray.origin.y / ray.direction.y;
    if (distance > 0.0f) {
      return ray.origin + ray.direction * distance;
    }
  }

  glm::vec3 fallback = getPointInFrontOfCamera(DEFAULT_PLACEMENT_DISTANCE);
  fallback.y = 0.0f;
  return fallback;
}

glm::vec3 EngineCore::getPointInFrontOfCamera(float parDistance) const {
  const camera::Camera& camera = m_camera_system.getCamera();
  return camera.position + camera.getForward() * std::max(parDistance, 0.1f);
}

glm::vec3 EngineCore::getCameraRayDirection(
    const glm::vec2& parCursorPixel, const glm::vec2& parViewportSize) const {
  return makeCameraRay(parCursorPixel, parViewportSize).direction;
}

scene::SceneManager::SceneRecord& EngineCore::getActiveScene() {
  return m_scene_manager.getActiveScene();
}

const scene::SceneManager::SceneRecord& EngineCore::getActiveScene() const {
  return m_scene_manager.getActiveScene();
}

EngineCore::CameraRay EngineCore::makeCameraRay(
    const glm::vec2& parCursorPixel, const glm::vec2& parViewportSize) const {
  const camera::Camera& camera = m_camera_system.getCamera();
  const glm::vec2 viewport_size = glm::max(parViewportSize, glm::vec2(1.0f));
  const glm::vec2 normalized_device_coordinate{
      (parCursorPixel.x / viewport_size.x) * 2.0f - 1.0f,
      1.0f - (parCursorPixel.y / viewport_size.y) * 2.0f};
  const glm::mat4 inverse_view_projection = glm::inverse(
      camera.getProjectionMatrix(viewport_size) * camera.getViewMatrix());
  glm::vec4 near_point = inverse_view_projection *
                         glm::vec4(normalized_device_coordinate, -1.0f, 1.0f);
  glm::vec4 far_point = inverse_view_projection *
                        glm::vec4(normalized_device_coordinate, 1.0f, 1.0f);
  near_point /= near_point.w;
  far_point /= far_point.w;

  CameraRay ray;
  ray.origin = glm::vec3(near_point);
  ray.direction = glm::normalize(glm::vec3(far_point - near_point));
  return ray;
}

lighting::LightingState EngineCore::buildLightingState() const {
  lighting::LightingState state = m_lighting_system.getState();
  state.point.enabled = false;
  for (const scene::EntityRecord& entity :
       getActiveScene().world.getEntities()) {
    if (!entity.alive || !entity.directional_light.has_value() ||
        !entity.directional_light->enabled) {
      continue;
    }

    state.sun.direction = glm::normalize(
        entity.transform.transform.rotation * LIGHT_LOCAL_FORWARD);
    state.sun.color = entity.directional_light->color;
    state.sun.intensity = entity.directional_light->intensity;
    break;
  }

  for (const scene::EntityRecord& entity :
       getActiveScene().world.getEntities()) {
    if (!entity.alive || !entity.point_light.has_value() ||
        !entity.point_light->enabled) {
      continue;
    }

    state.point.enabled = true;
    state.point.position = entity.transform.transform.translation;
    state.point.color = entity.point_light->color;
    state.point.intensity = entity.point_light->intensity;
    state.point.range = entity.point_light->range;
    break;
  }

  return state;
}

void EngineCore::createDefaultSceneEntities(
    scene::SceneManager::SceneRecord& parScene) {
  parScene.editor_camera_entity = parScene.world.createEntity("Editor Camera");
  scene::EntityRecord* camera_entity =
      parScene.world.findEntity(parScene.editor_camera_entity);
  if (camera_entity != nullptr) {
    camera_entity->transform.transform.translation =
        m_camera_system.getCamera().position;
    camera_entity->transform.transform.rotation =
        m_camera_system.getCamera().orientation;
  }
  scene::CameraComponent camera;
  camera.active = true;
  camera.vertical_fov_degrees =
      m_camera_system.getCamera().vertical_fov_degrees;
  camera.near_plane = m_camera_system.getCamera().near_plane;
  camera.far_plane = m_camera_system.getCamera().far_plane;
  parScene.world.setCamera(parScene.editor_camera_entity, camera);

  parScene.primary_light_entity = parScene.world.createEntity("Point Light");
  scene::EntityRecord* light_entity =
      parScene.world.findEntity(parScene.primary_light_entity);
  if (light_entity != nullptr) {
    light_entity->transform.transform.translation = glm::vec3(3.0f, 4.0f, 3.0f);
  }
  parScene.world.setPointLight(parScene.primary_light_entity,
                               scene::PointLightComponent{});
  parScene.selected_entity = {};
}

void EngineCore::syncEditorCameraEntity() {
  if (m_scene_manager.getScenes().empty()) {
    return;
  }

  EditorCameraBridge::syncEntityFromCamera(getActiveScene(), m_camera_system);
}

void EngineCore::syncCameraFromEditorEntity() {
  if (m_scene_manager.getScenes().empty()) {
    return;
  }

  EditorCameraBridge::syncCameraFromEntity(getActiveScene(), m_camera_system);
}

void EngineCore::rebuildAssetInstanceCounts() {
  const auto assets = m_asset_registry.getAssetLibrary();
  std::vector<std::size_t> instance_counts(assets.size(), 0);
  std::vector<std::size_t> next_instance_numbers(assets.size(), 0);
  for (std::size_t scene_index = 0;
       scene_index < m_scene_manager.getScenes().size(); ++scene_index) {
    const scene::SceneManager::SceneRecord* scene =
        m_scene_manager.getScene(scene_index);
    if (scene == nullptr) {
      continue;
    }

    for (const scene::EntityRecord& entity : scene->world.getEntities()) {
      if (!entity.alive || !entity.static_mesh.has_value() ||
          entity.static_mesh->asset_library_index ==
              kage::scene::INVALID_ASSET_LIBRARY_INDEX) {
        continue;
      }
      const std::size_t asset_index = entity.static_mesh->asset_library_index;
      if (asset_index >= assets.size()) {
        continue;
      }

      ++instance_counts[asset_index];
      next_instance_numbers[asset_index] = std::max(
          next_instance_numbers[asset_index],
          readInstanceSuffix(entity.name.name, assets[asset_index].label));
    }
  }

  for (std::size_t asset_index = 0; asset_index < assets.size();
       ++asset_index) {
    m_asset_registry.setInstanceState(
        asset_index, instance_counts[asset_index],
        std::max(instance_counts[asset_index],
                 next_instance_numbers[asset_index]));
  }
}

}  // namespace kage::engine
