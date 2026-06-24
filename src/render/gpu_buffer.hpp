#pragma once

#include <glad/gl.h>

#include <cstddef>

namespace kage::render {

class GpuBuffer final {
 public:
  GpuBuffer() = default;
  explicit GpuBuffer(GLenum parTarget);

  GpuBuffer(const GpuBuffer&) = delete;
  GpuBuffer& operator=(const GpuBuffer&) = delete;

  GpuBuffer(GpuBuffer&& parOther) noexcept;
  GpuBuffer& operator=(GpuBuffer&& parOther) noexcept;

  ~GpuBuffer();

  void create(GLenum parTarget);
  void bind() const;
  void setData(std::size_t parByteSize, const void* parData,
               GLenum parUsage) const;
  void release();

  [[nodiscard]] GLuint getHandle() const;
  [[nodiscard]] bool isValid() const;

  static void unbind(GLenum parTarget);

 private:
  GLuint m_handle = 0;
  GLenum m_target = GL_ARRAY_BUFFER;
};

}  // namespace kage::render
