#pragma once

#include "assets/asset_registry.hpp"
#include "camera/camera_system.hpp"
#include "lighting/lighting_system.hpp"
#include "math/transform.hpp"
#include "render/line_renderer.hpp"
#include "render/mesh_resource_cache.hpp"
#include "render/solid_gizmo_renderer.hpp"
#include "render/mesh_renderer.hpp"
#include "render/viewport_policy.hpp"
#include "scene/scene_manager.hpp"

#include <glm/glm.hpp>

#include <cstddef>
#include <array>
#include <optional>
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
  ViewportMode mode = ViewportMode::Material;
  MaterialDebugMode material_debug_mode = MaterialDebugMode::Lit;
  GizmoAxisSpace gizmo_axis_space = GizmoAxisSpace::Local;
  bool floor_grid_visible = true;
  int floor_grid_radius = 80;
};

struct SceneRenderSettings final {
  SkyPreset sky_preset = SkyPreset::DarkVoid;
};

struct PerformanceSnapshot final {
  float asset_load_ms = 0.0f;
  float gpu_upload_ms = 0.0f;
  float animation_update_ms = 0.0f;
  float render_ms = 0.0f;
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
};

struct EditorRenderSettings final {
  SceneRenderSettings scene;
  EditorViewportSettings viewport;
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
  void render(const scene::SceneManager::SceneRecord& parScene,
              const MeshResourceCache& parMeshResources,
              const camera::CameraSystem& parCameraSystem,
              const lighting::LightingState& parLighting,
              const PlacementGhost& parGhost,
              const GizmoGuide& parGizmoGuide,
              const EditorRenderSettings& parSettings,
              const glm::vec2& parViewportSize,
              PerformanceSnapshot& parSnapshot);

  [[nodiscard]] static const char* getSkyPresetName(SkyPreset parPreset);
  [[nodiscard]] static glm::vec3 getClearColor(SkyPreset parPreset);
  [[nodiscard]] std::optional<scene::EntityId> pickEntity(
      const scene::SceneManager::SceneRecord& parScene,
      const MeshResourceCache& parMeshResources,
      const camera::CameraSystem& parCameraSystem,
      const EditorRenderSettings& parSettings,
      const glm::vec2& parCursorPixel,
      const glm::vec2& parViewportSize);

 private:
  MeshRenderer m_mesh_renderer;
  SolidGizmoRenderer m_solid_gizmo_renderer;
  LineRenderer m_line_renderer;
  std::vector<SolidGizmoVertex> m_floor_vertices;
  std::vector<LineVertex> m_grid_line_vertices;
  std::vector<LineVertex> m_line_vertices;
  std::vector<SolidGizmoVertex> m_solid_vertices;
  std::vector<SolidGizmoVertex> m_glow_vertices;
  GLuint m_pick_framebuffer = 0;
  GLuint m_pick_texture = 0;
  GLuint m_pick_depth = 0;
  std::unordered_map<std::uint32_t, std::size_t> m_lod_history;
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
