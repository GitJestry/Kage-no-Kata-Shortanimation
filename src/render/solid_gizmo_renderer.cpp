#include "render/solid_gizmo_renderer.hpp"

#include <glad/gl.h>

#include <cstddef>

namespace {

constexpr GLuint POSITION_ATTRIBUTE = 0;
constexpr GLuint COLOR_ATTRIBUTE = 1;
constexpr GLsizei SOLID_VERTEX_STRIDE =
    static_cast<GLsizei>(sizeof(kage::render::SolidGizmoVertex));

constexpr char SOLID_VERTEX_SHADER[] = R"(#version 410 core
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec4 inColor;

uniform mat4 u_view_projection;

out vec4 vertexColor;

void main() {
  vertexColor = inColor;
  gl_Position = u_view_projection * vec4(inPosition, 1.0);
}
)";

constexpr char SOLID_FRAGMENT_SHADER[] = R"(#version 410 core
in vec4 vertexColor;

out vec4 fragColor;

void main() {
  fragColor = vertexColor;
}
)";

}  // namespace

namespace kage::render {

SolidGizmoRenderer::SolidGizmoRenderer() {
  m_shader.create(SOLID_VERTEX_SHADER, SOLID_FRAGMENT_SHADER);
  m_vertex_array.create();
  m_vertex_buffer.create(GL_ARRAY_BUFFER);

  m_vertex_array.bind();
  m_vertex_buffer.bind();
  m_vertex_array.setFloatAttribute(
      POSITION_ATTRIBUTE, 3, GL_FLOAT, SOLID_VERTEX_STRIDE,
      offsetof(SolidGizmoVertex, position));
  m_vertex_array.setFloatAttribute(
      COLOR_ATTRIBUTE, 4, GL_FLOAT, SOLID_VERTEX_STRIDE,
      offsetof(SolidGizmoVertex, color));
  VertexArray::unbind();
  GpuBuffer::unbind(GL_ARRAY_BUFFER);
}

void SolidGizmoRenderer::draw(std::span<const SolidGizmoVertex> parVertices,
                              const glm::mat4& parViewProjection) const {
  if (parVertices.empty()) {
    return;
  }

  m_shader.use();
  m_shader.setMat4("u_view_projection", parViewProjection);
  m_vertex_array.bind();
  m_vertex_buffer.setData(parVertices.size_bytes(), parVertices.data(),
                          GL_DYNAMIC_DRAW);
  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(parVertices.size()));
  VertexArray::unbind();
  GpuBuffer::unbind(GL_ARRAY_BUFFER);
}

}  // namespace kage::render
