#pragma once

#include "assets/asset_registry.hpp"
#include "assets/asset_streamer.hpp"
#include "assets/project_asset_catalog.hpp"
#include "animation/animation_system.hpp"
#include "camera/camera_system.hpp"
#include "film/film_exporter.hpp"
#include "lighting/lighting_system.hpp"
#include "platform/runtime_paths.hpp"
#include "render/mesh_resource_cache.hpp"
#include "render/viewport_rect.hpp"
#include "render/world_renderer.hpp"
#include "scene/scene_manager.hpp"

#include <glm/glm.hpp>

#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace kage::engine {

class ProjectSerializer;

class EngineCore final {
 public:
  struct CreateFilmCameraResult final {
    scene::EntityId entity;
    film::TargetSequenceId sequence_id = 0;
    film::SequenceInstanceId instance_id = 0;
  };

  struct CameraRay final {
    glm::vec3 origin{0.0f};
    glm::vec3 direction{0.0f, 0.0f, -1.0f};
  };

  EngineCore();

  std::size_t registerStaticAsset(std::string parLabel,
                                  std::filesystem::path parPath);
  std::size_t registerStaticAsset(std::string parLabel,
                                  std::filesystem::path parPath,
                                  std::filesystem::path parSourcePath);
  std::size_t registerStaticAsset(assets::AssetId parAssetId,
                                  std::string parLabel,
                                  std::filesystem::path parPath,
                                  std::filesystem::path parSourcePath);
  std::size_t registerModelAsset(std::string parLabel,
                                 std::filesystem::path parPath,
                                 assets::ModelAsset parDocument);
  void loadProjectAssetCatalog(const std::filesystem::path& parCatalogPath);
  [[nodiscard]] std::optional<std::size_t> importModelAsset(
      const std::filesystem::path& parSourcePath, std::string parLabel,
      std::string& parError);
  [[nodiscard]] std::optional<assets::AssetId> importPanorama(
      const std::filesystem::path& parSourcePath, std::string parLabel,
      std::string& parError);
  void createDefaultProject();
  bool loadProject();
  void saveProject();
  bool loadLocalSession();
  void saveLocalSession();
  [[nodiscard]] bool exportFilmSequence(std::string& parError);
  [[nodiscard]] film::TimelineValidation validateMovieTimeline(
      bool parForBake = false) const;
  void advanceFilmExport();
  void cancelFilmExport();
  [[nodiscard]] const film::FinalRenderJob& getFinalRenderJob() const;
  void markProjectDirty();
  void markLocalSessionDirty();
  [[nodiscard]] bool isProjectDirty() const;
  [[nodiscard]] const platform::RuntimePaths& getRuntimePaths() const;

  std::size_t createScene(std::string parName);
  std::size_t createLocalScene(std::string parName);
  bool deleteScene(std::size_t parSceneIndex);
  void setActiveScene(std::size_t parSceneIndex);
  void renameScene(std::size_t parSceneIndex, std::string parName);

  scene::EntityId instantiateAssetAt(std::size_t parAssetIndex,
                                     const glm::vec3& parPosition);
  scene::EntityId createCameraEntityAt(std::string parName,
                                       const glm::vec3& parPosition);
  scene::EntityId createPointLightEntityAt(std::string parName,
                                           const glm::vec3& parPosition);
  [[nodiscard]] std::expected<CreateFilmCameraResult, std::string>
  createFilmCameraFromView(film::FilmFrame parStartFrame);
  bool deleteEntity(scene::EntityId parEntity);
  void selectEntity(scene::EntityId parEntity);
  void clearSelection();
  void frameEntity(scene::EntityId parEntity);
  void resetEditorCamera();
  void frameWorld();
  void setEntityName(scene::EntityId parEntity, std::string parName);
  void setEntityPosition(scene::EntityId parEntity,
                         const glm::vec3& parPosition);
  void setEntityTransform(scene::EntityId parEntity,
                          const math::Transform& parTransform);
  void setEntityCamera(scene::EntityId parEntity,
                       const scene::CameraComponent& parCamera);
  void setStaticMeshVisible(scene::EntityId parEntity, bool parVisible);
  void setLight(scene::EntityId parEntity,
                const scene::LightComponent& parLight);
  void setSunLightSettings(const scene::SunLightSettings& parSunLight);
  void setExposure(float parExposure);
  void setPlacementGhost(render::PlacementGhost parGhost);
  void clearPlacementGhost();
  void setGizmoGuide(render::GizmoGuide parGuide);
  void clearGizmoGuide();
  void requestAssetLoad(std::size_t parAssetIndex);

  void setSkyPreset(render::SkyPreset parPreset);
  void setEnvironmentSettings(render::EnvironmentSettings parSettings);
  void setFloorGridVisible(bool parVisible);
  void setFloorGridRadius(int parRadius);
  void setEditorViewDistance(float parFarPlane);
  void setMaterialDebugMode(render::MaterialDebugMode parMode);
  void setGizmoAxisSpace(render::GizmoAxisSpace parAxisSpace);
  void setViewportMode(render::ViewportMode parMode);

  void update(float parDeltaSeconds, bool parMovieWorkspace,
              film::FilmFrame parPlaybackDuration = -1);
  void render(const render::ViewportRect& parViewport,
              bool parMovieWorkspace = false,
              bool parShotPreview = false, double parFilmFrame = -1.0,
              bool parShowOverlays = true,
              unsigned int parDestinationFramebuffer = 0,
              int parMsaaSamples = 1,
              film::TargetSequenceId parPreviewSequenceId = 0,
              scene::EntityId parMovieSelectionEntity = {},
              std::span<const film::ResolvedMovementPath> parMovementPaths = {});
  [[nodiscard]] const render::PerformanceSnapshot& getPerformanceSnapshot()
      const;

