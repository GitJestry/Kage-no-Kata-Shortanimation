#pragma once

#include "assets/asset_registry.hpp"
#include "camera/camera_system.hpp"
#include "lighting/lighting_system.hpp"
#include "math/transform.hpp"
#include "render/line_renderer.hpp"
#include "render/mesh_resource_cache.hpp"
#include "render/solid_gizmo_renderer.hpp"
#include "render/mesh_renderer.hpp"
#include "scene/scene_manager.hpp"

#include <glm/glm.hpp>

#include <cstddef>
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

enum class ViewportMode {
  Bounds,
  Solid,
  Material,
  Final
};

struct EditorViewportSettings final {
  ViewportMode mode = ViewportMode::Material;
};

struct RenderFrameStats final {
  std::size_t visible_entities = 0;
  std::size_t culled_entities = 0;
  std::size_t draw_calls = 0;
  std::size_t submitted_instances = 0;
  std::size_t submitted_triangles = 0;
};

struct EditorRenderSettings final {
  SkyPreset sky_preset = SkyPreset::DarkVoid;
  MaterialDebugMode material_debug_mode = MaterialDebugMode::Lit;
  GizmoAxisSpace gizmo_axis_space = GizmoAxisSpace::Local;
  bool floor_grid_visible = true;
  int floor_grid_radius = 80;
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
  void render(const scene::SceneManager::SceneRecord& parScene,
              const MeshResourceCache& parMeshResources,
              const camera::CameraSystem& parCameraSystem,
              const lighting::LightingState& parLighting,
              const PlacementGhost& parGhost,
              const GizmoGuide& parGizmoGuide,
              const EditorRenderSettings& parSettings,
              const glm::vec2& parViewportSize);

  [[nodiscard]] static const char* getSkyPresetName(SkyPreset parPreset);
  [[nodiscard]] static glm::vec3 getClearColor(SkyPreset parPreset);
  [[nodiscard]] const RenderFrameStats& getFrameStats() const;

 private:
  MeshRenderer m_mesh_renderer;
  SolidGizmoRenderer m_solid_gizmo_renderer;
  LineRenderer m_line_renderer;
  std::vector<SolidGizmoVertex> m_floor_vertices;
  std::vector<LineVertex> m_grid_line_vertices;
  std::vector<LineVertex> m_line_vertices;
  std::vector<SolidGizmoVertex> m_solid_vertices;
  std::vector<SolidGizmoVertex> m_glow_vertices;
  RenderFrameStats m_frame_stats;
};

inline bool PlacementGhost::isActive() const {
  return kind != Kind::None;
}

}  // namespace kage::render
