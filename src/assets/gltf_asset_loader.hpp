#pragma once

#include "assets/asset_types.hpp"

#include <filesystem>

namespace kage::assets {

class GltfAssetLoader final {
 public:
  [[nodiscard]] GltfDocument loadDocument(
      const std::filesystem::path& parPath) const;
  [[nodiscard]] StaticModel loadStaticModel(
      const std::filesystem::path& parPath) const;
};

}  // namespace kage::assets
