#include "render/shadow_renderer.hpp"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <stdexcept>

namespace {

constexpr int POINT_RESOLUTION = 1024;
constexpr float SUN_RADIUS = 70.0f;

template <typename Value>
void hashCombine(std::size_t& parHash, const Value& parValue) {
  const std::size_t value = std::hash<Value>{}(parValue);
  parHash ^= value + 0x9e3779b9u + (parHash << 6u) + (parHash >> 2u);
}

void hashVec3(std::size_t& parHash, const glm::vec3& parValue) {
  hashCombine(parHash, parValue.x);
  hashCombine(parHash, parValue.y);
  hashCombine(parHash, parValue.z);
}

void hashMat4(std::size_t& parHash, const glm::mat4& parValue) {
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      hashCombine(parHash, parValue[column][row]);
    }
  }
}

constexpr char SHADOW_VERTEX_SHADER[] = R"(#version 410 core
layout (location = 0) in vec3 inPosition;
layout (location = 2) in vec2 inTexCoord;
layout (location = 4) in uvec4 inJoints;
layout (location = 5) in vec4 inWeights;
uniform mat4 u_model;
uniform mat4 u_light_view_projection;
uniform bool u_skinning_enabled;
uniform mat4 u_joint_matrices[128];
out vec2 meshTexCoord;
out vec3 worldPosition;
void main() {
  mat4 skin = mat4(1.0);
  if (u_skinning_enabled) {
    skin = u_joint_matrices[inJoints.x] * inWeights.x +
           u_joint_matrices[inJoints.y] * inWeights.y +
           u_joint_matrices[inJoints.z] * inWeights.z +
           u_joint_matrices[inJoints.w] * inWeights.w;
  }
  vec4 world = u_model * skin * vec4(inPosition, 1.0);
  worldPosition = world.xyz;
  meshTexCoord = inTexCoord;
  gl_Position = u_light_view_projection * world;
}
)";

constexpr char SHADOW_FRAGMENT_SHADER[] = R"(#version 410 core
uniform bool u_alpha_mask;
uniform float u_alpha_cutoff;
uniform float u_base_color_alpha;
uniform bool u_has_base_color_texture;
uniform sampler2D u_base_color_texture;
uniform vec2 u_base_color_offset;
uniform vec2 u_base_color_scale;
uniform float u_base_color_rotation;
uniform bool u_point_shadow;
uniform vec3 u_light_position;
uniform float u_light_range;
in vec2 meshTexCoord;
in vec3 worldPosition;
vec2 transformUv(vec2 uv) {
  vec2 scaled = uv * u_base_color_scale;
  float c = cos(u_base_color_rotation);
  float s = sin(u_base_color_rotation);
  return vec2(c * scaled.x - s * scaled.y,
              s * scaled.x + c * scaled.y) + u_base_color_offset;
}
void main() {
  float alpha = u_base_color_alpha;
  if (u_has_base_color_texture) {
    alpha *= texture(u_base_color_texture, transformUv(meshTexCoord)).a;
  }
  if (u_alpha_mask && alpha < u_alpha_cutoff) {
    discard;
  }
  if (u_point_shadow) {
    gl_FragDepth = length(worldPosition - u_light_position) / u_light_range;
  }
}
)";

}  // namespace

