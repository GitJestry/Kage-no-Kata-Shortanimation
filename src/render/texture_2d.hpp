#pragma once

#include <glad/gl.h>

#include <cstddef>
#include <span>

namespace kage::render {

enum class TextureColorSpace {
  Linear,
  Srgb
};

class Texture2D final {
 public:
  Texture2D() = default;

  Texture2D(const Texture2D&) = delete;
  Texture2D& operator=(const Texture2D&) = delete;

  Texture2D(Texture2D&& parOther) noexcept;
  Texture2D& operator=(Texture2D&& parOther) noexcept;

  ~Texture2D();

  void create();
  void upload(int parWidth, int parHeight, int parComponentCount,
              std::span<const unsigned char> parPixels,
              TextureColorSpace parColorSpace = TextureColorSpace::Linear);
  void setSampling(GLint parMinFilter, GLint parMagFilter, GLint parWrapS,
                   GLint parWrapT) const;
  void bind(GLuint parTextureUnit) const;
  void release();

  [[nodiscard]] GLuint getHandle() const;
  [[nodiscard]] bool isValid() const;

  static void unbind(GLuint parTextureUnit);

 private:
  GLuint m_handle = 0;
};

}  // namespace kage::render
