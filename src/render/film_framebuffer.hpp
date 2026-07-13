#pragma once

#include "render/shader_program.hpp"
#include "render/viewport_rect.hpp"

#include <glm/glm.hpp>

namespace kage::render {

class FilmFramebuffer final {
 public:
  FilmFramebuffer();
  ~FilmFramebuffer();

  FilmFramebuffer(const FilmFramebuffer&) = delete;
  FilmFramebuffer& operator=(const FilmFramebuffer&) = delete;

  void begin(const ViewportRect& parViewport, GLuint parDestinationFramebuffer,
             int parSamples = 1);
  void resume();
  void present();

 private:
  void resize(int parWidth, int parHeight, int parSamples);

  ShaderProgram m_tone_map_shader;
  GLuint m_framebuffer = 0;
  GLuint m_color = 0;
  GLuint m_depth = 0;
  GLuint m_vertex_array = 0;
  GLuint m_msaa_framebuffer = 0;
  GLuint m_msaa_color = 0;
  GLuint m_msaa_depth = 0;
  GLuint m_destination_framebuffer = 0;
  ViewportRect m_present_viewport;
  int m_width = 0;
  int m_height = 0;
  int m_samples = 1;
};

}  // namespace kage::render
