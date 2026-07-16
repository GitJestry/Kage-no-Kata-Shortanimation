#include "render/environment_renderer.hpp"

#include <glad/gl.h>
#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace {

constexpr char VERTEX_SHADER[] = R"(#version 410 core
out vec2 screenPosition;
void main() {
  vec2 position = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
  screenPosition = position * 2.0 - 1.0;
  gl_Position = vec4(screenPosition, 1.0, 1.0);
}
)";

constexpr char FRAGMENT_SHADER[] = R"(#version 410 core
uniform mat4 u_inverse_projection;
uniform mat4 u_inverse_view_rotation;
uniform sampler2D u_panorama;
uniform float u_intensity;
uniform float u_yaw_radians;
in vec2 screenPosition;
out vec4 fragColor;
const float PI = 3.14159265358979323846;
void main() {
  vec4 view = u_inverse_projection * vec4(screenPosition, 1.0, 1.0);
  vec3 direction = normalize(mat3(u_inverse_view_rotation) * view.xyz);
  float u = fract(atan(direction.z, direction.x) / (2.0 * PI) + 0.5 +
                  u_yaw_radians / (2.0 * PI));
  float v = 0.5 - asin(clamp(direction.y, -1.0, 1.0)) / PI;
  fragColor = vec4(texture(u_panorama, vec2(u, v)).rgb * u_intensity, 1.0);
}
)";

}  // namespace

namespace kage::render {

EnvironmentRenderer::EnvironmentRenderer() {
  m_shader.create(VERTEX_SHADER, FRAGMENT_SHADER);
  glGenVertexArrays(1, &m_vertex_array);
}

EnvironmentRenderer::~EnvironmentRenderer() {
  if (m_decode.valid()) {
    m_decode.wait();
  }
  if (m_vertex_array != 0) {
    glDeleteVertexArrays(1, &m_vertex_array);
  }
}

void EnvironmentRenderer::request(assets::AssetId parAsset,
                                  const std::filesystem::path& parPath) {
  if (!parAsset.isValid() || parPath.empty()) {
    ++m_generation;
    m_requested_asset = {};
    m_loaded_asset = {};
    m_panorama.release();
    m_state = EnvironmentLoadState::None;
    m_error.clear();
    return;
  }
  if (parAsset == m_requested_asset &&
      (m_state == EnvironmentLoadState::Loading ||
       m_state == EnvironmentLoadState::Ready)) {
    return;
  }
  m_requested_asset = parAsset;
  const std::uint64_t generation = ++m_generation;
  m_state = EnvironmentLoadState::Loading;
  m_error.clear();
  m_decode = std::async(std::launch::async, [parPath, generation]() {
    DecodedEnvironmentImage image = decodeEnvironmentImage(parPath);
    image.generation = generation;
    return image;
  });
}

void EnvironmentRenderer::pollUpload() {
  if (!m_decode.valid() ||
      m_decode.wait_for(std::chrono::seconds(0)) !=
          std::future_status::ready) {
    return;
  }
  DecodedEnvironmentImage image = m_decode.get();
  if (image.generation != m_generation) {
    return;
  }
  if (!image.error.empty()) {
    m_state = EnvironmentLoadState::Error;
    m_error = std::move(image.error);
    return;
  }
  if (image.hdr) {
    m_panorama.uploadFloat(image.width, image.height, 3, image.hdr_pixels);
  } else {
    m_panorama.upload(image.width, image.height, 4, image.ldr_pixels,
                      TextureColorSpace::Srgb);
  }
  m_panorama.setSampling(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_REPEAT,
                         GL_CLAMP_TO_EDGE);
  m_loaded_asset = m_requested_asset;
  m_state = EnvironmentLoadState::Ready;
}

void EnvironmentRenderer::draw(const camera::Camera& parCamera,
                               const glm::vec2& parViewportSize,
                               const EnvironmentSettings& parSettings) {
  pollUpload();
  if (!parSettings.visible || !m_panorama.isValid() ||
      parSettings.asset_id != m_loaded_asset) {
    return;
  }
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  m_shader.use();
  const glm::vec2 viewport = glm::max(parViewportSize, glm::vec2(1.0f));
  m_shader.setMat4("u_inverse_projection",
                   glm::inverse(parCamera.getProjectionMatrix(viewport)));
  glm::mat4 view_rotation = parCamera.getViewMatrix();
  view_rotation[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
  m_shader.setMat4("u_inverse_view_rotation", glm::inverse(view_rotation));
  m_shader.setFloat("u_intensity", parSettings.intensity);
  m_shader.setFloat("u_yaw_radians", glm::radians(parSettings.yaw_degrees));
  m_shader.setInt("u_panorama", 0);
  m_panorama.bind(0);
  glBindVertexArray(m_vertex_array);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(0);
  Texture2D::unbind(0);
  glDepthMask(GL_TRUE);
  glEnable(GL_DEPTH_TEST);
}

EnvironmentLoadState EnvironmentRenderer::getState() const { return m_state; }

const std::string& EnvironmentRenderer::getError() const { return m_error; }

}  // namespace kage::render
