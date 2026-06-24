#include "render/gpu_mesh.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace {

constexpr GLuint POSITION_ATTRIBUTE = 0;
constexpr GLuint NORMAL_ATTRIBUTE = 1;
constexpr GLuint TEX_COORD_ATTRIBUTE = 2;
constexpr GLsizei STATIC_VERTEX_STRIDE =
    static_cast<GLsizei>(sizeof(kage::assets::StaticVertex));

}  // namespace

namespace kage::render {

void GpuMesh::upload(const assets::StaticModel& parModel) {
  clear();
  m_primitives.reserve(parModel.primitives.size());

  for (const assets::StaticPrimitive& primitive : parModel.primitives) {
    if (primitive.vertices.empty() || primitive.indices.empty()) {
      continue;
    }

    PrimitiveGpuData gpu_primitive;
    gpu_primitive.transform = primitive.transform;
    gpu_primitive.index_count =
        static_cast<GLsizei>(primitive.indices.size());
    gpu_primitive.vertex_array.create();
    gpu_primitive.vertex_buffer.create(GL_ARRAY_BUFFER);
    gpu_primitive.index_buffer.create(GL_ELEMENT_ARRAY_BUFFER);

    gpu_primitive.vertex_array.bind();
    gpu_primitive.vertex_buffer.setData(
        primitive.vertices.size() * sizeof(assets::StaticVertex),
        primitive.vertices.data(), GL_STATIC_DRAW);
    gpu_primitive.index_buffer.setData(
        primitive.indices.size() * sizeof(std::uint32_t),
        primitive.indices.data(), GL_STATIC_DRAW);

    gpu_primitive.vertex_array.setFloatAttribute(
        POSITION_ATTRIBUTE, 3, GL_FLOAT, STATIC_VERTEX_STRIDE,
        offsetof(assets::StaticVertex, position));
    gpu_primitive.vertex_array.setFloatAttribute(
        NORMAL_ATTRIBUTE, 3, GL_FLOAT, STATIC_VERTEX_STRIDE,
        offsetof(assets::StaticVertex, normal));
    gpu_primitive.vertex_array.setFloatAttribute(
        TEX_COORD_ATTRIBUTE, 2, GL_FLOAT, STATIC_VERTEX_STRIDE,
        offsetof(assets::StaticVertex, tex_coord));

    m_index_count += primitive.indices.size();
    m_primitives.push_back(std::move(gpu_primitive));
  }

  VertexArray::unbind();
  GpuBuffer::unbind(GL_ARRAY_BUFFER);
  GpuBuffer::unbind(GL_ELEMENT_ARRAY_BUFFER);
}

void GpuMesh::draw(const ShaderProgram& parShader,
                   const glm::mat4& parViewProjection,
                   const glm::vec3& parMaterialColor) const {
  if (m_primitives.empty()) {
    return;
  }

  parShader.use();
  parShader.setMat4("u_view_projection", parViewProjection);
  parShader.setVec3("u_material_color", parMaterialColor);

  for (const PrimitiveGpuData& primitive : m_primitives) {
    parShader.setMat4("u_model", primitive.transform);
    primitive.vertex_array.bind();
    glDrawElements(GL_TRIANGLES, primitive.index_count, GL_UNSIGNED_INT,
                   nullptr);
  }

  VertexArray::unbind();
}

void GpuMesh::clear() {
  m_primitives.clear();
  m_index_count = 0;
}

std::size_t GpuMesh::getPrimitiveCount() const {
  return m_primitives.size();
}

std::size_t GpuMesh::getIndexCount() const {
  return m_index_count;
}

bool GpuMesh::isValid() const {
  return !m_primitives.empty();
}

}  // namespace kage::render
