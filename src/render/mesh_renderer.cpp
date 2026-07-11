#include "render/mesh_renderer.hpp"

namespace {

constexpr char STATIC_MESH_VERTEX_SHADER[] = R"(#version 410 core
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;
layout (location = 3) in vec4 inTangent;
layout (location = 4) in uvec4 inJoints;
layout (location = 5) in vec4 inWeights;

uniform mat4 u_model;
uniform mat4 u_view_projection;
uniform bool u_skinning_enabled;
uniform mat4 u_joint_matrices[128];

out vec3 worldNormal;
out vec3 worldPosition;
out vec3 worldTangent;
out float tangentSign;
out vec2 meshTexCoord;

void main() {
  mat4 skin = mat4(1.0);
  if (u_skinning_enabled) {
    skin = u_joint_matrices[inJoints.x] * inWeights.x +
           u_joint_matrices[inJoints.y] * inWeights.y +
           u_joint_matrices[inJoints.z] * inWeights.z +
           u_joint_matrices[inJoints.w] * inWeights.w;
  }
  vec4 skinned_position = skin * vec4(inPosition, 1.0);
  vec4 world_position = u_model * skinned_position;
  mat3 normalMatrix = mat3(transpose(inverse(u_model)));
  worldNormal = normalMatrix * mat3(skin) * inNormal;
  worldTangent = normalMatrix * mat3(skin) * inTangent.xyz;
  tangentSign = inTangent.w;
  worldPosition = world_position.xyz;
  meshTexCoord = inTexCoord;
  gl_Position = u_view_projection * world_position;
}
)";

constexpr char STATIC_MESH_FRAGMENT_SHADER[] = R"(#version 410 core
in vec3 worldNormal;
in vec3 worldPosition;
in vec3 worldTangent;
in float tangentSign;
in vec2 meshTexCoord;

uniform vec4 u_base_color_factor;
uniform bool u_has_base_color_texture;
uniform sampler2D u_base_color_texture;
uniform vec2 u_base_color_offset;
uniform vec2 u_base_color_scale;
uniform float u_base_color_rotation;
uniform bool u_has_normal_texture;
uniform sampler2D u_normal_texture;
uniform vec2 u_normal_offset;
uniform vec2 u_normal_scale_uv;
uniform float u_normal_rotation;
uniform float u_normal_scale;
uniform bool u_has_metallic_roughness_texture;
uniform sampler2D u_metallic_roughness_texture;
uniform vec2 u_metallic_roughness_offset;
uniform vec2 u_metallic_roughness_scale;
uniform float u_metallic_roughness_rotation;
uniform bool u_has_emissive_texture;
uniform sampler2D u_emissive_texture;
uniform vec2 u_emissive_offset;
uniform vec2 u_emissive_scale;
uniform float u_emissive_rotation;
uniform float u_metallic_factor;
uniform float u_roughness_factor;
uniform float u_alpha_cutoff;
uniform bool u_alpha_mask;
uniform float u_entity_opacity;
uniform vec3 u_emissive_factor;
uniform vec3 u_ambient_diffuse;
uniform vec3 u_ambient_specular;
uniform float u_exposure;
uniform bool u_sun_enabled;
uniform vec3 u_sun_direction_to_light;
uniform vec3 u_sun_color;
uniform float u_sun_intensity;
uniform int u_point_light_count;
uniform vec3 u_point_light_positions[8];
uniform vec3 u_point_light_colors[8];
uniform float u_point_light_intensities[8];
uniform float u_point_light_ranges[8];
uniform vec3 u_camera_position;
uniform int u_material_debug_mode;
uniform bool u_solid_mode;

out vec4 fragColor;

vec2 transformUv(vec2 uv, vec2 offset, vec2 scale, float rotation) {
  vec2 scaled = uv * scale;
  float c = cos(rotation);
  float s = sin(rotation);
  return vec2(c * scaled.x - s * scaled.y,
              s * scaled.x + c * scaled.y) + offset;
}

