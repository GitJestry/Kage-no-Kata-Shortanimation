#include "render/vertex_array.hpp"

#include <stdexcept>
#include <utility>

namespace kage::render {

VertexArray::VertexArray(VertexArray&& parOther) noexcept
    : m_handle(std::exchange(parOther.m_handle, 0)) {}

VertexArray& VertexArray::operator=(VertexArray&& parOther) noexcept {
  if (this != &parOther) {
    release();
    m_handle = std::exchange(parOther.m_handle, 0);
  }

  return *this;
}

VertexArray::~VertexArray() {
  release();
}

void VertexArray::create() {
  release();
  glGenVertexArrays(1, &m_handle);
  if (m_handle == 0) {
    throw std::runtime_error("Failed to create OpenGL vertex array");
  }
}

void VertexArray::bind() const {
  if (m_handle == 0) {
    throw std::runtime_error("Cannot bind an empty OpenGL vertex array");
  }

  glBindVertexArray(m_handle);
}

void VertexArray::setFloatAttribute(GLuint parLocation,
                                    GLint parComponentCount,
                                    GLenum parComponentType,
                                    GLsizei parStride,
                                    std::size_t parByteOffset,
                                    bool parNormalized) const {
  bind();
  glEnableVertexAttribArray(parLocation);
  glVertexAttribPointer(
      parLocation, parComponentCount, parComponentType,
      parNormalized ? GL_TRUE : GL_FALSE, parStride,
      reinterpret_cast<const void*>(parByteOffset));
}

void VertexArray::release() {
  if (m_handle != 0) {
    glDeleteVertexArrays(1, &m_handle);
    m_handle = 0;
  }
}

GLuint VertexArray::getHandle() const {
  return m_handle;
}

bool VertexArray::isValid() const {
  return m_handle != 0;
}

void VertexArray::unbind() {
  glBindVertexArray(0);
}

}  // namespace kage::render
