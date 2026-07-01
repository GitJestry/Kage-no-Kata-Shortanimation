#pragma once

#include "assets/asset_types.hpp"
#include "lighting/light.hpp"
#include "render/gpu_buffer.hpp"
#include "render/shader_program.hpp"
#include "render/texture_2d.hpp"
#include "render/vertex_array.hpp"

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace kage::render {

enum class MaterialDebugMode {
  Lit,
  BaseColor,
  Normal,
  Roughness,
  Metallic,
  Uv
};

class GpuMesh final {
 public:
  GpuMesh() = default;

  GpuMesh(const GpuMesh&) = delete;
  GpuMesh& operator=(const GpuMesh&) = delete;

  GpuMesh(GpuMesh&&) noexcept = default;
  GpuMesh& operator=(GpuMesh&&) noexcept = default;

  void upload(const assets::StaticModel& parModel);
  void draw(const ShaderProgram& parShader,
            const glm::mat4& parViewProjection,
            const glm::vec3& parCameraPosition,
            const glm::mat4& parEntityTransform,
            const lighting::LightingState& parLighting,
            float parEntityOpacity,
            MaterialDebugMode parDebugMode) const;
  void drawOutline(const ShaderProgram& parShader,
                   const glm::mat4& parViewProjection,
                   const glm::mat4& parEntityTransform,
                   const glm::vec4& parColor,
                   float parThickness) const;
  void clear();

  [[nodiscard]] std::size_t getPrimitiveCount() const;
  [[nodiscard]] std::size_t getIndexCount() const;
  [[nodiscard]] bool isValid() const;

 private:
  struct PrimitiveGpuData final {
    VertexArray vertex_array;
    GpuBuffer vertex_buffer;
    GpuBuffer index_buffer;
    glm::mat4 transform{1.0f};
    glm::vec4 base_color_factor{1.0f};
    assets::MaterialTextureSlot base_color_texture;
    assets::MaterialTextureSlot normal_texture;
    assets::MaterialTextureSlot metallic_roughness_texture;
    assets::MaterialTextureSlot emissive_texture;
    float metallic_factor = 1.0f;
    float roughness_factor = 1.0f;
    float normal_scale = 1.0f;
    float alpha_cutoff = 0.5f;
    glm::vec3 emissive_factor{0.0f};
    bool alpha_blend = false;
    bool alpha_mask = false;
    bool double_sided = false;
    GLsizei index_count = 0;
  };

  std::vector<PrimitiveGpuData> m_primitives;
  Texture2D m_fallback_texture;
  std::vector<Texture2D> m_textures;
  std::size_t m_index_count = 0;
};

}  // namespace kage::render
