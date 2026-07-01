#pragma once

#include "lighting/light.hpp"
#include "render/gpu_mesh.hpp"
#include "render/shader_program.hpp"

#include <glm/glm.hpp>

namespace kage::render {

class StaticMeshRenderer final {
 public:
  StaticMeshRenderer();

  StaticMeshRenderer(const StaticMeshRenderer&) = delete;
  StaticMeshRenderer& operator=(const StaticMeshRenderer&) = delete;

  StaticMeshRenderer(StaticMeshRenderer&&) noexcept = default;
  StaticMeshRenderer& operator=(StaticMeshRenderer&&) noexcept = default;

  void draw(const GpuMesh& parMesh,
            const glm::mat4& parViewProjection,
            const glm::vec3& parCameraPosition,
            const glm::mat4& parEntityTransform,
            const lighting::LightingState& parLighting,
            float parEntityOpacity,
            MaterialDebugMode parDebugMode) const;
  void drawOutline(const GpuMesh& parMesh,
                   const glm::mat4& parViewProjection,
                   const glm::mat4& parEntityTransform,
                   const glm::vec4& parColor,
                   float parThickness) const;

 private:
  ShaderProgram m_shader;
  ShaderProgram m_outline_shader;
};

}  // namespace kage::render
