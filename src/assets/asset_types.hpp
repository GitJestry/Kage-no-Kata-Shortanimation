#pragma once

#include "math/bounds.hpp"
#include "math/transform.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace kage::assets {

struct AssetId final {
  std::size_t value = std::numeric_limits<std::size_t>::max();

  [[nodiscard]] bool isValid() const {
    return value != std::numeric_limits<std::size_t>::max();
  }

  friend bool operator==(AssetId parLeft, AssetId parRight) = default;
};

inline constexpr std::uint32_t INVALID_MATERIAL_INDEX =
    std::numeric_limits<std::uint32_t>::max();
inline constexpr std::uint32_t INVALID_TEXTURE_INDEX =
    std::numeric_limits<std::uint32_t>::max();
inline constexpr std::uint32_t INVALID_NODE_INDEX =
    std::numeric_limits<std::uint32_t>::max();
inline constexpr std::uint32_t INVALID_MESH_INDEX =
    std::numeric_limits<std::uint32_t>::max();
inline constexpr std::uint32_t INVALID_SKIN_INDEX =
    std::numeric_limits<std::uint32_t>::max();
inline constexpr int DEFAULT_STATIC_TEXTURE_MIN_FILTER = 9987;
inline constexpr int DEFAULT_STATIC_TEXTURE_MAG_FILTER = 9729;
inline constexpr int DEFAULT_STATIC_TEXTURE_WRAP = 10497;
inline constexpr int DEFAULT_STATIC_IMAGE_PIXEL_TYPE = 5121;

struct StaticVertex final {
  glm::vec3 position{};
  glm::vec3 normal{};
  glm::vec4 tangent{};
  glm::vec2 tex_coord{};
};

struct SkinInfluence final {
  glm::uvec4 joints{};
  glm::vec4 weights{};
};

struct StaticImage final {
  std::string name;
  std::vector<unsigned char> pixels;
  int width = 0;
  int height = 0;
  int component_count = 0;
  int pixel_type = DEFAULT_STATIC_IMAGE_PIXEL_TYPE;
};

struct StaticTexture final {
  std::string name;
  std::uint32_t image_index = INVALID_TEXTURE_INDEX;
  int min_filter = DEFAULT_STATIC_TEXTURE_MIN_FILTER;
  int mag_filter = DEFAULT_STATIC_TEXTURE_MAG_FILTER;
  int wrap_s = DEFAULT_STATIC_TEXTURE_WRAP;
  int wrap_t = DEFAULT_STATIC_TEXTURE_WRAP;
};

struct TextureTransform final {
  glm::vec2 offset{0.0f};
  glm::vec2 scale{1.0f};
  float rotation = 0.0f;
};

struct MaterialTextureSlot final {
  std::uint32_t texture_index = INVALID_TEXTURE_INDEX;
  TextureTransform transform;

  [[nodiscard]] bool isValid() const;
};

struct StaticMaterial final {
  std::string name;
  glm::vec4 base_color_factor{1.0f};
  MaterialTextureSlot base_color_texture;
  MaterialTextureSlot normal_texture;
  MaterialTextureSlot metallic_roughness_texture;
  MaterialTextureSlot emissive_texture;
  float metallic_factor = 1.0f;
  float roughness_factor = 1.0f;
  float normal_scale = 1.0f;
  float alpha_cutoff = 0.5f;
  glm::vec3 emissive_factor{0.0f};
  bool alpha_blend = false;
  bool alpha_mask = false;
  bool double_sided = false;
};

struct StaticPrimitive final {
  std::string name;
  std::vector<StaticVertex> vertices;
  std::vector<SkinInfluence> skin_influences;
  std::vector<std::uint32_t> indices;
  glm::mat4 transform{1.0f};
  std::uint32_t node_index = INVALID_NODE_INDEX;
  std::uint32_t skin_index = INVALID_SKIN_INDEX;
  std::uint32_t material_index = INVALID_MATERIAL_INDEX;
  math::Bounds3 bounds;

  [[nodiscard]] bool hasSkinInfluences() const;
};

inline bool StaticPrimitive::hasSkinInfluences() const {
  return skin_influences.size() == vertices.size();
}

inline bool MaterialTextureSlot::isValid() const {
  return texture_index != INVALID_TEXTURE_INDEX;
}

struct GltfAssetStats final {
  std::size_t scene_count = 0;
  std::size_t node_count = 0;
  std::size_t mesh_count = 0;
  std::size_t primitive_count = 0;
  std::size_t vertex_count = 0;
  std::size_t skinned_vertex_count = 0;
  std::size_t index_count = 0;
  std::size_t triangle_count = 0;
  std::size_t material_count = 0;
  std::size_t texture_count = 0;
  std::size_t image_count = 0;
  std::size_t skin_count = 0;
  std::size_t joint_count = 0;
  std::size_t animation_count = 0;
  std::size_t animation_channel_count = 0;
  std::size_t marker_count = 0;
};

struct StaticModel final {
  std::filesystem::path source_path;
  std::string scene_name;
  std::string import_warning;
  std::vector<StaticPrimitive> primitives;
  std::vector<StaticImage> images;
  std::vector<StaticTexture> textures;
  std::vector<StaticMaterial> materials;
  GltfAssetStats stats;
  math::Bounds3 bounds;
};

struct GltfNode final {
  std::string name;
  std::uint32_t parent_index = INVALID_NODE_INDEX;
  std::vector<std::uint32_t> children;
  math::Transform local_transform;
  glm::mat4 global_transform{1.0f};
  std::uint32_t mesh_index = INVALID_MESH_INDEX;
  std::uint32_t skin_index = INVALID_SKIN_INDEX;
};

struct GltfSkin final {
  std::string name;
  std::uint32_t skeleton_root = INVALID_NODE_INDEX;
  std::vector<std::uint32_t> joints;
  std::vector<glm::mat4> inverse_bind_matrices;
};

enum class AnimationTargetPath {
  Translation,
  Rotation,
  Scale
};

enum class AnimationInterpolation {
  Linear,
  Step
};

struct AnimationSampler final {
  AnimationInterpolation interpolation = AnimationInterpolation::Linear;
  std::vector<float> input_times;
  std::vector<glm::vec3> translations;
  std::vector<glm::quat> rotations;
  std::vector<glm::vec3> scales;
};

struct AnimationChannel final {
  std::uint32_t target_node = INVALID_NODE_INDEX;
  AnimationTargetPath target_path = AnimationTargetPath::Translation;
  std::uint32_t sampler_index = 0;
};

struct AnimationClip final {
  std::string name;
  float duration_seconds = 0.0f;
  std::vector<AnimationSampler> samplers;
  std::vector<AnimationChannel> channels;
};

struct GltfMarker final {
  std::string name;
  std::uint32_t node_index = INVALID_NODE_INDEX;
  glm::mat4 transform{1.0f};
};

struct GltfDocument final {
  std::filesystem::path source_path;
  std::string scene_name;
  std::string import_warning;
  StaticModel static_model;
  std::vector<GltfNode> nodes;
  std::vector<std::uint32_t> root_nodes;
  std::vector<GltfSkin> skins;
  std::vector<AnimationClip> animation_clips;
  std::vector<GltfMarker> markers;
  GltfAssetStats stats;
  math::Bounds3 bounds;
};

using MeshData = StaticModel;
using MaterialData = StaticMaterial;
using SkeletonData = GltfSkin;
using AnimationClipData = AnimationClip;
using ModelAsset = GltfDocument;

}  // namespace kage::assets
