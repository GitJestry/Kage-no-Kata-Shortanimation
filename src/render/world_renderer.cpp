#include "render/world_renderer.hpp"

#include "math/cubic_bezier.hpp"
#include "render/gizmo_metrics.hpp"
#include "render/viewport_picking.hpp"

#include "camera/screen_metrics.hpp"

#include <glad/gl.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <span>
#include <utility>

namespace {

constexpr glm::vec3 GRID_COLOR{0.115f, 0.12f, 0.13f};
constexpr glm::vec3 GRID_AXIS_X_COLOR{0.34f, 0.12f, 0.12f};
constexpr glm::vec3 GRID_AXIS_Z_COLOR{0.12f, 0.28f, 0.14f};
constexpr glm::vec3 SELECTED_CONTACT_COLOR{1.0f, 1.0f, 1.0f};
constexpr glm::vec3 FLOOR_INTERSECTION_COLOR{1.0f, 0.18f, 0.12f};
constexpr glm::vec3 ROTATION_COLOR{1.0f, 0.18f, 0.14f};
constexpr glm::vec3 CAMERA_GIZMO_COLOR{0.40f, 0.74f, 1.0f};
constexpr glm::vec3 MOVEMENT_PATH_COLOR{0.18f, 0.58f, 1.0f};
constexpr glm::vec3 MOVEMENT_TRANSITION_COLOR{0.62f, 0.34f, 0.86f};
constexpr glm::vec4 MOVEMENT_POINT_FILL{0.18f, 0.58f, 1.0f, 1.0f};
constexpr glm::vec4 MOVEMENT_TRANSITION_POINT_FILL{0.62f, 0.34f, 0.86f,
                                                    1.0f};
constexpr float GIZMO_AXIS_ALPHA = 0.92f;
constexpr glm::vec4 ROTATION_FILL{1.0f, 0.18f, 0.14f, 0.70f};
constexpr int GRID_LINE_CAP = 240;
constexpr kage::render::ShadowRenderSettings FINAL_SHADOW_SETTINGS{
    4096, 36.0f, true};
constexpr kage::render::ShadowRenderSettings MATERIAL_SHADOW_SETTINGS{
    2048, 70.0f, false};

void addLine(std::vector<kage::render::DebugVertex>& parVertices,
             const glm::vec3& parStart, const glm::vec3& parEnd,
             const glm::vec3& parColor) {
  const glm::vec4 color(parColor, 1.0f);
  parVertices.push_back({parStart, color});
  parVertices.push_back({parEnd, color});
}

void addTriangle(std::vector<kage::render::DebugVertex>& parVertices,
                 const glm::vec3& parA, const glm::vec3& parB,
                 const glm::vec3& parC, const glm::vec4& parColor) {
  parVertices.push_back({parA, parColor});
  parVertices.push_back({parB, parColor});
  parVertices.push_back({parC, parColor});
}

void addCube(std::vector<kage::render::DebugVertex>& parVertices,
             const glm::vec3& parCenter, float parSize,
             const glm::vec4& parColor) {
  const glm::vec3 e(parSize * 0.5f);
  const std::array<glm::vec3, 8> p = {
      parCenter + glm::vec3(-e.x, -e.y, -e.z),
      parCenter + glm::vec3(e.x, -e.y, -e.z),
      parCenter + glm::vec3(e.x, e.y, -e.z),
      parCenter + glm::vec3(-e.x, e.y, -e.z),
      parCenter + glm::vec3(-e.x, -e.y, e.z),
      parCenter + glm::vec3(e.x, -e.y, e.z),
      parCenter + glm::vec3(e.x, e.y, e.z),
      parCenter + glm::vec3(-e.x, e.y, e.z),
  };
  const std::array<std::array<int, 3>, 12> faces = {
      std::array<int, 3>{0, 1, 2}, {0, 2, 3}, {4, 6, 5}, {4, 7, 6},
      {0, 4, 5},                 {0, 5, 1}, {3, 2, 6}, {3, 6, 7},
      {1, 5, 6},                 {1, 6, 2}, {0, 3, 7}, {0, 7, 4},
  };
  for (const auto& face : faces) {
    addTriangle(parVertices, p[face[0]], p[face[1]], p[face[2]], parColor);
  }
}

[[nodiscard]] glm::vec3 getPerpendicularHelper(const glm::vec3& parDirection) {
  return std::abs(glm::dot(parDirection, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.92f
             ? glm::vec3(1.0f, 0.0f, 0.0f)
             : glm::vec3(0.0f, 1.0f, 0.0f);
}

[[nodiscard]] std::pair<glm::vec3, glm::vec3> getPerpendicularAxes(
    const glm::vec3& parDirection) {
  const glm::vec3 side = glm::normalize(
      glm::cross(parDirection, getPerpendicularHelper(parDirection)));
  return {side, glm::normalize(glm::cross(side, parDirection))};
}

template <typename SegmentVisitor>
void forEachRingSegment(const glm::vec3& parCenter,
                        const glm::vec3& parAxisA,
                        const glm::vec3& parAxisB, float parRadius,
                        int parSegmentCount, SegmentVisitor&& parVisitor) {
  glm::vec3 previous = parCenter + parAxisA * parRadius;
  for (int segment = 1; segment <= parSegmentCount; ++segment) {
    const float angle =
        (static_cast<float>(segment) / static_cast<float>(parSegmentCount)) *
        glm::two_pi<float>();
    const glm::vec3 current =
        parCenter + (std::cos(angle) * parAxisA +
                     std::sin(angle) * parAxisB) *
                        parRadius;
    parVisitor(previous, current);
    previous = current;
  }
}

void addDisk(std::vector<kage::render::DebugVertex>& parVertices,
             const glm::vec3& parCenter, const glm::vec3& parAxisA,
             const glm::vec3& parAxisB, float parRadius,
             const glm::vec4& parColor) {
  constexpr int SEGMENT_COUNT = 32;
  forEachRingSegment(parCenter, parAxisA, parAxisB, parRadius, SEGMENT_COUNT,
                     [&](const glm::vec3& previous,
                         const glm::vec3& current) {
                       addTriangle(parVertices, parCenter, previous, current,
                                   parColor);
                     });
}

void addCylinder(std::vector<kage::render::DebugVertex>& parVertices,
                 const glm::vec3& parStart, const glm::vec3& parEnd,
                 float parRadius, const glm::vec4& parColor) {
  constexpr int SEGMENT_COUNT = 12;
  const glm::vec3 direction = glm::normalize(parEnd - parStart);
  const auto [side, up] = getPerpendicularAxes(direction);
  forEachRingSegment(glm::vec3(0.0f), side, up, parRadius, SEGMENT_COUNT,
                     [&](const glm::vec3& offset_a,
                         const glm::vec3& offset_b) {
                       addTriangle(parVertices, parStart + offset_a,
                                   parEnd + offset_a, parEnd + offset_b,
                                   parColor);
                       addTriangle(parVertices, parStart + offset_a,
                                   parEnd + offset_b, parStart + offset_b,
                                   parColor);
                     });
}

void addCone(std::vector<kage::render::DebugVertex>& parVertices,
             const glm::vec3& parTip, const glm::vec3& parDirection,
             float parLength, float parRadius, const glm::vec4& parColor) {
  constexpr int SEGMENT_COUNT = 16;
  const glm::vec3 direction = glm::normalize(parDirection);
  const glm::vec3 base = parTip - direction * parLength;
  const auto [side, up] = getPerpendicularAxes(direction);
  forEachRingSegment(base, side, up, parRadius, SEGMENT_COUNT,
                     [&](const glm::vec3& previous,
                         const glm::vec3& current) {
                       addTriangle(parVertices, parTip, previous, current,
                                   parColor);
                       addTriangle(parVertices, base, current, previous,
                                   parColor);
                     });
}

void addSolidSphere(std::vector<kage::render::DebugVertex>& parVertices,
                    const glm::vec3& parCenter, float parRadius,
                    const glm::vec4& parColor) {
  const std::array<glm::vec3, 6> points = {
      glm::vec3(0.0f, parRadius, 0.0f),
      glm::vec3(0.0f, -parRadius, 0.0f),
      glm::vec3(parRadius, 0.0f, 0.0f),
      glm::vec3(-parRadius, 0.0f, 0.0f),
      glm::vec3(0.0f, 0.0f, parRadius),
      glm::vec3(0.0f, 0.0f, -parRadius),
  };
  constexpr std::array<std::array<int, 3>, 8> FACES = {{
      {{0, 2, 4}}, {{0, 4, 3}}, {{0, 3, 5}}, {{0, 5, 2}},
      {{1, 4, 2}}, {{1, 3, 4}}, {{1, 5, 3}}, {{1, 2, 5}},
  }};
  for (const auto& face : FACES) {
    addTriangle(parVertices, parCenter + points[face[0]],
                parCenter + points[face[1]], parCenter + points[face[2]],
                parColor);
  }
}

void addMovementPathSegment(
    std::vector<kage::render::DebugVertex>& parVertices,
    const kage::film::ResolvedMovementSpline& parSegment,
    const glm::vec3& parColor) {
  constexpr int SEGMENT_COUNT = 32;
  glm::vec3 previous = parSegment.start.translation;
  for (int segment = 1; segment <= SEGMENT_COUNT; ++segment) {
    const float t =
        static_cast<float>(segment) / static_cast<float>(SEGMENT_COUNT);
    const glm::vec3 current = kage::math::cubicBezier(
        parSegment.start.translation, parSegment.control_1,
        parSegment.control_2, parSegment.end.translation, t);
    addLine(parVertices, previous, current, parColor);
    previous = current;
  }
}

void addMovementPathOverlay(
    std::vector<kage::render::DebugVertex>& parLines,
    std::vector<kage::render::DebugVertex>& parSolid,
    const kage::film::ResolvedMovementSegment& parPath,
    const kage::camera::Camera& parCamera,
    const glm::vec2& parViewportSize) {
  const auto markerRadius = [&](const glm::vec3& parPosition) {
    return std::clamp(kage::camera::getWorldLengthForPixels(
                          parCamera, parPosition, parViewportSize, 10.0f),
                      0.04f, 1.5f);
  };

  if (parPath.transition_before.has_value() &&
      parPath.transition_before->enabled) {
    const auto& transition = parPath.transition_before->spline;
    addMovementPathSegment(parLines, transition,
                           MOVEMENT_TRANSITION_COLOR);
    addSolidSphere(parSolid, transition.start.translation,
                   markerRadius(transition.start.translation),
                   MOVEMENT_TRANSITION_POINT_FILL);
  }
  addMovementPathSegment(parLines, parPath.movement, MOVEMENT_PATH_COLOR);
  addSolidSphere(parSolid, parPath.movement.start.translation,
                 markerRadius(parPath.movement.start.translation),
                 MOVEMENT_POINT_FILL);
  addSolidSphere(parSolid, parPath.movement.end.translation,
                 markerRadius(parPath.movement.end.translation),
                 MOVEMENT_POINT_FILL);
}

void addCircle(std::vector<kage::render::DebugVertex>& parVertices,
               const glm::vec3& parCenter, const glm::vec3& parAxisA,
               const glm::vec3& parAxisB, float parRadius,
               const glm::vec3& parColor) {
  constexpr int SEGMENT_COUNT = 48;
  forEachRingSegment(parCenter, parAxisA, parAxisB, parRadius, SEGMENT_COUNT,
                     [&](const glm::vec3& previous,
                         const glm::vec3& current) {
                       addLine(parVertices, previous, current, parColor);
                     });
}

void addFloorGrid(std::vector<kage::render::DebugVertex>& parVertices,
                  const glm::vec3& parCameraPosition, int parRadius) {
  const int radius = std::clamp(parRadius, 8, GRID_LINE_CAP / 2);
  const int center_x = static_cast<int>(std::floor(parCameraPosition.x));
  const int center_z = static_cast<int>(std::floor(parCameraPosition.z));
  const int min_x = center_x - radius;
  const int max_x = center_x + radius;
  const int min_z = center_z - radius;
  const int max_z = center_z + radius;

  for (int x = min_x; x <= max_x; ++x) {
    const glm::vec3 color = x == 0 ? GRID_AXIS_X_COLOR : GRID_COLOR;
    addLine(parVertices, glm::vec3(x, 0.0f, min_z),
            glm::vec3(x, 0.0f, max_z), color);
  }
  for (int z = min_z; z <= max_z; ++z) {
    const glm::vec3 color = z == 0 ? GRID_AXIS_Z_COLOR : GRID_COLOR;
    addLine(parVertices, glm::vec3(min_x, 0.0f, z),
            glm::vec3(max_x, 0.0f, z), color);
  }
}

void addFloorContactCue(std::vector<kage::render::DebugVertex>& parVertices,
                        const kage::math::Bounds3& parBounds,
                        const glm::vec3& parOrigin) {
  if (!parBounds.is_valid) {
    addLine(parVertices, parOrigin, glm::vec3(parOrigin.x, 0.0f, parOrigin.z),
            SELECTED_CONTACT_COLOR);
    return;
  }

  const bool intersects_floor = parBounds.min.y < 0.0f;
  const glm::vec3 color =
      intersects_floor ? FLOOR_INTERSECTION_COLOR : SELECTED_CONTACT_COLOR;
  constexpr float CUE_HEIGHT = 0.004f;
  const std::array<glm::vec3, 4> footprint = {
      glm::vec3(parBounds.min.x, CUE_HEIGHT, parBounds.min.z),
      glm::vec3(parBounds.max.x, CUE_HEIGHT, parBounds.min.z),
      glm::vec3(parBounds.max.x, CUE_HEIGHT, parBounds.max.z),
      glm::vec3(parBounds.min.x, CUE_HEIGHT, parBounds.max.z),
  };
  for (std::size_t index = 0; index < footprint.size(); ++index) {
    addLine(parVertices, footprint[index],
            footprint[(index + 1) % footprint.size()], color);
  }

  const glm::vec3 bottom_center((parBounds.min.x + parBounds.max.x) * 0.5f,
                                parBounds.min.y,
                                (parBounds.min.z + parBounds.max.z) * 0.5f);
  addLine(parVertices, bottom_center,
          glm::vec3(bottom_center.x, 0.0f, bottom_center.z), color);
}

void addTransformAxes(std::vector<kage::render::DebugVertex>& parVertices,
                      std::vector<kage::render::DebugVertex>& parSolid,
                      const kage::math::Transform& parTransform,
                      float parLength,
                      kage::render::GizmoAxisSpace parAxisSpace) {
  const glm::vec3 position = parTransform.translation;
  const glm::quat rotation =
      parAxisSpace == kage::render::GizmoAxisSpace::World
          ? glm::quat(1.0f, 0.0f, 0.0f, 0.0f)
          : parTransform.rotation;
  const std::array<glm::vec3, 3> axes{
      rotation * glm::vec3(1.0f, 0.0f, 0.0f),
      rotation * glm::vec3(0.0f, 1.0f, 0.0f),
      rotation * glm::vec3(0.0f, 0.0f, 1.0f)};
  constexpr std::array<glm::vec3, 3> COLORS{
      kage::render::GIZMO_AXIS_X_COLOR, kage::render::GIZMO_AXIS_Y_COLOR,
      kage::render::GIZMO_AXIS_Z_COLOR};
  const float shaft_radius = std::max(parLength * 0.012f, 0.01f);
  const float head_length = parLength * 0.16f;
  const float head_radius = parLength * 0.04f;
  for (std::size_t index = 0; index < axes.size(); ++index) {
    const glm::vec4 fill(COLORS[index], GIZMO_AXIS_ALPHA);
    addCylinder(parSolid, position,
                position + axes[index] * (parLength - head_length),
                shaft_radius, fill);
    addCone(parSolid, position + axes[index] * parLength, axes[index],
            head_length, head_radius, fill);
  }
  addSolidSphere(parSolid, position, parLength * 0.055f, ROTATION_FILL);
  addCircle(parVertices, position, axes[0], axes[2], parLength * 0.24f,
            ROTATION_COLOR);
  const glm::vec3 handle_direction = glm::normalize(axes[0] + axes[1]);
  const glm::vec3 handle_position =
      position + handle_direction * parLength * 0.42f;
  addLine(parVertices, position, handle_position, ROTATION_COLOR);
  addCube(parSolid, handle_position, std::max(parLength * 0.075f, 0.045f),
          ROTATION_FILL);
}

void addOriginCore(std::vector<kage::render::DebugVertex>& parVertices,
                   const glm::vec3& parPosition, float parRadius) {
  constexpr std::array<glm::vec3, 3> AXES{
      glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
      glm::vec3(0.0f, 0.0f, 1.0f)};
  for (std::size_t axis = 0; axis < AXES.size(); ++axis) {
    addCircle(parVertices, parPosition, AXES[axis], AXES[(axis + 1) % 3],
              parRadius, SELECTED_CONTACT_COLOR);
  }
}

void addLightGizmo(std::vector<kage::render::DebugVertex>& parVertices,
                   std::vector<kage::render::DebugVertex>& parSolid,
                   const kage::math::Transform& parTransform,
                   const kage::scene::LightComponent& parLight,
                   const glm::vec3& parCameraRight,
                   const glm::vec3& parCameraUp) {
  const glm::vec3 position = parTransform.translation;
  const glm::vec3 color = glm::clamp(parLight.color, glm::vec3(0.0f),
                                      glm::vec3(1.0f));
  constexpr float SOURCE_RADIUS = 0.16f;
  addSolidSphere(parSolid, position, SOURCE_RADIUS, glm::vec4(color, 1.0f));
  addCircle(parVertices, position, parCameraRight, parCameraUp,
            std::max(parLight.range, SOURCE_RADIUS * 2.0f), color * 0.42f);
}

void addSunOverlay(std::vector<kage::render::DebugVertex>& parVertices,
                   std::vector<kage::render::DebugVertex>& parSolid,
                   std::vector<kage::render::DebugVertex>& parGlow,
                   const kage::camera::Camera& parCamera,
                   const glm::vec2& parViewportSize,
                   const kage::lighting::DirectionalLight& parSun) {
  if (!parSun.enabled || parSun.intensity <= 0.0f) {
    return;
  }

  const float direction_length = glm::length(parSun.direction_to_light);
  if (direction_length <= 0.0001f) {
    return;
  }

  const glm::vec3 direction_to_sun =
      parSun.direction_to_light / direction_length;
  const float sky_distance = std::clamp(parCamera.far_plane * 0.22f,
                                        45.0f, 140.0f);
  const glm::vec3 sun_position =
      parCamera.position + direction_to_sun * sky_distance;
  const glm::vec3 color =
      glm::clamp(parSun.color * (0.55f + parSun.intensity * 0.30f),
                 glm::vec3(0.0f), glm::vec3(1.0f));
  const float disk_radius = kage::camera::getWorldLengthForPixels(
      parCamera, sun_position, parViewportSize, 34.0f);
  const glm::vec4 disk_color(color, 0.92f);
  const glm::vec4 glow_color(color, 0.20f);
  addDisk(parSolid, sun_position, parCamera.getRight(), parCamera.getUp(),
          disk_radius, disk_color);
  addDisk(parGlow, sun_position, parCamera.getRight(), parCamera.getUp(),
          disk_radius * 2.2f, glow_color);

  const glm::vec3 ray_color = color * 0.48f;
  const float ray_length = disk_radius * 8.0f;
  const float ray_spacing = disk_radius * 1.85f;
  const std::array<glm::vec2, 5> offsets = {
      glm::vec2(0.0f), glm::vec2(-1.0f, 0.0f), glm::vec2(1.0f, 0.0f),
      glm::vec2(0.0f, -1.0f), glm::vec2(0.0f, 1.0f),
  };
  for (const glm::vec2& offset : offsets) {
    const glm::vec3 ray_start =
        sun_position + parCamera.getRight() * offset.x * ray_spacing +
        parCamera.getUp() * offset.y * ray_spacing;
    addLine(parVertices, ray_start,
            ray_start - direction_to_sun * ray_length, ray_color);
  }
}

[[nodiscard]] std::optional<glm::vec3> findNearestMeshCenter(
    const kage::scene::SceneManager::SceneRecord& parScene,
    const glm::vec3& parPosition) {
  std::optional<glm::vec3> result;
  float best_distance_squared = std::numeric_limits<float>::max();
  for (const kage::scene::EntityRecord& entity :
       parScene.world.getEntities()) {
    if (!entity.alive || !entity.static_mesh.has_value() ||
        !entity.static_mesh->visible) {
      continue;
    }

    const kage::math::Bounds3 bounds = kage::render::viewportEntityBounds(
        entity, entity.transform.transform);
    if (!bounds.is_valid) {
      continue;
    }

    const glm::vec3 center = (bounds.min + bounds.max) * 0.5f;
    const float distance_squared = glm::dot(center - parPosition,
                                           center - parPosition);
    if (!result || distance_squared < best_distance_squared) {
      result = center;
      best_distance_squared = distance_squared;
    }
  }

  return result;
}

void addCameraGizmo(std::vector<kage::render::DebugVertex>& parVertices,
                    const kage::math::Transform& parTransform) {
  const glm::vec3 position = parTransform.translation;
  const glm::vec3 right = parTransform.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
  const glm::vec3 up = parTransform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
  const glm::vec3 forward =
      parTransform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
  constexpr float DEPTH = 0.45f;
  constexpr float WIDTH = 0.28f;
  constexpr float HEIGHT = 0.18f;
  const glm::vec3 center = position + forward * DEPTH;
  const std::array<glm::vec3, 4> corners = {
      center - right * WIDTH - up * HEIGHT,
      center + right * WIDTH - up * HEIGHT,
      center + right * WIDTH + up * HEIGHT,
      center - right * WIDTH + up * HEIGHT,
  };
  for (const glm::vec3& corner : corners) {
    addLine(parVertices, position, corner, CAMERA_GIZMO_COLOR);
  }
  for (std::size_t index = 0; index < corners.size(); ++index) {
    addLine(parVertices, corners[index], corners[(index + 1) % corners.size()],
            CAMERA_GIZMO_COLOR);
  }
}

}  // namespace

namespace kage::render {

WorldRenderer::~WorldRenderer() {
  if (m_gpu_timer_queries[0] != 0) {
    glDeleteQueries(static_cast<GLsizei>(m_gpu_timer_queries.size()),
                    m_gpu_timer_queries.data());
  }
  if (m_pick_depth != 0) {
    glDeleteRenderbuffers(1, &m_pick_depth);
  }
  if (m_pick_texture != 0) {
    glDeleteTextures(1, &m_pick_texture);
  }
  if (m_pick_framebuffer != 0) {
    glDeleteFramebuffers(1, &m_pick_framebuffer);
  }
}

void WorldRenderer::requestEnvironment(
    assets::AssetId parAsset,
    const std::filesystem::path& parPanoramaPath) {
  m_environment_renderer.request(parAsset, parPanoramaPath);
}

EnvironmentLoadState WorldRenderer::getEnvironmentState() const {
  return m_environment_renderer.getState();
}

const std::string& WorldRenderer::getEnvironmentError() const {
  return m_environment_renderer.getError();
}

void WorldRenderer::render(const scene::SceneManager::SceneRecord& parScene,
                           const MeshResourceCache& parMeshResources,
                           const ViewportView& parView,
                           const lighting::LightingState& parLighting,
                           const PlacementGhost& parGhost,
                           const GizmoGuide& parGizmoGuide,
                           const EditorRenderSettings& parSettings,
                           const ViewportRect& parViewport,
                           PerformanceSnapshot& parSnapshot) {
  const glm::vec2 parViewportSize = parViewport.extent();
  if (m_gpu_timer_queries[0] == 0) {
    glGenQueries(static_cast<GLsizei>(m_gpu_timer_queries.size()),
                 m_gpu_timer_queries.data());
  }
  const std::size_t timer_slot = m_gpu_timer_cursor;
  bool timer_active = false;
  if (m_gpu_timer_pending[timer_slot]) {
    GLint available = GL_FALSE;
    glGetQueryObjectiv(m_gpu_timer_queries[timer_slot],
                       GL_QUERY_RESULT_AVAILABLE, &available);
    if (available == GL_TRUE) {
      GLuint64 nanoseconds = 0;
      glGetQueryObjectui64v(m_gpu_timer_queries[timer_slot], GL_QUERY_RESULT,
                            &nanoseconds);
      const float milliseconds =
          static_cast<float>(nanoseconds) / 1000000.0f;
      const FrameTimeStats gpu_stats =
          m_gpu_frame_history.record(milliseconds);
      parSnapshot.gpu_average_ms = gpu_stats.average_ms;
      parSnapshot.gpu_p95_ms = gpu_stats.p95_ms;
      m_gpu_timer_pending[timer_slot] = false;
    }
  }
  if (!m_gpu_timer_pending[timer_slot]) {
    glBeginQuery(GL_TIME_ELAPSED, m_gpu_timer_queries[timer_slot]);
    timer_active = true;
  }
  const auto finish_gpu_timer = [&]() {
    if (!timer_active) {
      return;
    }
    glEndQuery(GL_TIME_ELAPSED);
    m_gpu_timer_pending[timer_slot] = true;
    m_gpu_timer_cursor =
        (m_gpu_timer_cursor + 1) % m_gpu_timer_queries.size();
  };
  const auto bind_render_target = [&]() {
    if (parView.use_film_framebuffer) {
      m_film_framebuffer.resume();
      return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, parView.destination_framebuffer);
    glViewport(parViewport.origin.x, parViewport.glViewportY(),
               std::max(parViewport.size.x, 1),
               std::max(parViewport.size.y, 1));
  };
  if (parView.use_film_framebuffer) {
    m_film_framebuffer.begin(parViewport, parView.destination_framebuffer,
                             parView.msaa_samples);
  } else {
    bind_render_target();
  }
  parSnapshot.visible_entities = 0;
  parSnapshot.culled_entities = 0;
  parSnapshot.draw_calls = 0;
  parSnapshot.submitted_instances = 0;
  parSnapshot.submitted_triangles = 0;
  parSnapshot.shadow_render_ms = 0.0f;
  parSnapshot.frame_binding_ms = 0.0f;
  parSnapshot.material_submission_ms = 0.0f;
  parSnapshot.shadows_reused = false;
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  const glm::vec3 clear_color =
      parView.black_film_output ? glm::vec3(0.0f)
                                 : getClearColor(parSettings.scene.sky_preset);
  glClearColor(clear_color.r, clear_color.g, clear_color.b, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (parView.camera == nullptr) {
    if (parView.use_film_framebuffer) {
      m_film_framebuffer.present();
    }
    finish_gpu_timer();
    return;
  }
  const camera::Camera& camera = *parView.camera;
  m_environment_renderer.draw(camera, parViewportSize,
                              parSettings.scene.environment);
  const glm::mat4 view_projection =
      camera.getViewProjectionMatrix(parViewportSize);
  const auto find_skin_matrices = [&](const scene::EntityRecord& entity)
      -> std::span<const std::vector<glm::mat4>> {
    const auto evaluated = std::find_if(
        parView.skin_palettes.begin(), parView.skin_palettes.end(),
        [&](const animation::EvaluatedSkinPalette& palette) {
          return palette.entity == entity.id;
        });
    if (evaluated != parView.skin_palettes.end()) {
      return evaluated->primitive_skin_matrices;
    }
    return entity.rig.has_value()
               ? std::span<const std::vector<glm::mat4>>(
                     entity.rig->primitive_skin_matrices)
               : std::span<const std::vector<glm::mat4>>{};
  };

  struct FrameMesh final {
    const GpuMesh* mesh = nullptr;
    math::Transform transform;
    glm::mat4 model{1.0f};
    math::Bounds3 world_bounds;
    std::span<const std::vector<glm::mat4>> skin_matrices;
  };
  struct VisibleMesh final {
    const FrameMesh* source = nullptr;
    float camera_distance = 0.0f;
  };
  thread_local std::vector<FrameMesh> frame_meshes;
  frame_meshes.clear();
  frame_meshes.reserve(parScene.world.getEntities().size());
  thread_local std::vector<VisibleMesh> visible_meshes;
  visible_meshes.clear();
  visible_meshes.reserve(parScene.world.getEntities().size());

  for (const scene::EntityRecord& entity : parScene.world.getEntities()) {
    if (!entity.alive || !entity.static_mesh.has_value() ||
        !entity.static_mesh->visible) {
      continue;
    }

    const GpuMesh* mesh = parMeshResources.getStaticMesh(
        entity.static_mesh->asset_library_index);
    if (mesh == nullptr) {
      continue;
    }
    const math::Transform display_transform =
        viewportEntityTransform(entity, parView.film_state);
    const math::Bounds3 world_bounds =
        viewportEntityBounds(entity, display_transform);
    frame_meshes.push_back({mesh, display_transform,
                            display_transform.toMatrix(), world_bounds,
                            find_skin_matrices(entity)});
  }

  for (const FrameMesh& frame_mesh : frame_meshes) {
    if (!isVisible(frame_mesh.world_bounds, view_projection)) {
      ++parSnapshot.culled_entities;
      continue;
    }
    ++parSnapshot.visible_entities;
    if (parSettings.viewport.mode == ViewportMode::Bounds) {
      continue;
    }

    const glm::vec3 center = frame_mesh.world_bounds.is_valid
                                 ? (frame_mesh.world_bounds.min +
                                    frame_mesh.world_bounds.max) * 0.5f
                                 : frame_mesh.transform.translation;
    visible_meshes.push_back(
        {&frame_mesh, glm::length(center - camera.position)});
    parSnapshot.draw_calls += frame_mesh.mesh->getPrimitiveCount();
    ++parSnapshot.submitted_instances;
    parSnapshot.submitted_triangles += frame_mesh.mesh->getIndexCount() / 3;
  }

  const auto draw_visible = [&](const VisibleMesh& item, MeshDrawPass pass) {
    const FrameMesh& source = *item.source;
    if (!source.mesh->hasPrimitivesForPass(
            1.0f, parSettings.viewport.mode == ViewportMode::Solid, pass)) {
      return;
    }
    m_mesh_renderer.draw(*source.mesh, camera.position, source.model,
                         source.skin_matrices, 1.0f,
                         parSettings.viewport.mode == ViewportMode::Solid, pass);
  };

  const ShadowFrame* shadows = nullptr;
  if (parSettings.viewport.mode == ViewportMode::Material ||
      parSettings.viewport.mode == ViewportMode::Final) {
    thread_local std::vector<ShadowCaster> casters;
    casters.clear();
    casters.reserve(frame_meshes.size());
    for (const FrameMesh& frame_mesh : frame_meshes) {
      casters.push_back(
          {frame_mesh.mesh, frame_mesh.model, frame_mesh.skin_matrices});
    }
    const ShadowRenderSettings shadow_settings =
        parSettings.viewport.mode == ViewportMode::Final
            ? FINAL_SHADOW_SETTINGS
            : MATERIAL_SHADOW_SETTINGS;
    const auto shadow_start = std::chrono::steady_clock::now();
    shadows = &m_shadow_renderer.render(casters, camera, parLighting,
                                         shadow_settings);
    parSnapshot.shadow_render_ms = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - shadow_start).count();
    parSnapshot.shadows_reused = shadows->reused;
    bind_render_target();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
  }

  const auto frame_binding_start = std::chrono::steady_clock::now();
  m_mesh_renderer.beginFrame(
      view_projection, camera.position, parLighting,
      parSettings.viewport.material_debug_mode,
      parSettings.viewport.mode == ViewportMode::Solid, shadows);
  parSnapshot.frame_binding_ms = std::chrono::duration<float, std::milli>(
      std::chrono::steady_clock::now() - frame_binding_start).count();

  const auto material_submission_start = std::chrono::steady_clock::now();
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
  for (const VisibleMesh& item : visible_meshes) {
    draw_visible(item, MeshDrawPass::Opaque);
  }

  std::sort(visible_meshes.begin(), visible_meshes.end(),
            [](const VisibleMesh& left, const VisibleMesh& right) {
              return left.camera_distance > right.camera_distance;
            });
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  for (const VisibleMesh& item : visible_meshes) {
    draw_visible(item, MeshDrawPass::Blend);
  }
  glDepthMask(GL_TRUE);
  parSnapshot.material_submission_ms =
      std::chrono::duration<float, std::milli>(
          std::chrono::steady_clock::now() - material_submission_start)
          .count();

  if (parGhost.kind == PlacementGhost::Kind::StaticAsset) {
    const GpuMesh* mesh =
        parMeshResources.getStaticMesh(parGhost.asset_library_index);
    if (mesh != nullptr) {
      glEnable(GL_BLEND);
      glDepthMask(GL_FALSE);
      m_mesh_renderer.beginFrame(
          view_projection, camera.position, parLighting,
          parSettings.viewport.material_debug_mode,
          parSettings.viewport.mode == ViewportMode::Solid);
      m_mesh_renderer.draw(*mesh, camera.position,
                           parGhost.transform.toMatrix(), {}, parGhost.opacity,
                           parSettings.viewport.mode == ViewportMode::Solid);
      glDepthMask(GL_TRUE);
    }
  }

  const scene::EntityRecord* selected_entity =
      parScene.world.findEntity(parView.use_world_selection
                                    ? parScene.selected_entity
                                    : parView.selected_entity);
  if (parSettings.viewport.show_overlays && selected_entity != nullptr &&
      selected_entity->static_mesh.has_value() &&
      selected_entity->static_mesh->visible) {
    const GpuMesh* mesh =
        parMeshResources.getStaticMesh(
            selected_entity->static_mesh->asset_library_index);
    if (mesh != nullptr) {
      const math::Transform selected_transform =
          viewportEntityTransform(*selected_entity, parView.film_state);
      const math::Bounds3 selected_bounds =
          viewportEntityBounds(*selected_entity, selected_transform);
      const float outline_thickness =
          std::clamp(std::max({selected_bounds.getSize().x,
                               selected_bounds.getSize().y,
                               selected_bounds.getSize().z, 1.0f}) *
                         0.0016f,
                     0.0018f, 0.009f);
      m_mesh_renderer.drawOutline(
          *mesh, view_projection, selected_transform.toMatrix(),
          find_skin_matrices(*selected_entity),
          glm::vec4(1.0f), outline_thickness);
    }
  }

  if (parSettings.viewport.show_overlays) {
  m_grid_line_vertices.clear();
  m_grid_line_vertices.reserve(512);
  m_line_vertices.clear();
  m_line_vertices.reserve(1024);
  m_solid_vertices.clear();
  m_solid_vertices.reserve(512);
  m_glow_vertices.clear();
  m_glow_vertices.reserve(256);
  addSunOverlay(m_line_vertices, m_solid_vertices, m_glow_vertices, camera,
                parViewportSize, parLighting.sun);
  for (const kage::film::ResolvedMovementSegment& movement_path :
       parView.movement_paths) {
    addMovementPathOverlay(m_line_vertices, m_solid_vertices,
                           movement_path, camera, parViewportSize);
  }
  if (selected_entity != nullptr &&
      !parSettings.viewport.show_world_edit_gizmos &&
      !selected_entity->static_mesh.has_value()) {
    const math::Transform selected_transform =
        viewportEntityTransform(*selected_entity, parView.film_state);
    const float marker_radius = std::clamp(
        camera::getWorldLengthForPixels(camera,
                                        selected_transform.translation,
                                        parViewportSize, 18.0f),
        0.05f, 2.0f);
    addOriginCore(m_line_vertices, selected_transform.translation,
                  marker_radius);
  }
  if (parSettings.viewport.floor_grid_visible) {
    addFloorGrid(m_grid_line_vertices, camera.position,
                 parSettings.viewport.floor_grid_radius);
  }
  if (parGhost.kind == PlacementGhost::Kind::StaticAsset &&
      parMeshResources.getStaticMesh(parGhost.asset_library_index) == nullptr) {
    addCube(m_solid_vertices,
            parGhost.transform.translation + glm::vec3(0.0f, 0.5f, 0.0f),
            1.0f, glm::vec4(0.55f, 0.68f, 0.82f, parGhost.opacity));
  }
  for (const scene::EntityRecord& entity : parScene.world.getEntities()) {
    if (!entity.alive) {
      continue;
    }
    if (parSettings.viewport.mode == ViewportMode::Bounds &&
        entity.static_mesh.has_value() && entity.static_mesh->visible) {
      const math::Bounds3 bounds =
          viewportEntityBounds(entity, entity.transform.transform);
      if (bounds.is_valid &&
          isVisible(bounds, view_projection)) {
        const glm::vec3 center = (bounds.min + bounds.max) * 0.5f;
        const glm::vec3 size = bounds.getSize();
        addCube(m_solid_vertices, center,
                std::max({size.x, size.y, size.z, 0.05f}),
                glm::vec4(0.42f, 0.68f, 0.92f, 0.16f));
      }
    }
    if (entity.static_mesh.has_value() && entity.static_mesh->visible &&
        parMeshResources.getStaticMesh(
            entity.static_mesh->asset_library_index) == nullptr) {
      const math::Bounds3 bounds =
          viewportEntityBounds(entity, entity.transform.transform);
      const glm::vec3 center =
          bounds.is_valid ? (bounds.min + bounds.max) * 0.5f
                          : entity.transform.transform.translation;
      const glm::vec3 size =
          bounds.is_valid ? bounds.getSize() : glm::vec3(1.0f);
      addCube(m_solid_vertices, center,
              std::max({size.x, size.y, size.z, 1.0f}),
              glm::vec4(0.55f, 0.68f, 0.82f, 0.30f));
    }
    if (entity.light.has_value()) {
      addLightGizmo(m_line_vertices, m_solid_vertices,
                    viewportEntityTransform(entity, parView.film_state),
                    *entity.light,
                    camera.getRight(), camera.getUp());
    }
    if (entity.camera.has_value()) {
      addCameraGizmo(m_line_vertices,
                     viewportEntityTransform(entity, parView.film_state));
    }
  }
  if (parGhost.kind == PlacementGhost::Kind::PointLight) {
    scene::LightComponent light;
    light.color = parGhost.light_color;
    light.intensity = parGhost.light_intensity;
    addLightGizmo(m_line_vertices, m_solid_vertices, parGhost.transform,
                  light, camera.getRight(), camera.getUp());
  }
  if (parGhost.kind == PlacementGhost::Kind::Camera) {
    addCameraGizmo(m_line_vertices, parGhost.transform);
  }

  if (selected_entity != nullptr &&
      parSettings.viewport.show_world_edit_gizmos) {
    const math::Bounds3 world_bounds = viewportEntityBounds(
        *selected_entity, selected_entity->transform.transform);
    addFloorContactCue(m_grid_line_vertices, world_bounds,
                       selected_entity->transform.transform.translation);
    const float axis_length = getGizmoLength(
        camera, parViewportSize, *selected_entity, world_bounds);
    addTransformAxes(m_line_vertices, m_solid_vertices,
                     selected_entity->transform.transform, axis_length,
                     parSettings.viewport.gizmo_axis_space);
    if (!selected_entity->static_mesh.has_value()) {
      addOriginCore(m_line_vertices,
                    selected_entity->transform.transform.translation,
                    axis_length * 0.07f);
    }
    if (selected_entity->light.has_value()) {
      const std::optional<glm::vec3> nearest_mesh = findNearestMeshCenter(
          parScene, selected_entity->transform.transform.translation);
      if (nearest_mesh) {
        const glm::vec3 color =
            glm::clamp(selected_entity->light->color, glm::vec3(0.0f),
                       glm::vec3(1.0f));
        addLine(m_line_vertices,
                selected_entity->transform.transform.translation,
                *nearest_mesh, color);
      }
    }
  }
  if (parGizmoGuide.active) {
    const glm::vec3 axis = glm::normalize(parGizmoGuide.axis);
    addLine(m_line_vertices,
            parGizmoGuide.origin - axis * parGizmoGuide.half_length,
            parGizmoGuide.origin + axis * parGizmoGuide.half_length,
            parGizmoGuide.color);
  }

  // The editor grid is a depth-tested overlay. It never writes depth and is
  // omitted entirely from film output by the overlay gate above.
  glDepthMask(GL_FALSE);
  m_debug_renderer.drawLines(m_grid_line_vertices, view_projection);
  glDepthMask(GL_TRUE);
  glDisable(GL_DEPTH_TEST);
  m_debug_renderer.drawTriangles(m_solid_vertices, view_projection);
  m_debug_renderer.drawLines(m_line_vertices, view_projection);
  if (!m_glow_vertices.empty()) {
    glDepthMask(GL_FALSE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    m_debug_renderer.drawTriangles(m_glow_vertices, view_projection);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
  }
  glEnable(GL_DEPTH_TEST);
  }
  if (parView.use_film_framebuffer) {
    m_film_framebuffer.present();
  }
  finish_gpu_timer();
}

std::optional<scene::EntityId> WorldRenderer::pickEntity(
    const scene::SceneManager::SceneRecord& parScene,
    const MeshResourceCache& parMeshResources,
    const camera::Camera& parCamera,
    const glm::vec2& parCursorPixel, const glm::vec2& parViewportSize,
    const film::FilmFrameState* parFilmState) {
  if (m_pick_framebuffer == 0) {
    glGenFramebuffers(1, &m_pick_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_pick_framebuffer);
    glGenTextures(1, &m_pick_texture);
    glBindTexture(GL_TEXTURE_2D, m_pick_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, 1, 1, 0, GL_RED_INTEGER,
                 GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_pick_texture, 0);
    glGenRenderbuffers(1, &m_pick_depth);
    glBindRenderbuffer(GL_RENDERBUFFER, m_pick_depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 1, 1);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, m_pick_depth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      return std::nullopt;
    }
  } else {
    glBindFramebuffer(GL_FRAMEBUFFER, m_pick_framebuffer);
  }

  constexpr GLuint NO_ENTITY = std::numeric_limits<GLuint>::max();
  const GLuint clear_value = NO_ENTITY;
  glViewport(0, 0, 1, 1);
  glClearBufferuiv(GL_COLOR, 0, &clear_value);
  glClear(GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);

  const glm::vec2 viewport = glm::max(parViewportSize, glm::vec2(1.0f));
  const glm::vec2 ndc(
      (2.0f * parCursorPixel.x / viewport.x) - 1.0f,
      1.0f - (2.0f * parCursorPixel.y / viewport.y));
  glm::mat4 pick_transform(1.0f);
  pick_transform[0][0] = viewport.x;
  pick_transform[1][1] = viewport.y;
  pick_transform[3][0] = -ndc.x * viewport.x;
  pick_transform[3][1] = -ndc.y * viewport.y;
  const glm::mat4 pick_view_projection =
      pick_transform *
      parCamera.getViewProjectionMatrix(viewport);

  for (const scene::EntityRecord& entity : parScene.world.getEntities()) {
    if (!entity.alive || !entity.static_mesh.has_value() ||
        !entity.static_mesh->visible) {
      continue;
    }
    const GpuMesh* mesh =
        parMeshResources.getStaticMesh(entity.static_mesh->asset_library_index);
    if (mesh == nullptr) {
      continue;
    }
    std::span<const std::vector<glm::mat4>> skin_matrices;
    if (entity.rig.has_value()) {
      skin_matrices = entity.rig->primitive_skin_matrices;
    }
    m_mesh_renderer.drawPicking(
        *mesh, pick_view_projection,
        viewportEntityTransform(entity, parFilmState).toMatrix(),
        skin_matrices, entity.id.value);
  }

  GLuint entity_id = NO_ENTITY;
  glReadPixels(0, 0, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &entity_id);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, static_cast<GLsizei>(viewport.x),
             static_cast<GLsizei>(viewport.y));
  glEnable(GL_BLEND);
  return entity_id == NO_ENTITY
             ? std::nullopt
             : std::optional<scene::EntityId>{scene::EntityId{entity_id}};
}

glm::vec3 WorldRenderer::getClearColor(SkyPreset parPreset) {
  switch (parPreset) {
    case SkyPreset::ClearDay:
      return glm::vec3(0.50f, 0.60f, 0.70f);
    case SkyPreset::MountainDawn:
      return glm::vec3(0.40f, 0.43f, 0.47f);
    case SkyPreset::WarmDusk:
      return glm::vec3(0.45f, 0.40f, 0.36f);
    case SkyPreset::DarkStudio:
      return glm::vec3(0.025f, 0.035f, 0.055f);
    case SkyPreset::DarkVoid:
      return glm::vec3(0.0f);
  }

  return glm::vec3(0.025f, 0.035f, 0.055f);
}

}  // namespace kage::render
