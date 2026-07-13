#pragma once

#include "camera/camera.hpp"
#include "lighting/light.hpp"
#include "render/gpu_mesh.hpp"
#include "render/shader_program.hpp"

#include <array>
#include <span>

namespace kage::render {

struct ShadowCaster final {
  const GpuMesh* mesh = nullptr;
  glm::mat4 model{1.0f};
  std::span<const std::vector<glm::mat4>> skin_matrices;
};

struct ShadowFrame final {
  bool sun_enabled = false;
  bool reused = false;
  glm::mat4 sun_view_projection{1.0f};
  GLuint sun_depth = 0;
  std::array<GLuint, lighting::MAX_POINT_SHADOWS> point_depth{};
  std::array<std::uint32_t, lighting::MAX_POINT_SHADOWS> point_entity_ids{};
  std::size_t point_count = 0;
};

class ShadowRenderer final {
 public:
  ShadowRenderer();
  ~ShadowRenderer();

  ShadowRenderer(const ShadowRenderer&) = delete;
  ShadowRenderer& operator=(const ShadowRenderer&) = delete;

  [[nodiscard]] const ShadowFrame& render(
      std::span<const ShadowCaster> parCasters, const camera::Camera& parCamera,
      const lighting::LightingState& parLighting, int parSunResolution,
      bool parRenderPointShadows);

 private:
  [[nodiscard]] std::size_t getInputHash(
      std::span<const ShadowCaster> parCasters, const camera::Camera& parCamera,
      const lighting::LightingState& parLighting, int parSunResolution,
      bool parRenderPointShadows) const;
  void createResources();
  void resizeSunDepth(int parResolution);
  void drawCasters(std::span<const ShadowCaster> parCasters,
                   const glm::mat4& parViewProjection,
                   const glm::vec3* parPointPosition = nullptr,
                   float parPointRange = 1.0f);

  ShaderProgram m_shader;
  GLuint m_framebuffer = 0;
  ShadowFrame m_frame;
  int m_sun_resolution = 0;
  std::size_t m_last_input_hash = 0;
  bool m_has_cached_frame = false;
};

}  // namespace kage::render
