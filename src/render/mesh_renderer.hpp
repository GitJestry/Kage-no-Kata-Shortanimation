#pragma once

#include "lighting/light.hpp"
#include "render/gpu_mesh.hpp"
#include "render/shader_program.hpp"

#include <glm/glm.hpp>

#include <span>
#include <vector>

namespace kage::render {

class MeshRenderer final {
 public:
  MeshRenderer();

  MeshRenderer(const MeshRenderer&) = delete;
  MeshRenderer& operator=(const MeshRenderer&) = delete;

  MeshRenderer(MeshRenderer&&) noexcept = default;
  MeshRenderer& operator=(MeshRenderer&&) noexcept = default;

  void draw(const GpuMesh& parMesh,
            const glm::mat4& parViewProjection,
            const glm::vec3& parCameraPosition,
            const glm::mat4& parEntityTransform,
            const lighting::LightingState& parLighting,
            std::span<const std::vector<glm::mat4>> parSkinMatrices,
            float parEntityOpacity,
            MaterialDebugMode parDebugMode) const;
  void drawOutline(const GpuMesh& parMesh,
                   const glm::mat4& parViewProjection,
                   const glm::mat4& parEntityTransform,
                   std::span<const std::vector<glm::mat4>> parSkinMatrices,
                   const glm::vec4& parColor,
                   float parThickness) const;

 private:
  ShaderProgram m_shader;
  ShaderProgram m_outline_shader;
};

}  // namespace kage::render
