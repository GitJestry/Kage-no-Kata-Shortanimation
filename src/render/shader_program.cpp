#include "render/shader_program.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <stdexcept>
#include <string>
#include <utility>

namespace {

[[nodiscard]] std::string getShaderInfoLog(GLuint parShader) {
  GLint log_length = 0;
  glGetShaderiv(parShader, GL_INFO_LOG_LENGTH, &log_length);
  if (log_length <= 1) {
    return "no compiler log";
  }

  std::string log(static_cast<std::size_t>(log_length), '\0');
  glGetShaderInfoLog(parShader, log_length, nullptr, log.data());
  return log;
}

[[nodiscard]] std::string getProgramInfoLog(GLuint parProgram) {
  GLint log_length = 0;
  glGetProgramiv(parProgram, GL_INFO_LOG_LENGTH, &log_length);
  if (log_length <= 1) {
    return "no linker log";
  }

  std::string log(static_cast<std::size_t>(log_length), '\0');
  glGetProgramInfoLog(parProgram, log_length, nullptr, log.data());
  return log;
}

[[nodiscard]] GLuint compileShader(GLenum parType,
                                   std::string_view parSource) {
  const GLuint shader = glCreateShader(parType);
  if (shader == 0) {
    throw std::runtime_error("Failed to create OpenGL shader");
  }

  const char* source = parSource.data();
  const GLint source_length = static_cast<GLint>(parSource.size());
  glShaderSource(shader, 1, &source, &source_length);
  glCompileShader(shader);

  GLint compiled = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (compiled != GL_TRUE) {
    const std::string log = getShaderInfoLog(shader);
    glDeleteShader(shader);
    throw std::runtime_error("Failed to compile OpenGL shader: " + log);
  }

  return shader;
}

}  // namespace

namespace kage::render {

ShaderProgram::ShaderProgram(std::string_view parVertexSource,
                             std::string_view parFragmentSource) {
  create(parVertexSource, parFragmentSource);
}

ShaderProgram::ShaderProgram(ShaderProgram&& parOther) noexcept
    : m_handle(std::exchange(parOther.m_handle, 0)) {}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& parOther) noexcept {
  if (this != &parOther) {
    release();
    m_handle = std::exchange(parOther.m_handle, 0);
  }

  return *this;
}

ShaderProgram::~ShaderProgram() {
  release();
}

void ShaderProgram::create(std::string_view parVertexSource,
                           std::string_view parFragmentSource) {
  release();

  const GLuint vertex_shader =
      compileShader(GL_VERTEX_SHADER, parVertexSource);
  const GLuint fragment_shader =
      compileShader(GL_FRAGMENT_SHADER, parFragmentSource);

  m_handle = glCreateProgram();
  if (m_handle == 0) {
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    throw std::runtime_error("Failed to create OpenGL shader program");
  }

  glAttachShader(m_handle, vertex_shader);
  glAttachShader(m_handle, fragment_shader);
  glLinkProgram(m_handle);

  GLint linked = GL_FALSE;
  glGetProgramiv(m_handle, GL_LINK_STATUS, &linked);

  glDetachShader(m_handle, vertex_shader);
  glDetachShader(m_handle, fragment_shader);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  if (linked != GL_TRUE) {
    const std::string log = getProgramInfoLog(m_handle);
    release();
    throw std::runtime_error("Failed to link OpenGL shader program: " + log);
  }
}

void ShaderProgram::use() const {
  if (m_handle == 0) {
    throw std::runtime_error("Cannot use an empty OpenGL shader program");
  }

  glUseProgram(m_handle);
}

void ShaderProgram::release() {
  if (m_handle != 0) {
    glDeleteProgram(m_handle);
    m_handle = 0;
  }
}

void ShaderProgram::setMat4(const char* parName,
                            const glm::mat4& parValue) const {
  glUniformMatrix4fv(getUniformLocation(parName), 1, GL_FALSE,
                     glm::value_ptr(parValue));
}

void ShaderProgram::setVec3(const char* parName,
                            const glm::vec3& parValue) const {
  glUniform3f(getUniformLocation(parName), parValue.x, parValue.y,
              parValue.z);
}

GLuint ShaderProgram::getHandle() const {
  return m_handle;
}

bool ShaderProgram::isValid() const {
  return m_handle != 0;
}

GLint ShaderProgram::getUniformLocation(const char* parName) const {
  if (m_handle == 0) {
    throw std::runtime_error("Cannot query an empty OpenGL shader program");
  }

  return glGetUniformLocation(m_handle, parName);
}

}  // namespace kage::render
