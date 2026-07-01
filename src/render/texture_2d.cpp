#include "render/texture_2d.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace {

[[nodiscard]] GLenum getTextureFormat(int parComponentCount) {
  switch (parComponentCount) {
    case 1:
      return GL_RED;
    case 2:
      return GL_RG;
    case 3:
      return GL_RGB;
    case 4:
      return GL_RGBA;
    default:
      throw std::runtime_error("Unsupported texture component count");
  }
}

[[nodiscard]] GLenum getInternalTextureFormat(
    int parComponentCount, kage::render::TextureColorSpace parColorSpace) {
  if (parColorSpace == kage::render::TextureColorSpace::Srgb) {
    switch (parComponentCount) {
      case 3:
        return GL_SRGB8;
      case 4:
        return GL_SRGB8_ALPHA8;
      default:
        break;
    }
  }

  return getTextureFormat(parComponentCount);
}

[[nodiscard]] std::size_t getExpectedByteSize(int parWidth, int parHeight,
                                              int parComponentCount) {
  if (parWidth <= 0 || parHeight <= 0 || parComponentCount <= 0) {
    throw std::runtime_error("Texture dimensions are invalid");
  }

  const std::size_t width = static_cast<std::size_t>(parWidth);
  const std::size_t height = static_cast<std::size_t>(parHeight);
  const std::size_t component_count =
      static_cast<std::size_t>(parComponentCount);
  if (height >
      std::numeric_limits<std::size_t>::max() / width) {
    throw std::runtime_error("Texture byte size overflow");
  }

  const std::size_t texel_count = width * height;
  if (component_count >
      std::numeric_limits<std::size_t>::max() / texel_count) {
    throw std::runtime_error("Texture byte size overflow");
  }

  return texel_count * component_count;
}

}  // namespace

namespace kage::render {

Texture2D::Texture2D(Texture2D&& parOther) noexcept
    : m_handle(std::exchange(parOther.m_handle, 0)) {}

Texture2D& Texture2D::operator=(Texture2D&& parOther) noexcept {
  if (this != &parOther) {
    release();
    m_handle = std::exchange(parOther.m_handle, 0);
  }

  return *this;
}

Texture2D::~Texture2D() {
  release();
}

void Texture2D::create() {
  release();
  glGenTextures(1, &m_handle);
  if (m_handle == 0) {
    throw std::runtime_error("Failed to create OpenGL texture");
  }
}

void Texture2D::upload(int parWidth, int parHeight, int parComponentCount,
                       std::span<const unsigned char> parPixels,
                       TextureColorSpace parColorSpace) {
  const GLenum format = getTextureFormat(parComponentCount);
  const GLenum internal_format =
      getInternalTextureFormat(parComponentCount, parColorSpace);
  const std::size_t expected_byte_size =
      getExpectedByteSize(parWidth, parHeight, parComponentCount);
  if (parPixels.size() < expected_byte_size) {
    throw std::runtime_error("Texture pixel data is incomplete");
  }

  if (m_handle == 0) {
    create();
  }

  bind(0);
  GLint previous_unpack_alignment = 4;
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous_unpack_alignment);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, internal_format, parWidth, parHeight, 0,
               format, GL_UNSIGNED_BYTE, parPixels.data());
  glGenerateMipmap(GL_TEXTURE_2D);
  glPixelStorei(GL_UNPACK_ALIGNMENT, previous_unpack_alignment);
}

void Texture2D::setSampling(GLint parMinFilter, GLint parMagFilter,
                            GLint parWrapS, GLint parWrapT) const {
  bind(0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, parMinFilter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, parMagFilter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, parWrapS);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, parWrapT);
}

void Texture2D::bind(GLuint parTextureUnit) const {
  if (m_handle == 0) {
    throw std::runtime_error("Cannot bind an empty OpenGL texture");
  }

  glActiveTexture(GL_TEXTURE0 + parTextureUnit);
  glBindTexture(GL_TEXTURE_2D, m_handle);
}

void Texture2D::release() {
  if (m_handle != 0) {
    glDeleteTextures(1, &m_handle);
    m_handle = 0;
  }
}

GLuint Texture2D::getHandle() const {
  return m_handle;
}

bool Texture2D::isValid() const {
  return m_handle != 0;
}

void Texture2D::unbind(GLuint parTextureUnit) {
  glActiveTexture(GL_TEXTURE0 + parTextureUnit);
  glBindTexture(GL_TEXTURE_2D, 0);
}

}  // namespace kage::render
