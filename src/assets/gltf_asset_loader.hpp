#pragma once

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace kage::assets {

inline constexpr std::uint32_t INVALID_MATERIAL_INDEX =
    std::numeric_limits<std::uint32_t>::max();

struct AssetBounds final {
  glm::vec3 min{};
  glm::vec3 max{};
  bool is_valid = false;

  void includePoint(const glm::vec3& parPoint);
  [[nodiscard]] glm::vec3 getSize() const;
};

struct StaticVertex final {
  glm::vec3 position{};
  glm::vec3 normal{};
  glm::vec2 tex_coord{};
};

struct StaticPrimitive final {
  std::string name;
  std::vector<StaticVertex> vertices;
  std::vector<std::uint32_t> indices;
  glm::mat4 transform{1.0f};
  std::uint32_t material_index = INVALID_MATERIAL_INDEX;
  AssetBounds bounds;
};

struct GltfAssetStats final {
  std::size_t scene_count = 0;
  std::size_t node_count = 0;
  std::size_t mesh_count = 0;
  std::size_t primitive_count = 0;
  std::size_t vertex_count = 0;
  std::size_t index_count = 0;
  std::size_t triangle_count = 0;
  std::size_t material_count = 0;
  std::size_t texture_count = 0;
  std::size_t image_count = 0;
  std::size_t skin_count = 0;
  std::size_t animation_count = 0;
};

struct StaticModel final {
  std::filesystem::path source_path;
  std::string scene_name;
  std::string import_warning;
  std::vector<StaticPrimitive> primitives;
  GltfAssetStats stats;
  AssetBounds bounds;
};

class GltfAssetLoader final {
 public:
  [[nodiscard]] StaticModel loadStaticModel(
      const std::filesystem::path& parPath) const;
};

}  // namespace kage::assets