float distributionGgx(vec3 normal, vec3 halfDirection, float roughness) {
  float alpha = roughness * roughness;
  float alphaSquared = alpha * alpha;
  float ndoth = max(dot(normal, halfDirection), 0.0);
  float ndothSquared = ndoth * ndoth;
  float denominator =
      ndothSquared * (alphaSquared - 1.0) + 1.0;
  return alphaSquared / max(3.14159265 * denominator * denominator, 0.0001);
}

float geometrySchlickGgx(float ndotv, float roughness) {
  float r = roughness + 1.0;
  float k = (r * r) / 8.0;
  return ndotv / max(ndotv * (1.0 - k) + k, 0.0001);
}

float geometrySmith(vec3 normal, vec3 viewDirection, vec3 lightDirection,
                    float roughness) {
  float ndotv = max(dot(normal, viewDirection), 0.0);
  float ndotl = max(dot(normal, lightDirection), 0.0);
  return geometrySchlickGgx(ndotv, roughness) *
         geometrySchlickGgx(ndotl, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 f0) {
  return f0 + (1.0 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 evaluatePbrLight(vec3 baseColor, vec3 normal, vec3 viewDirection,
                      vec3 lightDirection, vec3 lightColor, float intensity,
                      float metallic, float roughness) {
  float ndotl = max(dot(normal, lightDirection), 0.0);
  if (ndotl <= 0.0 || intensity <= 0.0) {
    return vec3(0.0);
  }

  vec3 halfDirection = normalize(viewDirection + lightDirection);
  vec3 f0 = mix(vec3(0.04), baseColor, metallic);
  float distribution = distributionGgx(normal, halfDirection, roughness);
  float geometry = geometrySmith(normal, viewDirection, lightDirection, roughness);
  vec3 fresnel = fresnelSchlick(max(dot(halfDirection, viewDirection), 0.0), f0);
  vec3 numerator = distribution * geometry * fresnel;
  float denominator =
      4.0 * max(dot(normal, viewDirection), 0.0) * ndotl + 0.0001;
  vec3 specular = numerator / denominator;
  vec3 diffuse = (vec3(1.0) - fresnel) * (1.0 - metallic) *
                 baseColor / 3.14159265;
  return (diffuse + specular) * lightColor * intensity * ndotl;
}

void main() {
  if (u_solid_mode) {
    vec3 normal = normalize(worldNormal);
    float light = 0.28 + 0.72 * max(dot(normal, normalize(u_sun_direction_to_light)), 0.0);
    fragColor = vec4(vec3(0.62, 0.65, 0.69) * light, u_entity_opacity);
    return;
  }
  vec3 normal = length(worldNormal) > 0.001
      ? normalize(worldNormal)
      : vec3(0.0, 0.0, 1.0);
  if (u_has_normal_texture && length(worldTangent) > 0.001) {
    vec3 tangent = normalize(worldTangent);
    tangent = normalize(tangent - normal * dot(normal, tangent));
    vec3 bitangent = normalize(cross(normal, tangent)) * tangentSign;
    mat3 tangentSpace = mat3(tangent, bitangent, normal);
    vec3 mappedNormal =
        texture(u_normal_texture,
                transformUv(meshTexCoord, u_normal_offset,
                            u_normal_scale_uv, u_normal_rotation)).xyz *
            2.0 -
        1.0;
    mappedNormal.xy *= u_normal_scale;
    normal = normalize(tangentSpace * mappedNormal);
  }

  vec3 sunDirectionToLight = normalize(u_sun_direction_to_light);
  vec4 baseColor = u_base_color_factor;
  if (u_has_base_color_texture) {
    vec4 textureColor =
        texture(u_base_color_texture,
                transformUv(meshTexCoord, u_base_color_offset,
                            u_base_color_scale, u_base_color_rotation));
    baseColor.rgb *= textureColor.rgb;
    baseColor.a *= textureColor.a;
  }

  float alpha = baseColor.a * u_entity_opacity;
  if (u_alpha_mask && alpha < u_alpha_cutoff) {
    discard;
  }

  float roughness = clamp(u_roughness_factor, 0.04, 1.0);
  float metallic = clamp(u_metallic_factor, 0.0, 1.0);
  if (u_has_metallic_roughness_texture) {
    vec4 metallicRoughness =
        texture(u_metallic_roughness_texture,
                transformUv(meshTexCoord, u_metallic_roughness_offset,
                            u_metallic_roughness_scale,
                            u_metallic_roughness_rotation));
    roughness = clamp(roughness * metallicRoughness.g, 0.04, 1.0);
    metallic = clamp(metallic * metallicRoughness.b, 0.0, 1.0);
  }

  vec3 emissive = u_emissive_factor;
  if (u_has_emissive_texture) {
    emissive *= texture(u_emissive_texture,
                        transformUv(meshTexCoord, u_emissive_offset,
                                    u_emissive_scale,
                                    u_emissive_rotation)).rgb;
  }

  if (u_material_debug_mode == 1) {
    fragColor = vec4(baseColor.rgb, alpha);
    return;
  }
  if (u_material_debug_mode == 2) {
    fragColor = vec4(normal * 0.5 + 0.5, alpha);
    return;
  }
  if (u_material_debug_mode == 3) {
    fragColor = vec4(vec3(roughness), alpha);
    return;
  }
  if (u_material_debug_mode == 4) {
    fragColor = vec4(vec3(metallic), alpha);
    return;
  }
  if (u_material_debug_mode == 5) {
    fragColor = vec4(fract(meshTexCoord), 0.0, alpha);
    return;
  }

  vec3 viewDirection = normalize(u_camera_position - worldPosition);
  vec3 color = baseColor.rgb * u_ambient_diffuse * (1.0 - metallic * 0.45);
  if (u_sun_enabled) {
    color += evaluatePbrLight(baseColor.rgb, normal, viewDirection,
                              sunDirectionToLight, u_sun_color,
                              u_sun_intensity, metallic, roughness);
  }
  for (int lightIndex = 0; lightIndex < u_point_light_count; ++lightIndex) {
    vec3 toLight = u_point_light_positions[lightIndex] - worldPosition;
    float lightDistance = length(toLight);
    vec3 pointDirection = lightDistance > 0.001
        ? toLight / lightDistance
        : vec3(0.0, 1.0, 0.0);
    float attenuation =
        pow(clamp(1.0 - lightDistance /
                          max(u_point_light_ranges[lightIndex], 0.001),
                  0.0, 1.0), 2.0);
    color += evaluatePbrLight(baseColor.rgb, normal, viewDirection,
                              pointDirection,
                              u_point_light_colors[lightIndex],
                              u_point_light_intensities[lightIndex] *
                                  attenuation,
                              metallic, roughness);
  }

  vec3 f0 = mix(vec3(0.04), baseColor.rgb, metallic);
  vec3 skySpecular = fresnelSchlick(max(dot(normal, viewDirection), 0.0), f0);
  color += skySpecular * u_ambient_specular *
           (0.18 + metallic * 0.55);
  color += emissive;
  fragColor = vec4(color * u_exposure, alpha);
}
)";

constexpr char PICKING_VERTEX_SHADER[] = R"(#version 410 core
layout (location = 0) in vec3 inPosition;
layout (location = 2) in vec2 inTexCoord;
layout (location = 4) in uvec4 inJoints;
layout (location = 5) in vec4 inWeights;

uniform mat4 u_model;
uniform mat4 u_view_projection;
uniform bool u_skinning_enabled;
uniform mat4 u_joint_matrices[128];

out vec2 meshTexCoord;

void main() {
  mat4 skin = mat4(1.0);
  if (u_skinning_enabled) {
    skin = u_joint_matrices[inJoints.x] * inWeights.x +
           u_joint_matrices[inJoints.y] * inWeights.y +
           u_joint_matrices[inJoints.z] * inWeights.z +
           u_joint_matrices[inJoints.w] * inWeights.w;
  }
  meshTexCoord = inTexCoord;
  gl_Position = u_view_projection * u_model * skin * vec4(inPosition, 1.0);
}
)";

constexpr char PICKING_FRAGMENT_SHADER[] = R"(#version 410 core
uniform uint u_entity_id;
uniform bool u_alpha_mask;
uniform float u_alpha_cutoff;
uniform float u_base_color_alpha;
uniform bool u_has_base_color_texture;
uniform sampler2D u_base_color_texture;
uniform vec2 u_base_color_offset;
uniform vec2 u_base_color_scale;
uniform float u_base_color_rotation;

in vec2 meshTexCoord;
layout (location = 0) out uint outEntityId;

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
  outEntityId = u_entity_id;
}
)";

