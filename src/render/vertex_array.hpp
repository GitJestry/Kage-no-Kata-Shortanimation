#pragma once

#include <glad/gl.h>

#include <cstddef>

namespace kage::render {

class VertexArray final {
 public:
  VertexArray() = default;

  VertexArray(const VertexArray&) = delete;
  VertexArray& operator=(const VertexArray&) = delete;

  VertexArray(VertexArray&& parOther) noexcept;
  VertexArray& operator=(VertexArray&& parOther) noexcept;

  ~VertexArray();

  void create();
  void bind() const;
  void setFloatAttribute(GLuint parLocation, GLint parComponentCount,
                         GLenum parComponentType, GLsizei parStride,
                         std::size_t parByteOffset,
                         bool parNormalized = false) const;
  void release();

  [[nodiscard]] GLuint getHandle() const;
  [[nodiscard]] bool isValid() const;

  static void unbind();

 private:
  GLuint m_handle = 0;
};

}  // namespace kage::render
