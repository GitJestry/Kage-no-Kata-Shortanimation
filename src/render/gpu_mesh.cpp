#include "render/gpu_mesh.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

constexpr GLuint POSITION_ATTRIBUTE = 0;
constexpr GLuint NORMAL_ATTRIBUTE = 1;
constexpr GLuint TEX_COORD_ATTRIBUTE = 2;
constexpr GLuint TANGENT_ATTRIBUTE = 3;
constexpr GLuint BASE_COLOR_TEXTURE_UNIT = 0;
constexpr GLuint NORMAL_TEXTURE_UNIT = 1;
constexpr GLuint METALLIC_ROUGHNESS_TEXTURE_UNIT = 2;
constexpr GLuint EMISSIVE_TEXTURE_UNIT = 3;
constexpr GLsizei STATIC_VERTEX_STRIDE =
    static_cast<GLsizei>(sizeof(kage::assets::StaticVertex));
constexpr glm::vec4 DEFAULT_BASE_COLOR_FACTOR{1.0f};
constexpr std::array<unsigned char, 4> FALLBACK_TEXTURE_PIXELS{
    255, 255, 255, 255};

struct TextureUploadPlan final {
  bool used = false;
  kage::render::TextureColorSpace color_space =
      kage::render::TextureColorSpace::Linear;
};

[[nodiscard]] GLsizei checkedIndexCount(std::size_t parIndexCount) {
  if (parIndexCount >
      static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
    throw std::runtime_error("Primitive index count exceeds GLsizei range");
  }

  return static_cast<GLsizei>(parIndexCount);
}

[[nodiscard]] GLint getMinFilter(int parFilter) {
  switch (parFilter) {
    case GL_NEAREST:
    case GL_LINEAR:
    case GL_NEAREST_MIPMAP_NEAREST:
    case GL_LINEAR_MIPMAP_NEAREST:
    case GL_NEAREST_MIPMAP_LINEAR:
    case GL_LINEAR_MIPMAP_LINEAR:
      return parFilter;
    default:
      return GL_LINEAR_MIPMAP_LINEAR;
  }
}

[[nodiscard]] GLint getMagFilter(int parFilter) {
  switch (parFilter) {
    case GL_NEAREST:
    case GL_LINEAR:
      return parFilter;
    default:
      return GL_LINEAR;
  }
}

[[nodiscard]] GLint getWrapMode(int parWrapMode) {
  switch (parWrapMode) {
    case GL_CLAMP_TO_EDGE:
    case GL_MIRRORED_REPEAT:
    case GL_REPEAT:
      return parWrapMode;
    default:
      return GL_REPEAT;
  }
}

[[nodiscard]] std::vector<TextureUploadPlan> buildTextureUploadPlan(
    const kage::assets::StaticModel& parModel) {
  std::vector<TextureUploadPlan> upload_plan(parModel.textures.size());
  for (const kage::assets::StaticMaterial& material : parModel.materials) {
    const auto mark_linear = [&](const kage::assets::MaterialTextureSlot& slot) {
      if (slot.isValid() && static_cast<std::size_t>(slot.texture_index) <
                                upload_plan.size()) {
        upload_plan[slot.texture_index].used = true;
      }
    };
    const auto mark_srgb = [&](const kage::assets::MaterialTextureSlot& slot) {
      if (slot.isValid() && static_cast<std::size_t>(slot.texture_index) <
                                upload_plan.size()) {
        upload_plan[slot.texture_index].used = true;
        upload_plan[slot.texture_index].color_space =
            kage::render::TextureColorSpace::Srgb;
      }
    };
    mark_srgb(material.base_color_texture);
    mark_srgb(material.emissive_texture);
    mark_linear(material.normal_texture);
    mark_linear(material.metallic_roughness_texture);
  }

  return upload_plan;
}

}  // namespace

