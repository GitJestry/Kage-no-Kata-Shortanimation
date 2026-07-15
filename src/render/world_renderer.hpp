#pragma once

#include "assets/asset_registry.hpp"
#include "animation/animation_system.hpp"
#include "camera/camera.hpp"
#include "film/film_frame_state.hpp"
#include "lighting/light.hpp"
#include "math/transform.hpp"
#include "render/debug_primitive_renderer.hpp"
#include "render/mesh_resource_cache.hpp"
#include "render/mesh_renderer.hpp"
#include "render/viewport_policy.hpp"
#include "render/viewport_rect.hpp"
#include "render/environment_renderer.hpp"
#include "render/film_framebuffer.hpp"
#include "render/shadow_renderer.hpp"
#include "scene/scene_manager.hpp"

#include <glm/glm.hpp>

#include <cstddef>
#include <array>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace kage::render {

enum class SkyPreset {
  ClearDay,
  MountainDawn,
  WarmDusk,
  DarkStudio,
  DarkVoid
};

enum class GizmoAxisSpace {
  Local,
  World
};

struct EditorViewportSettings final {
  ViewportMode mode = ViewportMode::Solid;
  MaterialDebugMode material_debug_mode = MaterialDebugMode::Lit;
  GizmoAxisSpace gizmo_axis_space = GizmoAxisSpace::Local;
  bool floor_grid_visible = true;
  bool show_overlays = true;
  bool show_world_edit_gizmos = true;
  int floor_grid_radius = 80;
};

struct SceneRenderSettings final {
  SkyPreset sky_preset = SkyPreset::DarkVoid;
  EnvironmentSettings environment;
};

struct PerformanceSnapshot final {
  float asset_load_ms = 0.0f;
  float gpu_upload_ms = 0.0f;
  float animation_update_ms = 0.0f;
  float render_ms = 0.0f;
  float shadow_render_ms = 0.0f;
  float frame_binding_ms = 0.0f;
  float material_submission_ms = 0.0f;
  float cpu_average_ms = 0.0f;
  float cpu_p95_ms = 0.0f;
  float gpu_average_ms = 0.0f;
  float gpu_p95_ms = 0.0f;
  std::size_t visible_entities = 0;
  std::size_t culled_entities = 0;
  std::size_t draw_calls = 0;
  std::size_t submitted_instances = 0;
  std::size_t submitted_triangles = 0;
  std::size_t streaming_work_items = 0;
  std::size_t estimated_texture_bytes = 0;
  bool shadows_reused = false;
};

struct EditorRenderSettings final {
  SceneRenderSettings scene;
  EditorViewportSettings viewport;
};

struct ViewportView final {
  const camera::Camera* camera = nullptr;
  const film::FilmFrameState* film_state = nullptr;
  std::span<const film::ResolvedMovementSegment> movement_paths;
  scene::EntityId selected_entity;
  bool use_world_selection = true;
  bool black_film_output = false;
  GLuint destination_framebuffer = 0;
  bool use_film_framebuffer = false;
  std::span<const animation::EvaluatedSkinPalette> skin_palettes;
  int msaa_samples = 1;
};

struct GizmoGuide final {
  bool active = false;
  glm::vec3 origin{0.0f};
  glm::vec3 axis{1.0f, 0.0f, 0.0f};
  glm::vec3 color{1.0f};
  float half_length = 500.0f;
};

struct PlacementGhost final {
  enum class Kind {
    None,
    StaticAsset,
    Camera,
    PointLight
  };

  Kind kind = Kind::None;
  std::size_t mesh_handle = scene::INVALID_STATIC_MESH_HANDLE;
  math::Transform transform;
  glm::vec3 light_color{1.0f, 0.94f, 0.84f};
  float light_intensity = 1.0f;
  float opacity = 0.38f;

  [[nodiscard]] bool isActive() const;
};

class WorldRenderer final {
 public:
  ~WorldRenderer();
  void requestEnvironment(assets::AssetId parAsset,
                          const std::filesystem::path& parPanoramaPath);
  [[nodiscard]] EnvironmentLoadState getEnvironmentState() const;
  [[nodiscard]] const std::string& getEnvironmentError() const;
  void render(const scene::SceneManager::SceneRecord& parScene,
              const MeshResourceCache& parMeshResources,
              const ViewportView& parView,
              const lighting::LightingState& parLighting,
              const PlacementGhost& parGhost,
              const GizmoGuide& parGizmoGuide,
              const EditorRenderSettings& parSettings,
              const ViewportRect& parViewport,
              PerformanceSnapshot& parSnapshot);

  [[nodiscard]] static glm::vec3 getClearColor(SkyPreset parPreset);
  [[nodiscard]] std::optional<scene::EntityId> pickEntity(
      const scene::SceneManager::SceneRecord& parScene,
      const MeshResourceCache& parMeshResources,
      const camera::Camera& parCamera,
      const glm::vec2& parCursorPixel,
      const glm::vec2& parViewportSize,
      const film::FilmFrameState* parFilmState = nullptr);

 private:
  MeshRenderer m_mesh_renderer;
  EnvironmentRenderer m_environment_renderer;
  FilmFramebuffer m_film_framebuffer;
  ShadowRenderer m_shadow_renderer;
  DebugPrimitiveRenderer m_debug_renderer;
  std::vector<DebugVertex> m_grid_line_vertices;
  std::vector<DebugVertex> m_line_vertices;
  std::vector<DebugVertex> m_solid_vertices;
  std::vector<DebugVertex> m_glow_vertices;
  GLuint m_pick_framebuffer = 0;
  GLuint m_pick_texture = 0;
  GLuint m_pick_depth = 0;
  std::array<GLuint, 3> m_gpu_timer_queries{};
  std::array<bool, 3> m_gpu_timer_pending{};
  std::size_t m_gpu_timer_cursor = 0;
  std::array<float, 120> m_gpu_frame_samples{};
  std::size_t m_gpu_frame_sample_count = 0;
  std::size_t m_gpu_frame_sample_cursor = 0;
};

inline bool PlacementGhost::isActive() const {
  return kind != Kind::None;
}

}  // namespace kage::render
