#pragma once

#include "assets/asset_types.hpp"
#include "render/gpu_buffer.hpp"
#include "render/shader_program.hpp"
#include "render/texture_2d.hpp"
#include "render/vertex_array.hpp"

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <array>
#include <memory>
#include <span>
#include <vector>

namespace kage::render {

class TextureResourceCache;

enum class MaterialDebugMode {
  Lit,
  BaseColor,
  Normal,
  Roughness,
  Metallic,
  Uv
};

enum class MeshDrawPass {
  Opaque,
  Blend,
  All
};

class GpuMesh final {
 public:
  GpuMesh() = default;

  GpuMesh(const GpuMesh&) = delete;
  GpuMesh& operator=(const GpuMesh&) = delete;

  GpuMesh(GpuMesh&& parOther) noexcept;
  GpuMesh& operator=(GpuMesh&& parOther) noexcept;
  ~GpuMesh();

  void upload(const assets::StaticModel& parModel,
              TextureResourceCache& parTextureCache);
  void draw(const ShaderProgram& parShader,
            const glm::vec3& parCameraPosition,
            const glm::mat4& parEntityTransform,
            std::span<const std::vector<glm::mat4>> parSkinMatrices,
            float parEntityOpacity,
            bool parSolidMode = false,
            MeshDrawPass parPass = MeshDrawPass::All) const;
  void drawOutline(const ShaderProgram& parShader,
                   const glm::mat4& parViewProjection,
                   const glm::mat4& parEntityTransform,
                   std::span<const std::vector<glm::mat4>> parSkinMatrices,
                   const glm::vec4& parColor,
                   float parThickness) const;
  void drawPicking(const ShaderProgram& parShader,
                   const glm::mat4& parViewProjection,
                   const glm::mat4& parEntityTransform,
                   std::span<const std::vector<glm::mat4>> parSkinMatrices,
                   std::uint32_t parEntityId) const;
  void drawShadow(const ShaderProgram& parShader,
                  const glm::mat4& parLightViewProjection,
                  const glm::mat4& parEntityTransform,
                  std::span<const std::vector<glm::mat4>> parSkinMatrices,
                  const glm::vec3* parPointLightPosition = nullptr,
                  float parPointLightRange = 1.0f) const;
  void clear();

  [[nodiscard]] std::size_t getPrimitiveCount() const;
  [[nodiscard]] std::size_t getIndexCount() const;
  [[nodiscard]] bool isValid() const;
  [[nodiscard]] bool hasPrimitivesForPass(float parEntityOpacity,
                                          bool parSolidMode,
                                          MeshDrawPass parPass) const;

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
    assets::AlphaMode alpha_mode = assets::AlphaMode::Opaque;
    bool double_sided = false;
    bool has_skin = false;
    std::uint32_t skin_index = assets::INVALID_SKIN_INDEX;
    std::size_t primitive_index = 0;
    GLsizei index_count = 0;
    glm::vec3 center{0.0f};
  };

  struct TextureBinding final {
    std::shared_ptr<Texture2D> storage;
    TextureSampler sampler;
  };

  std::vector<PrimitiveGpuData> m_primitives;
  std::size_t m_index_count = 0;
  bool m_has_opaque_primitives = false;
  bool m_has_blend_primitives = false;
  Texture2D m_fallback_texture;
  std::vector<TextureBinding> m_textures;
};

}  // namespace kage::render