constexpr char OUTLINE_VERTEX_SHADER[] = R"(#version 410 core
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 4) in uvec4 inJoints;
layout (location = 5) in vec4 inWeights;

uniform mat4 u_model;
uniform mat4 u_view_projection;
uniform float u_outline_thickness;
uniform bool u_skinning_enabled;
uniform mat4 u_joint_matrices[128];

void main() {
  mat4 skin = mat4(1.0);
  if (u_skinning_enabled) {
    skin = u_joint_matrices[inJoints.x] * inWeights.x +
           u_joint_matrices[inJoints.y] * inWeights.y +
           u_joint_matrices[inJoints.z] * inWeights.z +
           u_joint_matrices[inJoints.w] * inWeights.w;
  }
  mat3 normalMatrix = mat3(transpose(inverse(u_model)));
  vec3 normal = normalize(normalMatrix * mat3(skin) * inNormal);
  vec4 worldPosition =
      u_model * skin * vec4(inPosition, 1.0) +
      vec4(normal * u_outline_thickness, 0.0);
  gl_Position = u_view_projection * worldPosition;
}
)";

constexpr char OUTLINE_FRAGMENT_SHADER[] = R"(#version 410 core
uniform vec4 u_outline_color;

out vec4 fragColor;

void main() {
  fragColor = u_outline_color;
}
)";

}  // namespace

