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
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr float DEFAULT_PLACEMENT_DISTANCE = 4.0f;
constexpr float ENTITY_HANDLE_EXTENT = 0.35f;
constexpr float RAY_EPSILON = 0.00001f;
constexpr float AUTOSAVE_INTERVAL_SECONDS = 2.0f;
constexpr float HANDLE_SCREEN_RADIUS = 28.0f;

using Clock = std::chrono::steady_clock;

[[nodiscard]] float elapsedMilliseconds(Clock::time_point parStart,
                                        Clock::time_point parEnd) {
  return std::chrono::duration<float, std::milli>(parEnd - parStart).count();
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

[[nodiscard]] bool intersectTriangle(
    const kage::engine::EngineCore::CameraRay& parRay, const glm::vec3& parA,
    const glm::vec3& parB, const glm::vec3& parC, float& parDistance) {
  const glm::vec3 edge_ab = parB - parA;
  const glm::vec3 edge_ac = parC - parA;
  const glm::vec3 p = glm::cross(parRay.direction, edge_ac);
  const float determinant = glm::dot(edge_ab, p);
  if (std::abs(determinant) <= RAY_EPSILON) {
    return false;
  }

  const float inverse_determinant = 1.0f / determinant;
  const glm::vec3 origin_to_a = parRay.origin - parA;
  const float u = glm::dot(origin_to_a, p) * inverse_determinant;
  if (u < 0.0f || u > 1.0f) {
    return false;
  }

  const glm::vec3 q = glm::cross(origin_to_a, edge_ab);
  const float v = glm::dot(parRay.direction, q) * inverse_determinant;
  if (v < 0.0f || u + v > 1.0f) {
    return false;
  }

  const float distance = glm::dot(edge_ac, q) * inverse_determinant;
  if (distance <= RAY_EPSILON) {
    return false;
  }

  parDistance = distance;
  return true;
}

[[nodiscard]] std::optional<float> pickMeshTriangles(
    const kage::engine::EngineCore::CameraRay& parRay,
    const kage::assets::StaticModel& parModel,
    const kage::math::Transform& parEntityTransform) {
  std::optional<float> closest_distance;
  const glm::mat4 entity_matrix = parEntityTransform.toMatrix();
  for (const kage::assets::StaticPrimitive& primitive : parModel.primitives) {
    const glm::mat4 primitive_matrix = entity_matrix * primitive.transform;
    for (std::size_t index = 0; index + 2 < primitive.indices.size();
         index += 3) {
      const std::uint32_t i0 = primitive.indices[index];
      const std::uint32_t i1 = primitive.indices[index + 1];
      const std::uint32_t i2 = primitive.indices[index + 2];
      if (i0 >= primitive.vertices.size() || i1 >= primitive.vertices.size() ||
          i2 >= primitive.vertices.size()) {
        continue;
      }

      const glm::vec3 a = glm::vec3(
          primitive_matrix *
          glm::vec4(primitive.vertices[i0].position, 1.0f));
      const glm::vec3 b = glm::vec3(
          primitive_matrix *
          glm::vec4(primitive.vertices[i1].position, 1.0f));
      const glm::vec3 c = glm::vec3(
          primitive_matrix *
          glm::vec4(primitive.vertices[i2].position, 1.0f));
      float distance = 0.0f;
      if (intersectTriangle(parRay, a, b, c, distance) &&
          (!closest_distance.has_value() || distance < *closest_distance)) {
        closest_distance = distance;
      }
    }
  }
  return closest_distance;
}

}  // namespace

