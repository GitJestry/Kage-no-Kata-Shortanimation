#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <string_view>

namespace kage::render {

class ShaderProgram final {
 public:
  ShaderProgram() = default;
  ShaderProgram(std::string_view parVertexSource,
                std::string_view parFragmentSource);

  ShaderProgram(const ShaderProgram&) = delete;
  ShaderProgram& operator=(const ShaderProgram&) = delete;

  ShaderProgram(ShaderProgram&& parOther) noexcept;
  ShaderProgram& operator=(ShaderProgram&& parOther) noexcept;

  ~ShaderProgram();

  void create(std::string_view parVertexSource,
              std::string_view parFragmentSource);
  void use() const;
  void release();

  void setMat4(const char* parName, const glm::mat4& parValue) const;
  void setVec3(const char* parName, const glm::vec3& parValue) const;

  [[nodiscard]] GLuint getHandle() const;
  [[nodiscard]] bool isValid() const;

 private:
  [[nodiscard]] GLint getUniformLocation(const char* parName) const;

  GLuint m_handle = 0;
};

}  // namespace kage::render
