#pragma once

#include "render/gpu_buffer.hpp"
#include "render/shader_program.hpp"
#include "render/vertex_array.hpp"

#include <glm/glm.hpp>

#include <span>

namespace kage::render {

struct DebugVertex final {
  glm::vec3 position{};
  glm::vec4 color{1.0f};
};

class DebugPrimitiveRenderer final {
 public:
  DebugPrimitiveRenderer();

  DebugPrimitiveRenderer(const DebugPrimitiveRenderer&) = delete;
  DebugPrimitiveRenderer& operator=(const DebugPrimitiveRenderer&) = delete;

  DebugPrimitiveRenderer(DebugPrimitiveRenderer&&) noexcept = default;
  DebugPrimitiveRenderer& operator=(DebugPrimitiveRenderer&&) noexcept = default;

  void drawLines(std::span<const DebugVertex> parVertices,
                 const glm::mat4& parViewProjection) const;
  void drawTriangles(std::span<const DebugVertex> parVertices,
                     const glm::mat4& parViewProjection) const;

 private:
  void draw(std::span<const DebugVertex> parVertices,
            const glm::mat4& parViewProjection, GLenum parMode) const;

  ShaderProgram m_shader;
  VertexArray m_vertex_array;
  GpuBuffer m_vertex_buffer;
};

}  // namespace kage::render
