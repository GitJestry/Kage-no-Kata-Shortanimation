#pragma once

#include "lighting/light.hpp"
#include "render/gpu_mesh.hpp"
#include "render/shadow_renderer.hpp"
#include "render/shader_program.hpp"
#include "render/texture_2d.hpp"

#include <glm/glm.hpp>

#include <span>
#include <vector>

namespace kage::render {

class MeshRenderer final {
 public:
  MeshRenderer();
  ~MeshRenderer();

  MeshRenderer(const MeshRenderer&) = delete;
  MeshRenderer& operator=(const MeshRenderer&) = delete;

  MeshRenderer(MeshRenderer&&) noexcept = delete;
  MeshRenderer& operator=(MeshRenderer&&) noexcept = delete;

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
  void createSunShadowFallback();
  void createPointShadowFallback();

  ShaderProgram m_shader;
  ShaderProgram m_outline_shader;
  ShaderProgram m_picking_shader;
  Texture2D m_sun_shadow_fallback;
  GLuint m_point_shadow_fallback = 0;
};

}  // namespace kage::render
