#pragma once

#include "assets/gltf_asset_loader.hpp"
#include "render/gpu_buffer.hpp"
#include "render/shader_program.hpp"
#include "render/vertex_array.hpp"

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <cstddef>
#include <vector>

namespace kage::render {

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
            const glm::vec3& parMaterialColor) const;
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
    GLsizei index_count = 0;
  };

  std::vector<PrimitiveGpuData> m_primitives;
  std::size_t m_index_count = 0;
};

}  // namespace kage::render
