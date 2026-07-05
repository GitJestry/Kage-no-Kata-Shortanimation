#pragma once

#include "assets/asset_registry.hpp"

#include <filesystem>
#include <vector>

namespace kage::assets {

struct ProjectAssetEntry final {
  AssetId id;
  std::string label;
  std::filesystem::path model_path;
  std::filesystem::path source_path;
  std::vector<AssetRegistry::AnimationPackEntry> animation_packs;
};

struct ProjectAssetCatalog final {
  std::vector<ProjectAssetEntry> assets;
};

[[nodiscard]] ProjectAssetCatalog loadProjectAssetCatalog(
    const std::filesystem::path& parCatalogPath);
void saveProjectAssetCatalog(const std::filesystem::path& parCatalogPath,
                             const ProjectAssetCatalog& parCatalog);

}  // namespace kage::assets
