#include "render/gpu_buffer.hpp"

#include <stdexcept>
#include <utility>

namespace kage::render {

GpuBuffer::GpuBuffer(GLenum parTarget) {
  create(parTarget);
}

GpuBuffer::GpuBuffer(GpuBuffer&& parOther) noexcept
    : m_handle(std::exchange(parOther.m_handle, 0)),
      m_target(std::exchange(parOther.m_target, GL_ARRAY_BUFFER)) {}

GpuBuffer& GpuBuffer::operator=(GpuBuffer&& parOther) noexcept {
  if (this != &parOther) {
    release();
    m_handle = std::exchange(parOther.m_handle, 0);
    m_target = std::exchange(parOther.m_target, GL_ARRAY_BUFFER);
  }

  return *this;
}

GpuBuffer::~GpuBuffer() {
  release();
}

void GpuBuffer::create(GLenum parTarget) {
  release();
  m_target = parTarget;
  glGenBuffers(1, &m_handle);
  if (m_handle == 0) {
    throw std::runtime_error("Failed to create OpenGL buffer");
  }
}

void GpuBuffer::bind() const {
  if (m_handle == 0) {
    throw std::runtime_error("Cannot bind an empty OpenGL buffer");
  }

  glBindBuffer(m_target, m_handle);
}

void GpuBuffer::setData(std::size_t parByteSize, const void* parData,
                        GLenum parUsage) const {
  bind();
  glBufferData(m_target, static_cast<GLsizeiptr>(parByteSize), parData,
               parUsage);
}

void GpuBuffer::release() {
  if (m_handle != 0) {
    glDeleteBuffers(1, &m_handle);
    m_handle = 0;
  }
}

GLuint GpuBuffer::getHandle() const {
  return m_handle;
}

bool GpuBuffer::isValid() const {
  return m_handle != 0;
}

void GpuBuffer::unbind(GLenum parTarget) {
  glBindBuffer(parTarget, 0);
}

}  // namespace kage::render
