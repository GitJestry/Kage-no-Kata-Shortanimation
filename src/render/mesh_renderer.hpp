#pragma once

#include "lighting/light.hpp"
#include "render/gpu_mesh.hpp"
#include "render/shadow_renderer.hpp"
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

  void beginFrame(const glm::mat4& parViewProjection,
                  const glm::vec3& parCameraPosition,
                  const lighting::LightingState& parLighting,
                  MaterialDebugMode parDebugMode, bool parSolidMode,
                  const ShadowFrame* parShadows = nullptr) const;
  void draw(const GpuMesh& parMesh,
            const glm::vec3& parCameraPosition,
            const glm::mat4& parEntityTransform,
            std::span<const std::vector<glm::mat4>> parSkinMatrices,
            float parEntityOpacity,
            bool parSolidMode = false,
            MeshDrawPass parPass = MeshDrawPass::All) const;
  void drawOutline(const GpuMesh& parMesh,
                   const glm::mat4& parViewProjection,
                   const glm::mat4& parEntityTransform,
                   std::span<const std::vector<glm::mat4>> parSkinMatrices,
                   const glm::vec4& parColor,
                   float parThickness) const;
  void drawPicking(const GpuMesh& parMesh,
                   const glm::mat4& parViewProjection,
                   const glm::mat4& parEntityTransform,
                   std::span<const std::vector<glm::mat4>> parSkinMatrices,
                   std::uint32_t parEntityId) const;

 private:
  ShaderProgram m_shader;
  ShaderProgram m_outline_shader;
  ShaderProgram m_picking_shader;
};

}  // namespace kage::render
