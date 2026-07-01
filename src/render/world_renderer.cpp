#include "render/world_renderer.hpp"

#include <glad/gl.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace {

constexpr glm::vec3 LIGHT_LOCAL_FORWARD{0.0f, 0.0f, -1.0f};
constexpr glm::vec3 GRID_COLOR{0.115f, 0.12f, 0.13f};
constexpr glm::vec3 GRID_AXIS_X_COLOR{0.34f, 0.12f, 0.12f};
constexpr glm::vec3 GRID_AXIS_Z_COLOR{0.12f, 0.28f, 0.14f};
constexpr glm::vec4 FLOOR_PLANE_COLOR{0.035f, 0.038f, 0.042f, 0.56f};
constexpr glm::vec3 SELECTED_CONTACT_COLOR{1.0f, 1.0f, 1.0f};
constexpr glm::vec3 FLOOR_INTERSECTION_COLOR{1.0f, 0.18f, 0.12f};
constexpr glm::vec3 ROTATION_COLOR{1.0f, 0.18f, 0.14f};
constexpr glm::vec3 CAMERA_GIZMO_COLOR{0.40f, 0.74f, 1.0f};
constexpr glm::vec4 AXIS_X_FILL{0.18f, 0.82f, 0.28f, 0.92f};
constexpr glm::vec4 AXIS_Y_FILL{1.0f, 0.86f, 0.22f, 0.92f};
constexpr glm::vec4 AXIS_Z_FILL{0.22f, 0.48f, 1.0f, 0.92f};
constexpr glm::vec4 ROTATION_FILL{1.0f, 0.18f, 0.14f, 0.70f};
constexpr float ENTITY_HANDLE_EXTENT = 0.35f;
constexpr int GRID_LINE_CAP = 240;

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

[[nodiscard]] kage::math::Bounds3 getEntityWorldBounds(
    const kage::scene::EntityRecord& parEntity) {
  if (parEntity.static_mesh.has_value()) {
    return transformBounds(parEntity.static_mesh->local_bounds,
                           parEntity.transform.transform.toMatrix());
  }

  return makePointBounds(parEntity.transform.transform.translation,
                         ENTITY_HANDLE_EXTENT);
}

void addLine(std::vector<kage::render::LineVertex>& parVertices,
             const glm::vec3& parStart, const glm::vec3& parEnd,
             const glm::vec3& parColor) {
  parVertices.push_back({parStart, parColor});
  parVertices.push_back({parEnd, parColor});
}

void addTriangle(std::vector<kage::render::SolidGizmoVertex>& parVertices,
                 const glm::vec3& parA, const glm::vec3& parB,
                 const glm::vec3& parC, const glm::vec4& parColor) {
  parVertices.push_back({parA, parColor});
  parVertices.push_back({parB, parColor});
  parVertices.push_back({parC, parColor});
}