namespace kage::render {

MeshRenderer::MeshRenderer() {
  m_shader.create(STATIC_MESH_VERTEX_SHADER, STATIC_MESH_FRAGMENT_SHADER);
  m_outline_shader.create(OUTLINE_VERTEX_SHADER, OUTLINE_FRAGMENT_SHADER);
  m_picking_shader.create(PICKING_VERTEX_SHADER, PICKING_FRAGMENT_SHADER);
}

void MeshRenderer::drawPicking(
    const GpuMesh& parMesh, const glm::mat4& parViewProjection,
    const glm::mat4& parEntityTransform,
    std::span<const std::vector<glm::mat4>> parSkinMatrices,
    std::uint32_t parEntityId) const {
  parMesh.drawPicking(m_picking_shader, parViewProjection, parEntityTransform,
                      parSkinMatrices, parEntityId);
}

void MeshRenderer::draw(const GpuMesh& parMesh,
                              const glm::mat4& parViewProjection,
                              const glm::vec3& parCameraPosition,
                              const glm::mat4& parEntityTransform,
                              const lighting::LightingState& parLighting,
                              std::span<const std::vector<glm::mat4>>
                                  parSkinMatrices,
                              float parEntityOpacity,
                              MaterialDebugMode parDebugMode,
                              bool parSolidMode,
                              std::size_t parLod) const {
  parMesh.draw(m_shader, parViewProjection, parCameraPosition,
               parEntityTransform, parLighting, parSkinMatrices,
               parEntityOpacity,
               parDebugMode, parSolidMode, parLod);
}

void MeshRenderer::drawOutline(const GpuMesh& parMesh,
                                     const glm::mat4& parViewProjection,
                                     const glm::mat4& parEntityTransform,
                                     std::span<const std::vector<glm::mat4>>
                                         parSkinMatrices,
                                     const glm::vec4& parColor,
                                     float parThickness) const {
  parMesh.drawOutline(m_outline_shader, parViewProjection, parEntityTransform,
                      parSkinMatrices, parColor, parThickness);
}

}  // namespace kage::render
