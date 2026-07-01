#pragma once

#include "render/gpu_buffer.hpp"
#include "render/shader_program.hpp"
#include "render/vertex_array.hpp"

#include <glm/glm.hpp>

#include <span>

namespace kage::render {

struct SolidGizmoVertex final {
  glm::vec3 position{};
  glm::vec4 color{1.0f};
};

class SolidGizmoRenderer final {
 public:
  SolidGizmoRenderer();

  SolidGizmoRenderer(const SolidGizmoRenderer&) = delete;
  SolidGizmoRenderer& operator=(const SolidGizmoRenderer&) = delete;

  SolidGizmoRenderer(SolidGizmoRenderer&&) noexcept = default;
  SolidGizmoRenderer& operator=(SolidGizmoRenderer&&) noexcept = default;

  void draw(std::span<const SolidGizmoVertex> parVertices,
            const glm::mat4& parViewProjection) const;

 private:
  ShaderProgram m_shader;
  VertexArray m_vertex_array;
  GpuBuffer m_vertex_buffer;
};

}  // namespace kage::render
