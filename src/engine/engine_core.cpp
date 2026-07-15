#include "engine/engine_core.hpp"

#include "film/film_exporter.hpp"
#include "film/movie_timeline_world_validation.hpp"

#include "engine/film_viewport.hpp"
#include "engine/project_serializer.hpp"
#include "math/screen_projection.hpp"
#include "render/viewport_picking.hpp"
#include "scene/components.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#if defined(__APPLE__)
#include <malloc/malloc.h>
#endif

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

void releaseTransientAllocatorPages() {
#if defined(__APPLE__)
  static_cast<void>(malloc_zone_pressure_relief(nullptr, 0));
#endif
}

}  // namespace

namespace kage::engine {

EngineCore::EngineCore()
    : m_runtime_paths(platform::RuntimePaths::fromExecutable()) {}

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
    m_performance_snapshot.gpu_upload_ms =
        elapsedMilliseconds(upload_begin, Clock::now());
    m_asset_registry.releaseStaticGeometryPayload(asset_index);
  }
  return asset_index;
}

void EngineCore::requestAssetLoad(std::size_t parAssetIndex) {
  const assets::AssetRegistry::AssetLibraryEntry* entry =
      m_asset_registry.getAssetLibraryEntry(parAssetIndex);
  if (entry == nullptr) {
    return;
  }
  if (m_mesh_resource_cache.getStaticMesh(entry->mesh_handle) != nullptr) {
    return;
  }
  if (entry->load_state == assets::AssetLoadState::Queued ||
      entry->load_state == assets::AssetLoadState::CpuLoading ||
      entry->load_state == assets::AssetLoadState::GpuUploading) {
    return;
  }
  m_asset_registry.requestLoad(parAssetIndex);
  m_asset_registry.beginCpuLoad(parAssetIndex);
  m_asset_streamer.request(parAssetIndex, entry->path,
                           assets::AssetLoadPriority::Visible);
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
            document->primitive_skin_bindings.size());
        scene_record.world.setRig(entity.id, std::move(rig));
      }
    }
  }
}

void EngineCore::pollAssetStreaming() {
  m_performance_snapshot.asset_load_ms = 0.0f;
  m_performance_snapshot.gpu_upload_ms = 0.0f;
  while (std::optional<assets::AssetStreamer::Result> result =
             m_asset_streamer.poll()) {
    const std::size_t asset_index = result->asset_index;
    if (!m_asset_registry.completeCpuLoad(
            asset_index, std::move(result->document), std::move(result->error),
            result->cpu_ms)) {
      continue;
    }

    const assets::AssetRegistry::AssetLibraryEntry* entry =
        m_asset_registry.getAssetLibraryEntry(asset_index);
    const assets::ModelAsset* document =
        m_asset_registry.getLoadedAsset(asset_index);
    if (entry == nullptr || document == nullptr) {
      continue;
    }

    m_performance_snapshot.asset_load_ms += entry->last_cpu_import_ms;
    try {
      if (m_mesh_resource_cache.getStaticMesh(entry->mesh_handle) == nullptr) {
        const Clock::time_point upload_begin = Clock::now();
        m_mesh_resource_cache.uploadStaticMesh(entry->mesh_handle,
                                               document->static_model);
        m_performance_snapshot.gpu_upload_ms +=
            elapsedMilliseconds(upload_begin, Clock::now());
      }
    } catch (const std::exception& error) {
      m_asset_registry.failLoad(asset_index, error.what());
      continue;
    }
    attachLoadedAssetToInstances(asset_index);
    m_asset_registry.completeGpuUpload(asset_index);
    m_asset_registry.releaseStaticGeometryPayload(asset_index);
  }
  const bool streaming_active = m_asset_streamer.getPendingCount() > 0;
  m_performance_snapshot.streaming_work_items =
      m_asset_streamer.getPendingCount();
  if (m_streaming_was_active && !streaming_active) {
    m_allocator_relief_passes_remaining = 3;
    m_allocator_relief_timer_seconds = 0.0f;
  }
  m_streaming_was_active = streaming_active;
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
        instantiateAssetAt(1, glm::vec3(0.0f));
    instantiateAssetAt(0, glm::vec3(-1.4f, 0.0f, 0.0f));
    frameEntity(torii);
    clearSelection();
  }
  const auto film_camera = createFilmCameraFromView(0);
  if (!film_camera.has_value()) {
    throw std::runtime_error(film_camera.error());
  }
  clearSelection();
  markProjectDirty();
}