namespace kage::render {

void GpuMesh::upload(const assets::StaticModel& parModel) {
  clear();
  m_fallback_texture.upload(1, 1, 4, FALLBACK_TEXTURE_PIXELS);
  m_fallback_texture.setSampling(GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE,
                                 GL_CLAMP_TO_EDGE);

  const std::vector<TextureUploadPlan> texture_upload_plan =
      buildTextureUploadPlan(parModel);
  m_textures.resize(parModel.textures.size());
  for (std::size_t texture_index = 0; texture_index < parModel.textures.size();
       ++texture_index) {
    if (!texture_upload_plan[texture_index].used) {
      continue;
    }

    const assets::StaticTexture& texture = parModel.textures[texture_index];
    if (texture.image_index == assets::INVALID_TEXTURE_INDEX ||
        static_cast<std::size_t>(texture.image_index) >=
            parModel.images.size()) {
      throw std::runtime_error("Static texture image index is out of range");
    }

    const assets::StaticImage& image =
        parModel.images[static_cast<std::size_t>(texture.image_index)];
    Texture2D& gpu_texture = m_textures[texture_index];
    gpu_texture.upload(image.width, image.height, image.component_count,
                       image.pixels,
                       texture_upload_plan[texture_index].color_space);
    gpu_texture.setSampling(getMinFilter(texture.min_filter),
                            getMagFilter(texture.mag_filter),
                            getWrapMode(texture.wrap_s),
                            getWrapMode(texture.wrap_t));
  }
  Texture2D::unbind(BASE_COLOR_TEXTURE_UNIT);

  m_primitives.reserve(parModel.primitives.size());

  for (const assets::StaticPrimitive& primitive : parModel.primitives) {
    if (primitive.vertices.empty() || primitive.indices.empty()) {
      continue;
    }

    PrimitiveGpuData gpu_primitive;
    gpu_primitive.transform = primitive.transform;
    gpu_primitive.index_count = checkedIndexCount(primitive.indices.size());
    if (primitive.material_index != assets::INVALID_MATERIAL_INDEX) {
      if (static_cast<std::size_t>(primitive.material_index) >=
          parModel.materials.size()) {
        throw std::runtime_error(
            "Static primitive material index is out of range");
      }

      const assets::StaticMaterial& material =
          parModel.materials[static_cast<std::size_t>(primitive.material_index)];
      gpu_primitive.base_color_factor = material.base_color_factor;
      gpu_primitive.base_color_texture = material.base_color_texture;
      gpu_primitive.normal_texture = material.normal_texture;
      gpu_primitive.metallic_roughness_texture =
          material.metallic_roughness_texture;
      gpu_primitive.emissive_texture = material.emissive_texture;
      gpu_primitive.metallic_factor = material.metallic_factor;
      gpu_primitive.roughness_factor = material.roughness_factor;
      gpu_primitive.normal_scale = material.normal_scale;
      gpu_primitive.alpha_cutoff = material.alpha_cutoff;
      gpu_primitive.emissive_factor = material.emissive_factor;
      gpu_primitive.alpha_blend = material.alpha_blend;
      gpu_primitive.alpha_mask = material.alpha_mask;
      gpu_primitive.double_sided = material.double_sided;
    } else {
      gpu_primitive.base_color_factor = DEFAULT_BASE_COLOR_FACTOR;
    }

    gpu_primitive.vertex_array.create();
    gpu_primitive.vertex_buffer.create(GL_ARRAY_BUFFER);
    gpu_primitive.index_buffer.create(GL_ELEMENT_ARRAY_BUFFER);

    gpu_primitive.vertex_array.bind();
    gpu_primitive.vertex_buffer.setData(
        primitive.vertices.size() * sizeof(assets::StaticVertex),
        primitive.vertices.data(), GL_STATIC_DRAW);
    gpu_primitive.index_buffer.setData(
        primitive.indices.size() * sizeof(std::uint32_t),
        primitive.indices.data(), GL_STATIC_DRAW);

    gpu_primitive.vertex_array.setFloatAttribute(
        POSITION_ATTRIBUTE, 3, GL_FLOAT, STATIC_VERTEX_STRIDE,
        offsetof(assets::StaticVertex, position));
    gpu_primitive.vertex_array.setFloatAttribute(
        NORMAL_ATTRIBUTE, 3, GL_FLOAT, STATIC_VERTEX_STRIDE,
        offsetof(assets::StaticVertex, normal));
    gpu_primitive.vertex_array.setFloatAttribute(
        TEX_COORD_ATTRIBUTE, 2, GL_FLOAT, STATIC_VERTEX_STRIDE,
        offsetof(assets::StaticVertex, tex_coord));
    gpu_primitive.vertex_array.setFloatAttribute(
        TANGENT_ATTRIBUTE, 4, GL_FLOAT, STATIC_VERTEX_STRIDE,
        offsetof(assets::StaticVertex, tangent));

    m_index_count += primitive.indices.size();
    m_primitives.push_back(std::move(gpu_primitive));
  }

  VertexArray::unbind();
  GpuBuffer::unbind(GL_ARRAY_BUFFER);
  GpuBuffer::unbind(GL_ELEMENT_ARRAY_BUFFER);
}

void GpuMesh::draw(const ShaderProgram& parShader,
                   const glm::mat4& parViewProjection,
                   const glm::vec3& parCameraPosition,
                   const glm::mat4& parEntityTransform,
                   const lighting::LightingState& parLighting,
                   float parEntityOpacity,
                   MaterialDebugMode parDebugMode) const {
  if (m_primitives.empty()) {
    return;
  }

  parShader.use();
  parShader.setMat4("u_view_projection", parViewProjection);
  parShader.setVec3("u_camera_position", parCameraPosition);
  parShader.setInt("u_base_color_texture", BASE_COLOR_TEXTURE_UNIT);
  parShader.setInt("u_normal_texture", NORMAL_TEXTURE_UNIT);
  parShader.setInt("u_metallic_roughness_texture",
                   METALLIC_ROUGHNESS_TEXTURE_UNIT);
  parShader.setInt("u_emissive_texture", EMISSIVE_TEXTURE_UNIT);
  parShader.setVec3("u_ambient_color", parLighting.ambient_color);
  parShader.setVec3("u_light_direction",
                    glm::normalize(parLighting.sun.direction));
  parShader.setVec3("u_light_color", parLighting.sun.color);
  parShader.setFloat("u_light_intensity", parLighting.sun.intensity);
  parShader.setInt("u_point_light_enabled",
                   parLighting.point.enabled ? 1 : 0);
  parShader.setVec3("u_point_light_position", parLighting.point.position);
  parShader.setVec3("u_point_light_color", parLighting.point.color);
  parShader.setFloat("u_point_light_intensity",
                     parLighting.point.intensity);
  parShader.setFloat("u_point_light_range", parLighting.point.range);
  parShader.setInt("u_material_debug_mode",
                   static_cast<int>(parDebugMode));

  for (const PrimitiveGpuData& primitive : m_primitives) {
    parShader.setMat4("u_model", parEntityTransform * primitive.transform);
    parShader.setVec4("u_base_color_factor", primitive.base_color_factor);
    parShader.setFloat("u_metallic_factor", primitive.metallic_factor);
    parShader.setFloat("u_roughness_factor", primitive.roughness_factor);
    parShader.setFloat("u_normal_scale", primitive.normal_scale);
    parShader.setFloat("u_alpha_cutoff", primitive.alpha_cutoff);
    parShader.setVec3("u_emissive_factor", primitive.emissive_factor);
    parShader.setInt("u_alpha_mask", primitive.alpha_mask ? 1 : 0);
    parShader.setFloat("u_entity_opacity", parEntityOpacity);

    const auto bind_slot = [&](const char* has_name, const char* offset_name,
                               const char* scale_name,
                               const char* rotation_name,
                               const assets::MaterialTextureSlot& slot,
                               GLuint texture_unit) {
      const bool has_texture =
          slot.isValid() && static_cast<std::size_t>(slot.texture_index) <
                                m_textures.size() &&
          m_textures[slot.texture_index].isValid();
      parShader.setInt(has_name, has_texture ? 1 : 0);
      parShader.setVec2(offset_name, slot.transform.offset);
      parShader.setVec2(scale_name, slot.transform.scale);
      parShader.setFloat(rotation_name, slot.transform.rotation);
      if (has_texture) {
        m_textures[slot.texture_index].bind(texture_unit);
      } else {
        m_fallback_texture.bind(texture_unit);
      }
    };

    bind_slot("u_has_base_color_texture", "u_base_color_offset",
              "u_base_color_scale", "u_base_color_rotation",
              primitive.base_color_texture, BASE_COLOR_TEXTURE_UNIT);
    bind_slot("u_has_normal_texture", "u_normal_offset", "u_normal_scale_uv",
              "u_normal_rotation", primitive.normal_texture,
              NORMAL_TEXTURE_UNIT);
    bind_slot("u_has_metallic_roughness_texture", "u_metallic_roughness_offset",
              "u_metallic_roughness_scale",
              "u_metallic_roughness_rotation",
              primitive.metallic_roughness_texture,
              METALLIC_ROUGHNESS_TEXTURE_UNIT);
    bind_slot("u_has_emissive_texture", "u_emissive_offset",
              "u_emissive_scale", "u_emissive_rotation",
              primitive.emissive_texture, EMISSIVE_TEXTURE_UNIT);

    primitive.vertex_array.bind();
    glDrawElements(GL_TRIANGLES, primitive.index_count, GL_UNSIGNED_INT,
                   nullptr);
  }

  VertexArray::unbind();
  Texture2D::unbind(BASE_COLOR_TEXTURE_UNIT);
  Texture2D::unbind(NORMAL_TEXTURE_UNIT);
  Texture2D::unbind(METALLIC_ROUGHNESS_TEXTURE_UNIT);
  Texture2D::unbind(EMISSIVE_TEXTURE_UNIT);
}

void GpuMesh::drawOutline(const ShaderProgram& parShader,
                          const glm::mat4& parViewProjection,
                          const glm::mat4& parEntityTransform,
                          const glm::vec4& parColor,
                          float parThickness) const {
  if (m_primitives.empty()) {
    return;
  }

  parShader.use();
  parShader.setMat4("u_view_projection", parViewProjection);
  parShader.setVec4("u_outline_color", parColor);
  parShader.setFloat("u_outline_thickness", parThickness);

  glEnable(GL_CULL_FACE);
  glCullFace(GL_FRONT);
  for (const PrimitiveGpuData& primitive : m_primitives) {
    parShader.setMat4("u_model", parEntityTransform * primitive.transform);
    primitive.vertex_array.bind();
    glDrawElements(GL_TRIANGLES, primitive.index_count, GL_UNSIGNED_INT,
                   nullptr);
  }
  glCullFace(GL_BACK);
  glDisable(GL_CULL_FACE);
  VertexArray::unbind();
}

void GpuMesh::clear() {
  m_primitives.clear();
  m_fallback_texture.release();
  m_textures.clear();
  m_index_count = 0;
}

std::size_t GpuMesh::getPrimitiveCount() const {
  return m_primitives.size();
}

std::size_t GpuMesh::getIndexCount() const {
  return m_index_count;
}

bool GpuMesh::isValid() const {
  return !m_primitives.empty();
}

}  // namespace kage::render
