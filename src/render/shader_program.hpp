#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <string>
#include <string_view>
#include <unordered_map>

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
  void setFloat(const char* parName, float parValue) const;
  void setInt(const char* parName, int parValue) const;
  void setVec2(const char* parName, const glm::vec2& parValue) const;
  void setVec3(const char* parName, const glm::vec3& parValue) const;
  void setVec4(const char* parName, const glm::vec4& parValue) const;

  [[nodiscard]] GLuint getHandle() const;
  [[nodiscard]] bool isValid() const;

 private:
  [[nodiscard]] GLint getUniformLocation(const char* parName) const;

  GLuint m_handle = 0;
  mutable std::unordered_map<std::string, GLint> m_uniform_locations;
};

}  // namespace kage::render