bool EngineCore::loadProject() {
  return ProjectSerializer::loadProject(*this);
}

void EngineCore::saveProject() {
  ProjectSerializer::saveProject(*this);
}

bool EngineCore::loadLocalSession() {
  const bool loaded = ProjectSerializer::loadLocalSession(*this);
  m_last_session_fly_speed = m_camera_system.getFlyMoveSpeed();
  return loaded;
}

void EngineCore::saveLocalSession() {
  if (!m_local_session_dirty) {
    return;
  }
  ProjectSerializer::saveLocalSession(*this);
  m_local_session_dirty = false;
}

void EngineCore::markProjectDirty() {
  const auto scenes = m_scene_manager.getScenes();
  const std::size_t active_scene = m_scene_manager.getActiveSceneIndex();
  if (active_scene < scenes.size() && scenes[active_scene].local_only) {
    m_local_session_dirty = true;
    return;
  }
  m_project_dirty = true;
}

void EngineCore::markLocalSessionDirty() {
  m_local_session_dirty = true;
}

bool EngineCore::isProjectDirty() const {
  return m_project_dirty;
}

const platform::RuntimePaths& EngineCore::getRuntimePaths() const {
  return m_runtime_paths;
}

bool EngineCore::exportFilmSequence(std::string& parError) {
  const film::TimelineValidation validation = validateMovieTimeline(true);
  if (validation.hasErrors()) {
    const auto error = std::find_if(
        validation.diagnostics.begin(), validation.diagnostics.end(),
        [](const film::TimelineDiagnostic& diagnostic) {
          return diagnostic.severity ==
                 film::TimelineDiagnostic::Severity::Error;
        });
    parError = error != validation.diagnostics.end()
                   ? error->message
                   : "Movie is not valid for final render";
    return false;
  }
  const std::string& name = getActiveScene().movie_timeline.name;
  return m_final_render_job.start(
      getActiveScene().movie_timeline,
      std::filesystem::path("output") / "frames" / name,
      std::filesystem::path("output") / (name + ".mp4"),
      film::findFfmpegExecutable(), parError);
}

film::TimelineValidation EngineCore::validateMovieTimeline(
    bool parForBake) const {
  const scene::SceneManager::SceneRecord& scene = getActiveScene();
  return film::validateMovieTimelineWithWorld(scene.movie_timeline, scene.world,
                                               parForBake, &m_asset_registry);
}

void EngineCore::advanceFilmExport() {
  if (!m_final_render_job.isActive()) {
    return;
  }
  m_final_render_job.advance(
      getActiveScene().movie_timeline,
      [&](int frame, int width, int height,
          unsigned int destination_framebuffer) {
        render(render::ViewportRect{{0, 0}, {width, height}, height},
               true, true, frame, false, destination_framebuffer, 4);
      });
}

void EngineCore::cancelFilmExport() { m_final_render_job.cancel(); }

const film::FinalRenderJob& EngineCore::getFinalRenderJob() const {
  return m_final_render_job;
}

void EngineCore::update(float parDeltaSeconds, bool parMovieWorkspace,
                        film::FilmFrame parPlaybackDuration) {
  pollAssetStreaming();
  if (m_allocator_relief_passes_remaining > 0) {
    m_allocator_relief_timer_seconds += parDeltaSeconds;
    if (m_allocator_relief_timer_seconds >= 1.0f) {
      releaseTransientAllocatorPages();
      --m_allocator_relief_passes_remaining;
      m_allocator_relief_timer_seconds = 0.0f;
    }
  }
  m_camera_system.update(parDeltaSeconds);
  if (parMovieWorkspace) {
    const film::FilmFrame duration =
        parPlaybackDuration >= 0
            ? parPlaybackDuration
            : getActiveScene().movie_timeline.durationFrames();
    m_film_playback.update(parDeltaSeconds, duration);
  } else {
    m_film_playback.playing = false;
    m_film_playback.previewing = false;
  }
  const float fly_speed = m_camera_system.getFlyMoveSpeed();
  if (std::abs(fly_speed - m_last_session_fly_speed) > 0.0001f) {
    m_last_session_fly_speed = fly_speed;
    m_local_session_dirty = true;
  }
  if (!parMovieWorkspace) {
    m_performance_snapshot.animation_update_ms = 0.0f;
  }
  m_lighting_state = buildLightingState(m_camera_system.getEditorCamera());

  m_local_session_autosave_timer_seconds += parDeltaSeconds;
  if (m_local_session_dirty &&
      m_local_session_autosave_timer_seconds >= AUTOSAVE_INTERVAL_SECONDS) {
    saveLocalSession();
    m_local_session_autosave_timer_seconds = 0.0f;
  }
}

