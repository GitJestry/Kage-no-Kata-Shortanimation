#pragma once

#include "assets/asset_types.hpp"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kage::assets {

enum class AssetLoadState {
  MetadataReady,
  Queued,
  CpuLoading,
  GpuUploading,
  Ready,
  Error
};

class AssetRegistry final {
 public:
  using StaticMeshHandle = std::size_t;

  struct AnimationPackEntry final {
    std::string label;
    std::filesystem::path path;
  };

  struct AssetLibraryEntry final {
    AssetId id;
    std::string label;
    std::filesystem::path path;
    std::filesystem::path source_path;
    AssetLoadState load_state = AssetLoadState::MetadataReady;
    std::optional<ModelAsset> document;
    std::string load_error;
    std::chrono::steady_clock::time_point load_started_at{};
    float last_cpu_import_ms = 0.0f;
    StaticMeshHandle mesh_handle = 0;
    std::size_t instance_count = 0;
    std::size_t next_instance_number = 0;
    std::vector<AnimationPackEntry> animation_packs;
  };

  std::size_t registerStaticAsset(std::string parLabel,
                                  std::filesystem::path parPath);
  std::size_t registerStaticAsset(std::string parLabel,
                                  std::filesystem::path parPath,
                                  std::filesystem::path parSourcePath);
  std::size_t registerStaticAsset(AssetId parAssetId, std::string parLabel,
                                  std::filesystem::path parPath,
                                  std::filesystem::path parSourcePath);
  std::size_t registerModelAsset(std::string parLabel,
                                 std::filesystem::path parPath,
                                 ModelAsset parDocument);
  bool addAnimationPack(AssetId parAssetId, std::string parLabel,
                        std::filesystem::path parPath,
                        std::string& parError);
  ModelAsset& loadAsset(std::size_t parAssetIndex);
  void requestLoad(std::size_t parAssetIndex);
  void beginCpuLoad(std::size_t parAssetIndex);
  bool completeCpuLoad(std::size_t parAssetIndex,
                       std::optional<ModelAsset> parDocument,
                       std::string parError, float parCpuMs);
  void completeGpuUpload(std::size_t parAssetIndex);
  void failLoad(std::size_t parAssetIndex, std::string parError);
  std::string reserveInstanceName(std::size_t parAssetIndex);
  void releaseInstance(std::size_t parAssetIndex);
  void setInstanceState(std::size_t parAssetIndex, std::size_t parCount,
                        std::size_t parNextInstanceNumber);

  [[nodiscard]] std::span<const AssetLibraryEntry> getAssetLibrary() const;
  [[nodiscard]] const AssetLibraryEntry* getAssetLibraryEntry(
      std::size_t parAssetIndex) const;
  [[nodiscard]] AssetLibraryEntry* getAssetLibraryEntry(
      std::size_t parAssetIndex);
  [[nodiscard]] const AssetLibraryEntry* getAssetLibraryEntryById(
      AssetId parAssetId) const;
  [[nodiscard]] AssetLibraryEntry* getAssetLibraryEntryById(
      AssetId parAssetId);
  [[nodiscard]] std::optional<std::size_t> getAssetIndexById(
      AssetId parAssetId) const;
  [[nodiscard]] const ModelAsset* getLoadedAsset(
      std::size_t parAssetIndex) const;
  void releaseStaticGeometryPayload(std::size_t parAssetIndex);
  [[nodiscard]] const StaticModel* getStaticMeshSource(
      StaticMeshHandle parHandle) const;

 private:
  void applyAnimationPacks(AssetLibraryEntry& parAsset);
  std::vector<AssetLibraryEntry> m_asset_library;
};

[[nodiscard]] AssetId makeStableAssetId(std::string_view parNamespace,
                                        const std::filesystem::path& parPath);

}  // namespace kage::assets