  [[nodiscard]] scene::World& getWorld();
  [[nodiscard]] const scene::World& getWorld() const;
  [[nodiscard]] camera::CameraSystem& getCameraSystem();
  [[nodiscard]] const lighting::LightingState& getLightingState() const;
  [[nodiscard]] assets::AssetRegistry& getAssetRegistry();
  [[nodiscard]] std::span<const assets::AssetRegistry::AssetLibraryEntry>
  getAssetLibrary() const;
  [[nodiscard]] std::span<const assets::EnvironmentAsset>
  getEnvironmentAssets() const;
  [[nodiscard]] std::span<const scene::SceneManager::SceneRecord> getScenes()
      const;
  [[nodiscard]] const assets::StaticModel* getStaticMeshSource(
      assets::AssetRegistry::StaticMeshHandle parHandle) const;
  [[nodiscard]] const assets::AssetRegistry::AssetLibraryEntry*
  getAssetLibraryEntry(std::size_t parAssetIndex) const;
  [[nodiscard]] scene::EntityId getSelectedEntity() const;
  [[nodiscard]] const scene::SunLightSettings& getSunLightSettings() const;
  [[nodiscard]] std::size_t getActiveSceneIndex() const;
  [[nodiscard]] render::SkyPreset getSkyPreset() const;
  [[nodiscard]] const render::EnvironmentSettings& getEnvironmentSettings()
      const;
  [[nodiscard]] render::EnvironmentLoadState getEnvironmentLoadState() const;
  [[nodiscard]] const std::string& getEnvironmentError() const;
  [[nodiscard]] bool isFloorGridVisible() const;
  [[nodiscard]] int getFloorGridRadius() const;
  [[nodiscard]] float getEditorViewDistance() const;
  [[nodiscard]] render::MaterialDebugMode getMaterialDebugMode() const;
  [[nodiscard]] render::GizmoAxisSpace getGizmoAxisSpace() const;
  [[nodiscard]] render::ViewportMode getViewportMode() const;
  [[nodiscard]] film::MovieTimeline& getMovieTimeline();
  [[nodiscard]] film::FilmPlayback& getFilmPlayback();
  void clearFilmPreviewState();
  [[nodiscard]] math::Bounds3 getEntityWorldBounds(
      scene::EntityId parEntity) const;
  [[nodiscard]] std::optional<scene::EntityId> pickEntity(
      const glm::vec2& parCursorPixel,
      const glm::vec2& parViewportSize);
  [[nodiscard]] std::optional<scene::EntityId> pickMovieEntity(
      const glm::vec2& parCursorPixel,
      const glm::vec2& parViewportSize);
  [[nodiscard]] bool isCursorOverEntityCore(
      scene::EntityId parEntity, const glm::vec2& parCursorPixel,
      const glm::vec2& parViewportSize) const;
  [[nodiscard]] glm::vec3 getPlacementPointOnFloor(
      const glm::vec2& parCursorPixel,
      const glm::vec2& parViewportSize) const;
  [[nodiscard]] glm::vec3 getPointInFrontOfCamera(float parDistance) const;

 private:
  void attachLoadedAssetToInstances(std::size_t parAssetIndex);
  void pollAssetStreaming();
  [[nodiscard]] scene::SceneManager::SceneRecord& getActiveScene();
  [[nodiscard]] const scene::SceneManager::SceneRecord& getActiveScene() const;
  [[nodiscard]] CameraRay makeCameraRay(const glm::vec2& parCursorPixel,
                                        const glm::vec2& parViewportSize) const;
  [[nodiscard]] lighting::LightingState buildLightingState(
      const camera::Camera& parCamera,
      const film::FilmFrameState* parFilmState = nullptr) const;
  void createDefaultSceneEntities(scene::SceneManager::SceneRecord& parScene);
  void rebuildAssetInstanceCounts();
  friend class ProjectSerializer;

  platform::RuntimePaths m_runtime_paths;
  assets::AssetRegistry m_asset_registry;
  assets::AssetStreamer m_asset_streamer;
  std::vector<assets::EnvironmentAsset> m_environment_assets;
  animation::AnimationSystem m_animation_system;
  scene::SceneManager m_scene_manager;
  camera::CameraSystem m_camera_system;
  lighting::LightingSystem m_lighting_system;
  lighting::LightingState m_lighting_state;
  render::MeshResourceCache m_mesh_resource_cache;
  render::WorldRenderer m_world_renderer;
  render::EditorRenderSettings m_render_settings;
  render::PlacementGhost m_placement_ghost;
  render::GizmoGuide m_gizmo_guide;
  render::PerformanceSnapshot m_performance_snapshot;
  film::FilmPlayback m_film_playback;
  film::FilmFrameState m_film_frame_state;
  film::FilmFrameState m_viewport_film_frame_state;
  std::optional<camera::Camera> m_viewport_camera;
  bool m_viewport_consumes_film_state = false;
  bool m_viewport_black_output = false;
  std::vector<animation::EvaluatedSkinPalette> m_film_skin_palettes;
  film::FinalRenderJob m_final_render_job;
  std::array<float, 120> m_cpu_frame_samples{};
  std::size_t m_cpu_frame_sample_count = 0;
  std::size_t m_cpu_frame_sample_cursor = 0;
  bool m_project_dirty = false;
  bool m_local_session_dirty = false;
  float m_local_session_autosave_timer_seconds = 0.0f;
  float m_last_session_fly_speed = 0.0f;
  bool m_streaming_was_active = false;
  int m_allocator_relief_passes_remaining = 0;
  float m_allocator_relief_timer_seconds = 0.0f;
};

}  // namespace kage::engine