void EngineCore::render(const render::ViewportRect& parViewport,
                        bool parMovieWorkspace, bool parShotPreview,
                        double parFilmFrame, bool parShowOverlays,
                        unsigned int parDestinationFramebuffer,
                        int parMsaaSamples,
                        film::TargetSequenceId parPreviewSequenceId,
                        scene::EntityId parMovieSelectionEntity,
                        std::span<const film::ResolvedMovementPath>
                            parMovementPaths) {
  const Clock::time_point render_begin = Clock::now();
  const double film_frame =
      parFilmFrame >= 0.0 ? parFilmFrame : m_film_playback.playhead_frame;
  const film::FilmFrameState* film_state = nullptr;
  const bool consume_film_state = film::requiresFilmFrameState(
      parMovieWorkspace, parShotPreview, parFilmFrame, m_film_playback);
  if (consume_film_state) {
    const auto frame = static_cast<film::FilmFrame>(std::clamp(
        std::floor(film_frame), 0.0,
        static_cast<double>(film::MAX_FILM_FRAMES)));
    if (parPreviewSequenceId != 0) {
      const std::optional<film::FilmFrameState> preview =
          film::evaluateTargetSequencePreview(getActiveScene().movie_timeline,
                                              parPreviewSequenceId, frame);
      m_film_frame_state = preview.value_or(film::FilmFrameState{});
    } else {
      m_film_frame_state =
          film::evaluateMovieTimeline(getActiveScene().movie_timeline, frame);
    }
    film_state = &m_film_frame_state;
    const Clock::time_point animation_begin = Clock::now();
    m_animation_system.evaluateFilmFrame(getActiveScene().world,
                                         m_asset_registry, *film_state,
                                         m_film_skin_palettes);
    m_performance_snapshot.animation_update_ms =
        elapsedMilliseconds(animation_begin, Clock::now());
  } else {
    m_film_skin_palettes.clear();
    m_performance_snapshot.animation_update_ms = 0.0f;
  }
  const film::TargetSequence* preview_sequence =
      getActiveScene().movie_timeline.findSequence(parPreviewSequenceId);
  const bool use_film_camera =
      parPreviewSequenceId == 0 ||
      (preview_sequence != nullptr && film::isCameraSequence(*preview_sequence));
  const FilmViewportCamera viewport_camera = resolveFilmViewportCamera(
      m_camera_system.getEditorCamera(), getActiveScene().world, film_state,
      use_film_camera);
  const bool black_camera_output = viewport_camera.black_output;
  const camera::Camera* view_camera = viewport_camera.camera.has_value()
                                          ? &*viewport_camera.camera
                                          : nullptr;
  if (parDestinationFramebuffer == 0) {
    m_viewport_consumes_film_state = viewport_camera.consumes_film_state;
    m_viewport_black_output = viewport_camera.black_output;
    m_viewport_camera = viewport_camera.camera;
    if (film_state != nullptr) {
      m_viewport_film_frame_state = *film_state;
    }
  }
  const lighting::LightingState frame_lighting = buildLightingState(
      view_camera != nullptr ? *view_camera : m_camera_system.getEditorCamera(),
      film_state);
  const render::ViewportView view{
      view_camera,
      film_state,
      parMovementPaths,
      parMovieSelectionEntity,
      !parMovieWorkspace,
      black_camera_output,
      parDestinationFramebuffer,
      parDestinationFramebuffer != 0,
      consume_film_state
          ? std::span<const animation::EvaluatedSkinPalette>(
                m_film_skin_palettes)
          : std::span<const animation::EvaluatedSkinPalette>{},
      parMsaaSamples,
  };
  render::EditorRenderSettings settings = m_render_settings;
  if (parDestinationFramebuffer != 0) {
    settings.viewport.mode = render::ViewportMode::Final;
    settings.viewport.material_debug_mode = render::MaterialDebugMode::Lit;
  }
  settings.viewport.show_overlays = shouldShowEditorOverlays(
      parShowOverlays, viewport_camera, use_film_camera);
  settings.viewport.show_world_edit_gizmos = !parMovieWorkspace;
  if (!settings.viewport.show_overlays) {
    settings.viewport.floor_grid_visible = false;
  }
  m_world_renderer.render(getActiveScene(), m_mesh_resource_cache,
                          view,
                          frame_lighting, m_placement_ghost,
                          m_gizmo_guide, settings, parViewport,
                          m_performance_snapshot);
  m_performance_snapshot.render_ms =
      elapsedMilliseconds(render_begin, Clock::now());
  m_performance_snapshot.estimated_texture_bytes =
      m_mesh_resource_cache.getEstimatedTextureBytes();
  const float cpu_frame_ms = m_performance_snapshot.animation_update_ms +
                             m_performance_snapshot.render_ms +
                             m_performance_snapshot.gpu_upload_ms;
  m_cpu_frame_samples[m_cpu_frame_sample_cursor] = cpu_frame_ms;
  m_cpu_frame_sample_cursor =
      (m_cpu_frame_sample_cursor + 1) % m_cpu_frame_samples.size();
  m_cpu_frame_sample_count =
      std::min(m_cpu_frame_sample_count + 1, m_cpu_frame_samples.size());
  std::array<float, 120> sorted_samples = m_cpu_frame_samples;
  std::sort(sorted_samples.begin(),
            sorted_samples.begin() + m_cpu_frame_sample_count);
  const float total = std::accumulate(
      sorted_samples.begin(),
      sorted_samples.begin() + m_cpu_frame_sample_count, 0.0f);
  m_performance_snapshot.cpu_average_ms =
      total / static_cast<float>(m_cpu_frame_sample_count);
  const std::size_t p95_index =
      std::min(m_cpu_frame_sample_count - 1,
               (m_cpu_frame_sample_count * 95 + 99) / 100 - 1);
  m_performance_snapshot.cpu_p95_ms = sorted_samples[p95_index];
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

const lighting::LightingState& EngineCore::getLightingState() const {
  return m_lighting_state;
}

assets::AssetRegistry& EngineCore::getAssetRegistry() {
  return m_asset_registry;
}

std::span<const assets::AssetRegistry::AssetLibraryEntry>
EngineCore::getAssetLibrary() const {
  return m_asset_registry.getAssetLibrary();
}

std::span<const assets::EnvironmentAsset>
EngineCore::getEnvironmentAssets() const {
  return m_environment_assets;
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

scene::EntityId EngineCore::getSelectedEntity() const {
  return m_scene_manager.getSelectedEntity();
}

const scene::SunLightSettings& EngineCore::getSunLightSettings() const {
  return getActiveScene().sun_light;
}

std::size_t EngineCore::getActiveSceneIndex() const {
  return m_scene_manager.getActiveSceneIndex();
}

render::SkyPreset EngineCore::getSkyPreset() const {
  return m_render_settings.scene.sky_preset;
}

const render::EnvironmentSettings& EngineCore::getEnvironmentSettings() const {
  return m_render_settings.scene.environment;
}

render::EnvironmentLoadState EngineCore::getEnvironmentLoadState() const {
  return m_world_renderer.getEnvironmentState();
}

const std::string& EngineCore::getEnvironmentError() const {
  return m_world_renderer.getEnvironmentError();
}

bool EngineCore::isFloorGridVisible() const {
  return m_render_settings.viewport.floor_grid_visible;
}

int EngineCore::getFloorGridRadius() const {
  return m_render_settings.viewport.floor_grid_radius;
}

float EngineCore::getEditorViewDistance() const {
  return m_camera_system.getEditorCamera().far_plane;
}

render::MaterialDebugMode EngineCore::getMaterialDebugMode() const {
  return m_render_settings.viewport.material_debug_mode;
}

render::GizmoAxisSpace EngineCore::getGizmoAxisSpace() const {
  return m_render_settings.viewport.gizmo_axis_space;
}

render::ViewportMode EngineCore::getViewportMode() const {
  return m_render_settings.viewport.mode;
}

film::MovieTimeline& EngineCore::getMovieTimeline() {
  return getActiveScene().movie_timeline;
}

film::FilmPlayback& EngineCore::getFilmPlayback() {
  return m_film_playback;
}

void EngineCore::clearFilmPreviewState() {
  m_film_playback.playing = false;
  m_film_playback.previewing = false;
  m_film_frame_state = {};
  m_viewport_film_frame_state = {};
  m_viewport_camera.reset();
  m_viewport_consumes_film_state = false;
  m_viewport_black_output = false;
  m_film_skin_palettes.clear();
}

const render::PerformanceSnapshot& EngineCore::getPerformanceSnapshot() const {
  return m_performance_snapshot;
}

math::Bounds3 EngineCore::getEntityWorldBounds(scene::EntityId parEntity) const {
  const scene::EntityRecord* entity =
      getActiveScene().world.findEntity(parEntity);
  if (entity == nullptr) {
    return {};
  }

  if (entity->static_mesh.has_value()) {
    return math::transformBounds(entity->static_mesh->local_bounds,
                                 entity->transform.transform.toMatrix());
  }

  return math::makePointBounds(entity->transform.transform.translation,
                               ENTITY_HANDLE_EXTENT);
}

std::optional<scene::EntityId> EngineCore::pickEntity(
    const glm::vec2& parCursorPixel, const glm::vec2& parViewportSize) {
  if (m_render_settings.viewport.mode != render::ViewportMode::Bounds) {
    if (std::optional<scene::EntityId> gpu_pick = m_world_renderer.pickEntity(
        getActiveScene(), m_mesh_resource_cache,
        m_camera_system.getEditorCamera(), parCursorPixel, parViewportSize)) {
      return gpu_pick;
    }
  }
  const CameraRay ray = makeCameraRay(parCursorPixel, parViewportSize);
  float closest_distance = std::numeric_limits<float>::max();
  std::optional<scene::EntityId> closest_entity;
  for (const scene::EntityRecord& entity :
       getActiveScene().world.getEntities()) {
    if (!entity.alive) {
      continue;
    }

    float distance = 0.0f;
    bool hit = false;
    if (entity.static_mesh.has_value() && entity.static_mesh->visible) {
      const math::Bounds3 world_bounds = getEntityWorldBounds(entity.id);
      if (!intersectBounds(ray, world_bounds, distance)) {
        continue;
      }
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

std::optional<scene::EntityId> EngineCore::pickMovieEntity(
    const glm::vec2& parCursorPixel, const glm::vec2& parViewportSize) {
  if (!m_viewport_consumes_film_state) {
    return pickEntity(parCursorPixel, parViewportSize);
  }
  if (m_viewport_black_output || !m_viewport_camera.has_value()) {
    return std::nullopt;
  }
  if (m_render_settings.viewport.mode != render::ViewportMode::Bounds) {
    if (std::optional<scene::EntityId> gpu_pick = m_world_renderer.pickEntity(
            getActiveScene(), m_mesh_resource_cache, *m_viewport_camera,
            parCursorPixel, parViewportSize, &m_viewport_film_frame_state)) {
      return gpu_pick;
    }
  }
  return render::pickViewportEntityBounds(
      getActiveScene().world, &*m_viewport_camera,
      &m_viewport_film_frame_state, parCursorPixel, parViewportSize,
      ENTITY_HANDLE_EXTENT);
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
      m_camera_system.getEditorCamera().getViewProjectionMatrix(parViewportSize);
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
  const camera::Camera& camera = m_camera_system.getEditorCamera();
  return camera.position + camera.getForward() * std::max(parDistance, 0.1f);
}

scene::SceneManager::SceneRecord& EngineCore::getActiveScene() {
  return m_scene_manager.getActiveScene();
}

const scene::SceneManager::SceneRecord& EngineCore::getActiveScene() const {
  return m_scene_manager.getActiveScene();
}

EngineCore::CameraRay EngineCore::makeCameraRay(
    const glm::vec2& parCursorPixel, const glm::vec2& parViewportSize) const {
  const camera::Camera& camera = m_camera_system.getEditorCamera();
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

lighting::LightingState EngineCore::buildLightingState(
    const camera::Camera& parCamera,
    const film::FilmFrameState* parFilmState) const {
  lighting::EnvironmentIllumination environment;
  const glm::vec3 sky = render::WorldRenderer::getClearColor(
      m_render_settings.scene.sky_preset);
  environment.diffuse = glm::max(sky * 0.28f, glm::vec3(0.025f));
  environment.specular = glm::max(sky * 0.16f, glm::vec3(0.012f));
  environment.exposure = m_lighting_state.exposure;
  return m_lighting_system.extract(
      getActiveScene().world, getActiveScene().sun_light,
      parCamera.position, environment, parFilmState);
}

void EngineCore::createDefaultSceneEntities(
    scene::SceneManager::SceneRecord& parScene) {
  parScene.sun_light = {};
  parScene.selected_entity = {};
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
