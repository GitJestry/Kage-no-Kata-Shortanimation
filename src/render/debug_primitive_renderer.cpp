#include "render/debug_primitive_renderer.hpp"

#include <glad/gl.h>

#include <cstddef>

namespace {

constexpr GLuint POSITION_ATTRIBUTE = 0;
constexpr GLuint COLOR_ATTRIBUTE = 1;
constexpr GLsizei VERTEX_STRIDE =
    static_cast<GLsizei>(sizeof(kage::render::DebugVertex));

constexpr char VERTEX_SHADER[] = R"(#version 410 core
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec4 inColor;

uniform mat4 u_view_projection;

out vec4 vertexColor;

void main() {
  vertexColor = inColor;
  gl_Position = u_view_projection * vec4(inPosition, 1.0);
}
)";

constexpr char FRAGMENT_SHADER[] = R"(#version 410 core
in vec4 vertexColor;

out vec4 fragColor;

void main() {
  fragColor = vertexColor;
}
)";

}  // namespace

namespace kage::render {

DebugPrimitiveRenderer::DebugPrimitiveRenderer() {
  m_shader.create(VERTEX_SHADER, FRAGMENT_SHADER);
  m_vertex_array.create();
  m_vertex_buffer.create(GL_ARRAY_BUFFER);

  m_vertex_array.bind();
  m_vertex_buffer.bind();
  m_vertex_array.setFloatAttribute(POSITION_ATTRIBUTE, 3, GL_FLOAT,
                                   VERTEX_STRIDE,
                                   offsetof(DebugVertex, position));
  m_vertex_array.setFloatAttribute(COLOR_ATTRIBUTE, 4, GL_FLOAT,
                                   VERTEX_STRIDE,
                                   offsetof(DebugVertex, color));
  VertexArray::unbind();
  GpuBuffer::unbind(GL_ARRAY_BUFFER);
}

void DebugPrimitiveRenderer::drawLines(
    std::span<const DebugVertex> parVertices,
    const glm::mat4& parViewProjection) const {
  draw(parVertices, parViewProjection, GL_LINES);
}

void DebugPrimitiveRenderer::drawTriangles(
    std::span<const DebugVertex> parVertices,
    const glm::mat4& parViewProjection) const {
  draw(parVertices, parViewProjection, GL_TRIANGLES);
}

void DebugPrimitiveRenderer::draw(
    std::span<const DebugVertex> parVertices,
    const glm::mat4& parViewProjection, GLenum parMode) const {
  if (parVertices.empty()) {
    return;
  }

  m_shader.use();
  m_shader.setMat4("u_view_projection", parViewProjection);
  m_vertex_array.bind();
  m_vertex_buffer.setData(parVertices.size_bytes(), parVertices.data(),
                          GL_DYNAMIC_DRAW);
  glDrawArrays(parMode, 0, static_cast<GLsizei>(parVertices.size()));
  VertexArray::unbind();
  GpuBuffer::unbind(GL_ARRAY_BUFFER);
}

}  // namespace kage::render