[[nodiscard]] glm::vec3 getPerpendicularHelper(const glm::vec3& parDirection) {
  return std::abs(glm::dot(parDirection, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.92f
             ? glm::vec3(1.0f, 0.0f, 0.0f)
             : glm::vec3(0.0f, 1.0f, 0.0f);
}

void addDisk(std::vector<kage::render::SolidGizmoVertex>& parVertices,
             const glm::vec3& parCenter, const glm::vec3& parAxisA,
             const glm::vec3& parAxisB, float parRadius,
             const glm::vec4& parColor) {
  constexpr int SEGMENT_COUNT = 32;
  glm::vec3 previous = parCenter + parAxisA * parRadius;
  for (int segment = 1; segment <= SEGMENT_COUNT; ++segment) {
    const float angle =
        (static_cast<float>(segment) / static_cast<float>(SEGMENT_COUNT)) *
        glm::two_pi<float>();
    const glm::vec3 current =
        parCenter + (std::cos(angle) * parAxisA +
                     std::sin(angle) * parAxisB) *
                        parRadius;
    addTriangle(parVertices, parCenter, previous, current, parColor);
    previous = current;
  }
}

void addCylinder(std::vector<kage::render::SolidGizmoVertex>& parVertices,
                 const glm::vec3& parStart, const glm::vec3& parEnd,
                 float parRadius, const glm::vec4& parColor) {
  constexpr int SEGMENT_COUNT = 12;
  const glm::vec3 direction = glm::normalize(parEnd - parStart);
  const glm::vec3 side =
      glm::normalize(glm::cross(direction, getPerpendicularHelper(direction)));
  const glm::vec3 up = glm::normalize(glm::cross(side, direction));
  for (int segment = 0; segment < SEGMENT_COUNT; ++segment) {
    const float angle_a =
        (static_cast<float>(segment) / static_cast<float>(SEGMENT_COUNT)) *
        glm::two_pi<float>();
    const float angle_b =
        (static_cast<float>(segment + 1) /
         static_cast<float>(SEGMENT_COUNT)) *
        glm::two_pi<float>();
    const glm::vec3 offset_a =
        (std::cos(angle_a) * side + std::sin(angle_a) * up) * parRadius;
    const glm::vec3 offset_b =
        (std::cos(angle_b) * side + std::sin(angle_b) * up) * parRadius;
    const glm::vec3 start_a = parStart + offset_a;
    const glm::vec3 start_b = parStart + offset_b;
    const glm::vec3 end_a = parEnd + offset_a;
    const glm::vec3 end_b = parEnd + offset_b;
    addTriangle(parVertices, start_a, end_a, end_b, parColor);
    addTriangle(parVertices, start_a, end_b, start_b, parColor);
  }
}

void addCone(std::vector<kage::render::SolidGizmoVertex>& parVertices,
             const glm::vec3& parTip, const glm::vec3& parDirection,
             float parLength, float parRadius, const glm::vec4& parColor) {
  constexpr int SEGMENT_COUNT = 16;
  const glm::vec3 direction = glm::normalize(parDirection);
  const glm::vec3 base = parTip - direction * parLength;
  const glm::vec3 side =
      glm::normalize(glm::cross(direction, getPerpendicularHelper(direction)));
  const glm::vec3 up = glm::normalize(glm::cross(side, direction));
  glm::vec3 previous = base + side * parRadius;
  for (int segment = 1; segment <= SEGMENT_COUNT; ++segment) {
    const float angle =
        (static_cast<float>(segment) / static_cast<float>(SEGMENT_COUNT)) *
        glm::two_pi<float>();
    const glm::vec3 current =
        base + (std::cos(angle) * side + std::sin(angle) * up) * parRadius;
    addTriangle(parVertices, parTip, previous, current, parColor);
    addTriangle(parVertices, base, current, previous, parColor);
    previous = current;
  }
}

void addSolidSphere(std::vector<kage::render::SolidGizmoVertex>& parVertices,
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

void addCircle(std::vector<kage::render::LineVertex>& parVertices,
               const glm::vec3& parCenter, const glm::vec3& parAxisA,
               const glm::vec3& parAxisB, float parRadius,
               const glm::vec3& parColor) {
  constexpr int SEGMENT_COUNT = 48;
  glm::vec3 previous = parCenter + parAxisA * parRadius;
  for (int segment = 1; segment <= SEGMENT_COUNT; ++segment) {
    const float angle =
        (static_cast<float>(segment) / static_cast<float>(SEGMENT_COUNT)) *
        glm::two_pi<float>();
    const glm::vec3 current =
        parCenter + (std::cos(angle) * parAxisA +
                     std::sin(angle) * parAxisB) *
                        parRadius;
    addLine(parVertices, previous, current, parColor);
    previous = current;
  }
}

void addFloorGrid(std::vector<kage::render::LineVertex>& parVertices,
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

void addFloorPlane(std::vector<kage::render::SolidGizmoVertex>& parVertices,
                   const glm::vec3& parCameraPosition, int parRadius) {
  const int radius = std::clamp(parRadius, 8, GRID_LINE_CAP / 2);
  const int center_x = static_cast<int>(std::floor(parCameraPosition.x));
  const int center_z = static_cast<int>(std::floor(parCameraPosition.z));
  const float min_x = static_cast<float>(center_x - radius);
  const float max_x = static_cast<float>(center_x + radius);
  const float min_z = static_cast<float>(center_z - radius);
  const float max_z = static_cast<float>(center_z + radius);
  constexpr float FLOOR_Y = 0.0f;

  const glm::vec3 a(min_x, FLOOR_Y, min_z);
  const glm::vec3 b(max_x, FLOOR_Y, min_z);
  const glm::vec3 c(max_x, FLOOR_Y, max_z);
  const glm::vec3 d(min_x, FLOOR_Y, max_z);
  addTriangle(parVertices, a, b, c, FLOOR_PLANE_COLOR);
  addTriangle(parVertices, a, c, d, FLOOR_PLANE_COLOR);
}

void addFloorContactCue(std::vector<kage::render::LineVertex>& parVertices,
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
  const glm::vec3 min(parBounds.min.x, 0.004f, parBounds.min.z);
  const glm::vec3 max(parBounds.max.x, 0.004f, parBounds.max.z);
  const std::array<glm::vec3, 4> footprint = {
      glm::vec3(min.x, 0.004f, min.z),
      glm::vec3(max.x, 0.004f, min.z),
      glm::vec3(max.x, 0.004f, max.z),
      glm::vec3(min.x, 0.004f, max.z),
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

void addTransformAxes(std::vector<kage::render::LineVertex>& parVertices,
                      std::vector<kage::render::SolidGizmoVertex>& parSolid,
                      const kage::math::Transform& parTransform,
                      float parLength,
                      kage::render::GizmoAxisSpace parAxisSpace) {
  const glm::vec3 position = parTransform.translation;
  const glm::vec3 right =
      parAxisSpace == kage::render::GizmoAxisSpace::World
          ? glm::vec3(1.0f, 0.0f, 0.0f)
          : parTransform.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
  const glm::vec3 up =
      parAxisSpace == kage::render::GizmoAxisSpace::World
          ? glm::vec3(0.0f, 1.0f, 0.0f)
          : parTransform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
  const glm::vec3 forward =
      parAxisSpace == kage::render::GizmoAxisSpace::World
          ? glm::vec3(0.0f, 0.0f, 1.0f)
          : parTransform.rotation * glm::vec3(0.0f, 0.0f, 1.0f);
  const float shaft_radius = std::max(parLength * 0.012f, 0.01f);
  const float head_length = parLength * 0.16f;
  const float head_radius = parLength * 0.04f;
  addCylinder(parSolid, position, position + right * (parLength - head_length),
              shaft_radius, AXIS_X_FILL);
  addCylinder(parSolid, position, position + up * (parLength - head_length),
              shaft_radius, AXIS_Y_FILL);
  addCylinder(parSolid, position, position + forward * (parLength - head_length),
              shaft_radius, AXIS_Z_FILL);
  addCone(parSolid, position + right * parLength, right, head_length,
          head_radius, AXIS_X_FILL);
  addCone(parSolid, position + up * parLength, up, head_length, head_radius,
          AXIS_Y_FILL);
  addCone(parSolid, position + forward * parLength, forward, head_length,
          head_radius, AXIS_Z_FILL);
  addSolidSphere(parSolid, position, parLength * 0.055f, ROTATION_FILL);
  addCircle(parVertices, position, right, forward, parLength * 0.24f,
            ROTATION_COLOR);
}

void addOriginCore(std::vector<kage::render::LineVertex>& parVertices,
                   const glm::vec3& parPosition, float parRadius) {
  addCircle(parVertices, parPosition, glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f), parRadius,
            SELECTED_CONTACT_COLOR);
  addCircle(parVertices, parPosition, glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f), parRadius,
            SELECTED_CONTACT_COLOR);
  addCircle(parVertices, parPosition, glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f), parRadius,
            SELECTED_CONTACT_COLOR);
}

void addLightGizmo(std::vector<kage::render::LineVertex>& parVertices,
                   std::vector<kage::render::SolidGizmoVertex>& parSolid,
                   std::vector<kage::render::SolidGizmoVertex>& parGlow,
                   const kage::math::Transform& parTransform,
                   const kage::scene::PointLightComponent& parLight,
                   const glm::vec3& parCameraRight,
                   const glm::vec3& parCameraUp) {
  const glm::vec3 position = parTransform.translation;
  const glm::vec3 color =
      glm::clamp(parLight.color * (0.45f + parLight.intensity * 0.24f),
                 glm::vec3(0.0f), glm::vec3(1.0f));
  const float radius = 0.16f + std::min(parLight.intensity, 8.0f) * 0.045f;
  addSolidSphere(parSolid, position, radius, glm::vec4(color, 0.95f));
  addDisk(parGlow, position, parCameraRight, parCameraUp, radius * 2.8f,
          glm::vec4(color, 0.18f));
  addDisk(parGlow, position, parCameraRight, parCameraUp, radius * 5.0f,
          glm::vec4(color, 0.08f));
  addCircle(parVertices, position, parCameraRight, parCameraUp,
            std::max(parLight.range, radius * 2.0f), color * 0.42f);
}

void addSunGizmo(std::vector<kage::render::LineVertex>& parVertices,
                 const kage::math::Transform& parTransform,
                 const kage::scene::DirectionalLightComponent& parLight) {
  const glm::vec3 position = parTransform.translation;
  const glm::vec3 right = parTransform.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
  const glm::vec3 up = parTransform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
  const glm::vec3 forward = parTransform.rotation * LIGHT_LOCAL_FORWARD;
  const glm::vec3 color =
      glm::clamp(parLight.color * (0.45f + parLight.intensity * 0.24f),
                 glm::vec3(0.0f), glm::vec3(1.0f));
  constexpr float RADIUS = 0.26f;
  addCircle(parVertices, position, right, up, RADIUS, color);
  addCircle(parVertices, position, right, forward, RADIUS, color);
  addCircle(parVertices, position, up, forward, RADIUS, color);
  addLine(parVertices, position, position + forward * 1.6f, color);
}

void addCameraGizmo(std::vector<kage::render::LineVertex>& parVertices,
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
  addLine(parVertices, corners[0], corners[1], CAMERA_GIZMO_COLOR);
  addLine(parVertices, corners[1], corners[2], CAMERA_GIZMO_COLOR);
  addLine(parVertices, corners[2], corners[3], CAMERA_GIZMO_COLOR);
  addLine(parVertices, corners[3], corners[0], CAMERA_GIZMO_COLOR);
}

}  // namespace

namespace kage::render {

void WorldRenderer::render(const scene::SceneManager::SceneRecord& parScene,
                           const MeshResourceCache& parMeshResources,
                           const camera::CameraSystem& parCameraSystem,
                           const lighting::LightingState& parLighting,
                           const PlacementGhost& parGhost,
                           const EditorRenderSettings& parSettings,
                           const glm::vec2& parViewportSize) {
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  const glm::vec3 clear_color = getClearColor(parSettings.sky_preset);
  glClearColor(clear_color.r, clear_color.g, clear_color.b, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  const camera::Camera& camera = parCameraSystem.getCamera();
  const glm::mat4 view_projection =
      camera.getViewProjectionMatrix(parViewportSize);

  for (const scene::EntityRecord& entity : parScene.world.getEntities()) {
    if (!entity.alive || !entity.static_mesh.has_value() ||
        !entity.static_mesh->visible) {
      continue;
    }

    const GpuMesh* mesh =
        parMeshResources.getStaticMesh(entity.static_mesh->mesh_handle);
    if (mesh == nullptr) {
      continue;
    }

    m_static_mesh_renderer.draw(*mesh, view_projection,
                                camera.position,
                                entity.transform.transform.toMatrix(),
                                parLighting, entity.static_mesh->opacity,
                                parSettings.material_debug_mode);
  }

  if (parGhost.kind == PlacementGhost::Kind::StaticAsset) {
    const GpuMesh* mesh = parMeshResources.getStaticMesh(parGhost.mesh_handle);
    if (mesh != nullptr) {
      m_static_mesh_renderer.draw(*mesh, view_projection,
                                  camera.position,
                                  parGhost.transform.toMatrix(),
                                  parLighting, parGhost.opacity,
                                  parSettings.material_debug_mode);
    }
  }

  const scene::EntityRecord* selected_entity =
      parScene.world.findEntity(parScene.selected_entity);
  if (selected_entity != nullptr && selected_entity->static_mesh.has_value() &&
      selected_entity->static_mesh->visible) {
    const GpuMesh* mesh =
        parMeshResources.getStaticMesh(selected_entity->static_mesh->mesh_handle);
    if (mesh != nullptr) {
      const math::Bounds3 selected_bounds = getEntityWorldBounds(*selected_entity);
      const float outline_thickness =
          std::clamp(std::max({selected_bounds.getSize().x,
                               selected_bounds.getSize().y,
                               selected_bounds.getSize().z, 1.0f}) *
                         0.004f,
                     0.004f, 0.026f);
      m_static_mesh_renderer.drawOutline(
          *mesh, view_projection,
          selected_entity->transform.transform.toMatrix(),
          glm::vec4(1.0f), outline_thickness);
    }
  }

  m_floor_vertices.clear();
  m_floor_vertices.reserve(6);
  m_grid_line_vertices.clear();
  m_grid_line_vertices.reserve(512);
  m_line_vertices.clear();
  m_line_vertices.reserve(1024);
  m_solid_vertices.clear();
  m_solid_vertices.reserve(512);
  m_glow_vertices.clear();
  m_glow_vertices.reserve(256);
  if (parSettings.floor_grid_visible) {
    addFloorPlane(m_floor_vertices, camera.position,
                  parSettings.floor_grid_radius);
    addFloorGrid(m_grid_line_vertices, camera.position,
                 parSettings.floor_grid_radius);
  }
  for (const scene::EntityRecord& entity : parScene.world.getEntities()) {
    if (!entity.alive) {
      continue;
    }
    if (entity.directional_light.has_value()) {
      addSunGizmo(m_line_vertices, entity.transform.transform,
                  *entity.directional_light);
    }
    if (entity.point_light.has_value()) {
      addLightGizmo(m_line_vertices, m_solid_vertices, m_glow_vertices,
                    entity.transform.transform, *entity.point_light,
                    camera.getRight(), camera.getUp());
    }
    if (entity.camera.has_value()) {
      addCameraGizmo(m_line_vertices, entity.transform.transform);
    }
  }
  if (parGhost.kind == PlacementGhost::Kind::PointLight) {
    scene::PointLightComponent light;
    light.color = parGhost.light_color;
    light.intensity = parGhost.light_intensity;
    addLightGizmo(m_line_vertices, m_solid_vertices, m_glow_vertices,
                  parGhost.transform, light, camera.getRight(),
                  camera.getUp());
  }
  if (parGhost.kind == PlacementGhost::Kind::Camera) {
    addCameraGizmo(m_line_vertices, parGhost.transform);
  }

  if (selected_entity != nullptr) {
    const math::Bounds3 world_bounds = getEntityWorldBounds(*selected_entity);
    addFloorContactCue(m_grid_line_vertices, world_bounds,
                       selected_entity->transform.transform.translation);
    const float axis_length =
        std::max({world_bounds.getSize().x, world_bounds.getSize().y,
                  world_bounds.getSize().z, 1.0f}) *
        0.27f;
    addTransformAxes(m_line_vertices, m_solid_vertices,
                     selected_entity->transform.transform, axis_length,
                     parSettings.gizmo_axis_space);
    if (!selected_entity->static_mesh.has_value()) {
      addOriginCore(m_line_vertices,
                    selected_entity->transform.transform.translation,
                    axis_length * 0.07f);
    }
  }

  if (!m_floor_vertices.empty()) {
    glDepthMask(GL_FALSE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    m_solid_gizmo_renderer.draw(m_floor_vertices, view_projection);
    glDepthMask(GL_TRUE);
  }
  m_line_renderer.draw(m_grid_line_vertices, view_projection);
  glDisable(GL_DEPTH_TEST);
  m_solid_gizmo_renderer.draw(m_solid_vertices, view_projection);
  m_line_renderer.draw(m_line_vertices, view_projection);
  if (!m_glow_vertices.empty()) {
    glDepthMask(GL_FALSE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    m_solid_gizmo_renderer.draw(m_glow_vertices, view_projection);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
  }
  glEnable(GL_DEPTH_TEST);
}

const char* WorldRenderer::getSkyPresetName(SkyPreset parPreset) {
  switch (parPreset) {
    case SkyPreset::ClearDay:
      return "Clear day";
    case SkyPreset::MountainDawn:
      return "Mountain dawn";
    case SkyPreset::WarmDusk:
      return "Warm dusk";
    case SkyPreset::DarkStudio:
      return "Dark studio";
  }

  return "Unknown";
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
  }

  return glm::vec3(0.025f, 0.035f, 0.055f);
}

}  // namespace kage::render