namespace kage::render {

ShadowRenderer::ShadowRenderer() {
  m_shader.create(SHADOW_VERTEX_SHADER, SHADOW_FRAGMENT_SHADER);
  createResources();
}

ShadowRenderer::~ShadowRenderer() {
  glDeleteTextures(1, &m_frame.sun_depth);
  glDeleteTextures(static_cast<GLsizei>(m_frame.point_depth.size()),
                   m_frame.point_depth.data());
  glDeleteFramebuffers(1, &m_framebuffer);
}

void ShadowRenderer::createResources() {
  glGenFramebuffers(1, &m_framebuffer);
  glGenTextures(1, &m_frame.sun_depth);
  resizeSunDepth(4096);

  glGenTextures(static_cast<GLsizei>(m_frame.point_depth.size()),
                m_frame.point_depth.data());
  for (GLuint texture : m_frame.point_depth) {
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
    for (int face = 0; face < 6; ++face) {
      glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0,
                   GL_DEPTH_COMPONENT32F, POINT_RESOLUTION,
                   POINT_RESOLUTION, 0, GL_DEPTH_COMPONENT, GL_FLOAT,
                   nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  }
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void ShadowRenderer::resizeSunDepth(int parResolution) {
  parResolution = std::clamp(parResolution, 256, 4096);
  if (parResolution == m_sun_resolution) {
    return;
  }
  m_sun_resolution = parResolution;
  glBindTexture(GL_TEXTURE_2D, m_frame.sun_depth);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, m_sun_resolution,
               m_sun_resolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  constexpr float BORDER[] = {1.0f, 1.0f, 1.0f, 1.0f};
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, BORDER);

}

void ShadowRenderer::drawCasters(
    std::span<const ShadowCaster> parCasters,
    const glm::mat4& parViewProjection, const glm::vec3* parPointPosition,
    float parPointRange) {
  for (const ShadowCaster& caster : parCasters) {
    if (caster.mesh != nullptr) {
      caster.mesh->drawShadow(m_shader, parViewProjection, caster.model,
                              caster.skin_matrices, parPointPosition,
                              parPointRange);
    }
  }
}

std::size_t ShadowRenderer::getInputHash(
    std::span<const ShadowCaster> parCasters, const camera::Camera& parCamera,
    const lighting::LightingState& parLighting, int parSunResolution,
    bool parRenderPointShadows) const {
  std::size_t hash = 0;
  hashCombine(hash, parSunResolution);
  hashCombine(hash, parRenderPointShadows);
  hashCombine(hash, parLighting.sun.enabled);
  hashVec3(hash, parLighting.sun.direction_to_light);
  hashVec3(hash, parCamera.position);
  hashVec3(hash, parCamera.getForward());
  hashCombine(hash, parLighting.point_light_count);
  for (std::size_t index = 0; index < parLighting.point_light_count; ++index) {
    const lighting::PointLight& light = parLighting.point_lights[index];
    hashCombine(hash, light.entity_id);
    hashCombine(hash, light.casts_shadow);
    hashVec3(hash, light.position);
    hashCombine(hash, light.range);
  }
  hashCombine(hash, parCasters.size());
  for (const ShadowCaster& caster : parCasters) {
    hashCombine(hash, reinterpret_cast<std::uintptr_t>(caster.mesh));
    hashMat4(hash, caster.model);
    hashCombine(hash, caster.skin_matrices.size());
    for (const std::vector<glm::mat4>& palette : caster.skin_matrices) {
      hashCombine(hash, palette.size());
      for (const glm::mat4& matrix : palette) {
        hashMat4(hash, matrix);
      }
    }
  }
  return hash;
}

const ShadowFrame& ShadowRenderer::render(
    std::span<const ShadowCaster> parCasters, const camera::Camera& parCamera,
    const lighting::LightingState& parLighting, int parSunResolution,
    bool parRenderPointShadows) {
  const std::size_t input_hash = getInputHash(
      parCasters, parCamera, parLighting, parSunResolution,
      parRenderPointShadows);
  if (m_has_cached_frame && input_hash == m_last_input_hash) {
    m_frame.reused = true;
    return m_frame;
  }
  m_frame.reused = false;
  resizeSunDepth(parSunResolution);
  m_frame.sun_enabled = parLighting.sun.enabled;
  m_frame.point_count = 0;
  glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_FRONT);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(2.0f, 4.0f);

  if (m_frame.sun_enabled) {
    const glm::vec3 center =
        parCamera.position + parCamera.getForward() * 24.0f;
    const glm::vec3 direction =
        glm::normalize(parLighting.sun.direction_to_light);
    const glm::vec3 up = std::abs(direction.y) > 0.96f
                             ? glm::vec3(0.0f, 0.0f, 1.0f)
                             : glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::mat4 view =
        glm::lookAt(center + direction * SUN_RADIUS, center, up);
    glm::mat4 projection =
        glm::ortho(-SUN_RADIUS, SUN_RADIUS, -SUN_RADIUS, SUN_RADIUS,
                   0.1f, SUN_RADIUS * 3.0f);
    // Snap the projection origin to shadow texels to prevent camera shimmer.
    glm::vec4 origin = projection * view * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    origin *= static_cast<float>(m_sun_resolution) * 0.5f;
    const glm::vec4 rounded = glm::round(origin);
    const glm::vec4 offset =
        (rounded - origin) * (2.0f / static_cast<float>(m_sun_resolution));
    projection[3][0] += offset.x;
    projection[3][1] += offset.y;
    m_frame.sun_view_projection = projection * view;
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D, m_frame.sun_depth, 0);
    glViewport(0, 0, m_sun_resolution, m_sun_resolution);
    glClear(GL_DEPTH_BUFFER_BIT);
    drawCasters(parCasters, m_frame.sun_view_projection);
  }

  constexpr std::array<glm::vec3, 6> DIRECTIONS = {
      glm::vec3(1, 0, 0),  glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0),
      glm::vec3(0, -1, 0), glm::vec3(0, 0, 1),  glm::vec3(0, 0, -1)};
  constexpr std::array<glm::vec3, 6> UPS = {
      glm::vec3(0, -1, 0), glm::vec3(0, -1, 0), glm::vec3(0, 0, 1),
      glm::vec3(0, 0, -1), glm::vec3(0, -1, 0), glm::vec3(0, -1, 0)};
  for (std::size_t light_index = 0; parRenderPointShadows &&
       light_index < parLighting.point_light_count &&
       m_frame.point_count < lighting::MAX_POINT_SHADOWS;
       ++light_index) {
    const lighting::PointLight& light =
        parLighting.point_lights[light_index];
    if (!light.casts_shadow) {
      continue;
    }
    const std::size_t shadow_index = m_frame.point_count++;
    m_frame.point_entity_ids[shadow_index] = light.entity_id;
    const glm::mat4 projection = glm::perspective(
        glm::radians(90.0f), 1.0f, 0.05f, std::max(light.range, 0.1f));
    glViewport(0, 0, POINT_RESOLUTION, POINT_RESOLUTION);
    for (std::size_t face = 0; face < DIRECTIONS.size(); ++face) {
      glFramebufferTexture2D(
          GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
          GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(face),
          m_frame.point_depth[shadow_index], 0);
      glClear(GL_DEPTH_BUFFER_BIT);
      const glm::mat4 view =
          glm::lookAt(light.position, light.position + DIRECTIONS[face],
                      UPS[face]);
      drawCasters(parCasters, projection * view, &light.position, light.range);
    }
  }
  glDisable(GL_POLYGON_OFFSET_FILL);
  glCullFace(GL_BACK);
  glDisable(GL_CULL_FACE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  m_last_input_hash = input_hash;
  m_has_cached_frame = true;
  return m_frame;
}

}  // namespace kage::render
