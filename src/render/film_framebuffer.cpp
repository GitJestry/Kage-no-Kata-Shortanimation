#include "render/film_framebuffer.hpp"

#include <glad/gl.h>

#include <algorithm>
#include <stdexcept>

namespace {

constexpr char VERTEX_SHADER[] = R"(#version 410 core
out vec2 texCoord;
void main() {
  vec2 position = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
  texCoord = position;
  gl_Position = vec4(position * 2.0 - 1.0, 0.0, 1.0);
}
)";

constexpr char FRAGMENT_SHADER[] = R"(#version 410 core
uniform sampler2D u_hdr_color;
in vec2 texCoord;
out vec4 fragColor;
void main() {
  vec3 hdr = max(texture(u_hdr_color, texCoord).rgb, vec3(0.0));
  vec3 mapped = hdr / (hdr + vec3(1.0));
  fragColor = vec4(pow(mapped, vec3(1.0 / 2.2)), 1.0);
}
)";

}  // namespace

namespace kage::render {

FilmFramebuffer::FilmFramebuffer() {
  m_tone_map_shader.create(VERTEX_SHADER, FRAGMENT_SHADER);
  glGenFramebuffers(1, &m_framebuffer);
  glGenTextures(1, &m_color);
  glGenRenderbuffers(1, &m_depth);
  glGenVertexArrays(1, &m_vertex_array);
  glGenFramebuffers(1, &m_msaa_framebuffer);
  glGenRenderbuffers(1, &m_msaa_color);
  glGenRenderbuffers(1, &m_msaa_depth);
}

FilmFramebuffer::~FilmFramebuffer() {
  glDeleteVertexArrays(1, &m_vertex_array);
  glDeleteRenderbuffers(1, &m_depth);
  glDeleteTextures(1, &m_color);
  glDeleteFramebuffers(1, &m_framebuffer);
  glDeleteRenderbuffers(1, &m_msaa_depth);
  glDeleteRenderbuffers(1, &m_msaa_color);
  glDeleteFramebuffers(1, &m_msaa_framebuffer);
}

void FilmFramebuffer::resize(int parWidth, int parHeight, int parSamples) {
  m_width = std::max(parWidth, 1);
  m_height = std::max(parHeight, 1);
  m_samples = std::clamp(parSamples, 1, 4);
  glBindTexture(GL_TEXTURE_2D, m_color);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0, GL_RGBA,
               GL_HALF_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindRenderbuffer(GL_RENDERBUFFER, m_depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_width,
                        m_height);
  glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         m_color, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, m_depth);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    throw std::runtime_error("Film framebuffer is incomplete");
  }
  if (m_samples > 1) {
    glBindRenderbuffer(GL_RENDERBUFFER, m_msaa_color);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples, GL_RGBA16F,
                                     m_width, m_height);
    glBindRenderbuffer(GL_RENDERBUFFER, m_msaa_depth);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples,
                                     GL_DEPTH_COMPONENT24, m_width, m_height);
    glBindFramebuffer(GL_FRAMEBUFFER, m_msaa_framebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, m_msaa_color);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, m_msaa_depth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      throw std::runtime_error("Multisample film framebuffer is incomplete");
    }
  }
}

void FilmFramebuffer::begin(const ViewportRect& parViewport,
                            GLuint parDestinationFramebuffer,
                            int parSamples) {
  m_destination_framebuffer = parDestinationFramebuffer;
  m_present_viewport = parViewport;
  const int width = std::max(parViewport.size.x, 1);
  const int height = std::max(parViewport.size.y, 1);
  parSamples = std::clamp(parSamples, 1, 4);
  if (width != m_width || height != m_height || parSamples != m_samples) {
    resize(width, height, parSamples);
  }
  glBindFramebuffer(GL_FRAMEBUFFER,
                    m_samples > 1 ? m_msaa_framebuffer : m_framebuffer);
  glViewport(0, 0, m_width, m_height);
}

void FilmFramebuffer::resume() {
  glBindFramebuffer(GL_FRAMEBUFFER,
                    m_samples > 1 ? m_msaa_framebuffer : m_framebuffer);
  glViewport(0, 0, m_width, m_height);
}

void FilmFramebuffer::present() {
  if (m_samples > 1) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_msaa_framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_framebuffer);
    glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, m_destination_framebuffer);
  glViewport(m_present_viewport.origin.x, m_present_viewport.glViewportY(),
             m_width, m_height);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_BLEND);
  m_tone_map_shader.use();
  m_tone_map_shader.setInt("u_hdr_color", 0);
  glActiveTexture(GL_TEXTURE0);
  glBindSampler(0, 0);
  glBindTexture(GL_TEXTURE_2D, m_color);
  glBindVertexArray(m_vertex_array);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glDepthMask(GL_TRUE);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
}

}  // namespace kage::render
