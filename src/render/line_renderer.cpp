#include "render/line_renderer.hpp"

#include <glad/gl.h>

#include <cstddef>

namespace {

constexpr GLuint POSITION_ATTRIBUTE = 0;
constexpr GLuint COLOR_ATTRIBUTE = 1;
constexpr GLsizei LINE_VERTEX_STRIDE =
    static_cast<GLsizei>(sizeof(kage::render::LineVertex));

constexpr char LINE_VERTEX_SHADER[] = R"(#version 410 core
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inColor;

uniform mat4 u_view_projection;

out vec3 lineColor;

void main() {
  lineColor = inColor;
  gl_Position = u_view_projection * vec4(inPosition, 1.0);
}
)";

constexpr char LINE_FRAGMENT_SHADER[] = R"(#version 410 core
in vec3 lineColor;

out vec4 fragColor;

void main() {
  fragColor = vec4(lineColor, 1.0);
}
)";

}  // namespace

namespace kage::render {

LineRenderer::LineRenderer() {
  m_shader.create(LINE_VERTEX_SHADER, LINE_FRAGMENT_SHADER);
  m_vertex_array.create();
  m_vertex_buffer.create(GL_ARRAY_BUFFER);

  m_vertex_array.bind();
  m_vertex_buffer.bind();
  m_vertex_array.setFloatAttribute(
      POSITION_ATTRIBUTE, 3, GL_FLOAT, LINE_VERTEX_STRIDE,
      offsetof(LineVertex, position));
  m_vertex_array.setFloatAttribute(COLOR_ATTRIBUTE, 3, GL_FLOAT,
                                   LINE_VERTEX_STRIDE,
                                   offsetof(LineVertex, color));
  VertexArray::unbind();
  GpuBuffer::unbind(GL_ARRAY_BUFFER);
}

void LineRenderer::draw(std::span<const LineVertex> parVertices,
                        const glm::mat4& parViewProjection) const {
  if (parVertices.empty()) {
    return;
  }

  m_shader.use();
  m_shader.setMat4("u_view_projection", parViewProjection);
  m_vertex_array.bind();
  m_vertex_buffer.setData(parVertices.size_bytes(), parVertices.data(),
                          GL_DYNAMIC_DRAW);
  glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(parVertices.size()));
  VertexArray::unbind();
  GpuBuffer::unbind(GL_ARRAY_BUFFER);
}

}  // namespace kage::render
