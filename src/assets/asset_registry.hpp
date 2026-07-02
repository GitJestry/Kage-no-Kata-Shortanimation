#pragma once

#include "assets/asset_types.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace kage::assets {

class AssetRegistry final {
 public:
  using StaticMeshHandle = std::size_t;

  struct AssetLibraryEntry final {
    AssetId id;
    std::string label;
    std::filesystem::path path;
    std::optional<ModelAsset> document;
    std::string load_error;
    StaticMeshHandle mesh_handle = 0;
    std::size_t instance_count = 0;
    std::size_t next_instance_number = 0;

    [[nodiscard]] bool isLoaded() const;
  };

  std::size_t registerStaticAsset(std::string parLabel,
                                  std::filesystem::path parPath);
  std::size_t registerModelAsset(std::string parLabel,
                                 std::filesystem::path parPath,
                                 ModelAsset parDocument);
  ModelAsset& loadAsset(std::size_t parAssetIndex);
  std::string reserveInstanceName(std::size_t parAssetIndex);
  void releaseInstance(std::size_t parAssetIndex);
  void setInstanceState(std::size_t parAssetIndex, std::size_t parCount,
                        std::size_t parNextInstanceNumber);
  void resetInstanceCounts();
  void rebuildInstanceCountsFromAssets(std::span<const std::size_t> parAssets);

  [[nodiscard]] std::span<const AssetLibraryEntry> getAssetLibrary() const;
  [[nodiscard]] const AssetLibraryEntry* getAssetLibraryEntry(
      std::size_t parAssetIndex) const;
  [[nodiscard]] AssetLibraryEntry* getAssetLibraryEntry(
      std::size_t parAssetIndex);
  [[nodiscard]] const ModelAsset* getLoadedAsset(
      std::size_t parAssetIndex) const;
  [[nodiscard]] const StaticModel* getStaticMeshSource(
      StaticMeshHandle parHandle) const;

 private:
  std::vector<AssetLibraryEntry> m_asset_library;
};

inline bool AssetRegistry::AssetLibraryEntry::isLoaded() const {
  return document.has_value();
}

}  // namespace kage::assets