namespace kage::engine {

EngineCore::EngineCore() = default;

std::size_t EngineCore::registerStaticAsset(std::string parLabel,
                                            std::filesystem::path parPath) {
  return m_asset_registry.registerStaticAsset(std::move(parLabel),
                                              std::move(parPath));
}

std::size_t EngineCore::registerStaticAsset(
    std::string parLabel, std::filesystem::path parPath,
    std::filesystem::path parSourcePath) {
  return m_asset_registry.registerStaticAsset(std::move(parLabel),
                                              std::move(parPath),
                                              std::move(parSourcePath));
}

std::size_t EngineCore::registerModelAsset(std::string parLabel,
                                           std::filesystem::path parPath,
                                           assets::ModelAsset parDocument) {
  const std::size_t asset_index = m_asset_registry.registerModelAsset(
      std::move(parLabel), std::move(parPath), std::move(parDocument));
  const assets::AssetRegistry::AssetLibraryEntry* asset =
      m_asset_registry.getAssetLibraryEntry(asset_index);
  if (asset != nullptr && asset->document.has_value()) {
    const Clock::time_point upload_begin = Clock::now();
    m_mesh_resource_cache.uploadStaticMesh(asset->mesh_handle,
                                           asset->document->static_model);
    m_frame_timings.gpu_upload_ms =
        elapsedMilliseconds(upload_begin, Clock::now());
  }
  return asset_index;
}

void EngineCore::requestAssetLoad(std::size_t parAssetIndex) {
  m_asset_registry.requestLoad(parAssetIndex);
}

void EngineCore::attachLoadedAssetToInstances(std::size_t parAssetIndex) {
  const assets::AssetRegistry::AssetLibraryEntry* asset =
      m_asset_registry.getAssetLibraryEntry(parAssetIndex);
  const assets::ModelAsset* document =
      m_asset_registry.getLoadedAsset(parAssetIndex);
  if (asset == nullptr || document == nullptr) {
    return;
  }

  for (scene::SceneManager::SceneRecord& scene_record :
       m_scene_manager.getScenes()) {
    for (scene::EntityRecord& entity : scene_record.world.getEntities()) {
      if (!entity.alive || !entity.static_mesh.has_value() ||
          entity.static_mesh->asset_library_index != parAssetIndex) {
        continue;
      }

      entity.static_mesh->mesh_handle = asset->mesh_handle;
      entity.static_mesh->local_bounds = document->static_model.bounds;
      if (!document->skins.empty() && !entity.rig.has_value()) {
        scene::RigComponent rig;
        rig.primitive_skin_matrices.resize(
            document->static_model.primitives.size());
        scene_record.world.setRig(entity.id, std::move(rig));
      }
    }
  }
}

void EngineCore::pollAssetStreaming() {
  m_frame_timings.asset_load_ms = 0.0f;
  m_frame_timings.gpu_upload_ms = 0.0f;
  const auto assets = m_asset_registry.getAssetLibrary();
  for (std::size_t asset_index = 0; asset_index < assets.size();
       ++asset_index) {
    if (!m_asset_registry.pollLoad(asset_index)) {
      continue;
    }

    const assets::AssetRegistry::AssetLibraryEntry* entry =
        m_asset_registry.getAssetLibraryEntry(asset_index);
    const assets::ModelAsset* document =
        m_asset_registry.getLoadedAsset(asset_index);
    if (entry == nullptr || document == nullptr) {
      continue;
    }

    m_frame_timings.asset_load_ms += entry->last_cpu_import_ms;
    if (m_mesh_resource_cache.getStaticMesh(entry->mesh_handle) == nullptr) {
      const Clock::time_point upload_begin = Clock::now();
      m_mesh_resource_cache.uploadStaticMesh(entry->mesh_handle,
                                             document->static_model);
      m_frame_timings.gpu_upload_ms +=
          elapsedMilliseconds(upload_begin, Clock::now());
    }
    attachLoadedAssetToInstances(asset_index);
  }
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
  const auto scenes = m_scene_manager.getScenes();
  const std::size_t active_scene = m_scene_manager.getActiveSceneIndex();
  if (active_scene < scenes.size() && scenes[active_scene].local_only) {
    saveLocalSession();
    return;
  }
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

void EngineCore::update(float parDeltaSeconds) {
  pollAssetStreaming();
  m_camera_system.update(parDeltaSeconds);
  syncEditorCameraEntity();
  const Clock::time_point animation_begin = Clock::now();
  m_animation_system.update(getActiveScene().world, m_asset_registry,
                            parDeltaSeconds);
  m_frame_timings.animation_update_ms =
      elapsedMilliseconds(animation_begin, Clock::now());
  m_lighting_system.setState(buildLightingState());

  m_local_session_autosave_timer_seconds += parDeltaSeconds;
  if (m_local_session_autosave_timer_seconds >= AUTOSAVE_INTERVAL_SECONDS) {
    saveLocalSession();
    m_local_session_autosave_timer_seconds = 0.0f;
  }
}

void EngineCore::render(const glm::vec2& parViewportSize) {
  const Clock::time_point render_begin = Clock::now();
  m_world_renderer.render(getActiveScene(), m_mesh_resource_cache,
                          m_camera_system,
                          m_lighting_system.getState(), m_placement_ghost,
                          m_gizmo_guide, m_render_settings, parViewportSize);
  m_frame_timings.render_ms = elapsedMilliseconds(render_begin, Clock::now());
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

const scene::SunLightSettings& EngineCore::getSunLightSettings() const {
  return getActiveScene().sun_light;
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

const EngineCore::FrameTimings& EngineCore::getFrameTimings() const {
  return m_frame_timings;
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
  float closest_distance = std::numeric_limits<float>::max();
  std::optional<scene::EntityId> closest_entity;
  for (const scene::EntityRecord& entity :
       getActiveScene().world.getEntities()) {
    if (!entity.alive || entity.id == getActiveScene().editor_camera_entity) {
      continue;
    }

    float distance = 0.0f;
    bool hit = false;
    if (entity.static_mesh.has_value() && entity.static_mesh->visible) {
      const math::Bounds3 world_bounds = getEntityWorldBounds(entity.id);
      if (!intersectBounds(ray, world_bounds, distance)) {
        continue;
      }
      const assets::StaticModel* model =
          getStaticMeshSource(entity.static_mesh->mesh_handle);
      if (model == nullptr) {
        continue;
      }
      const std::optional<float> triangle_distance =
          pickMeshTriangles(ray, *model, entity.transform.transform);
      if (!triangle_distance.has_value()) {
        continue;
      }
      distance = *triangle_distance;
      hit = true;
    } else {
      hit = intersectSphere(ray, entity.transform.transform.translation,
                            ENTITY_HANDLE_EXTENT);
      distance =
          glm::length(entity.transform.transform.translation - ray.origin);
    }

    if (!hit) {
      continue;
    }

    const float candidate_distance = distance;
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
  const scene::SunLightSettings& sun = getActiveScene().sun_light;
  state.sun.enabled = sun.enabled && sun.intensity > 0.0f;
  const float sun_direction_length = glm::length(sun.direction_to_sun);
  state.sun.direction_to_light =
      sun_direction_length > 0.0001f
          ? sun.direction_to_sun / sun_direction_length
          : glm::vec3(0.35f, 0.85f, 0.45f);
  state.sun.color = sun.color;
  state.sun.intensity = sun.intensity;
  state.point_light_count = 0;
  for (lighting::PointLight& light : state.point_lights) {
    light.enabled = false;
  }
  for (const scene::EntityRecord& entity :
       getActiveScene().world.getEntities()) {
    if (!entity.alive || !entity.light.has_value() ||
        entity.light->type != scene::LightType::Point ||
        !entity.light->enabled) {
      continue;
    }

    if (state.point_light_count >= state.point_lights.size()) {
      break;
    }

    lighting::PointLight& point =
        state.point_lights[state.point_light_count++];
    point.enabled = true;
    point.position = entity.transform.transform.translation;
    point.color = entity.light->color;
    point.intensity = entity.light->intensity;
    point.range = entity.light->range;
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
  parScene.sun_light = {};
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
