#pragma once

#include "render/gpu_buffer.hpp"
#include "render/shader_program.hpp"
#include "render/vertex_array.hpp"

#include <glm/glm.hpp>

#include <span>

namespace kage::render {

struct LineVertex final {
  glm::vec3 position{};
  glm::vec3 color{1.0f};
};

class LineRenderer final {
 public:
  LineRenderer();

  LineRenderer(const LineRenderer&) = delete;
  LineRenderer& operator=(const LineRenderer&) = delete;

  LineRenderer(LineRenderer&&) noexcept = default;
  LineRenderer& operator=(LineRenderer&&) noexcept = default;

  void draw(std::span<const LineVertex> parVertices,
            const glm::mat4& parViewProjection) const;

 private:
  ShaderProgram m_shader;
  VertexArray m_vertex_array;
  GpuBuffer m_vertex_buffer;
};

}  // namespace kage::render
