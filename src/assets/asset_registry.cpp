#include "assets/asset_registry.hpp"

#include "assets/gltf_asset_loader.hpp"

#include <stdexcept>
#include <utility>

namespace kage::assets {

std::size_t AssetRegistry::registerStaticAsset(
    std::string parLabel, std::filesystem::path parPath) {
  GltfAssetLoader loader;
  ModelAsset document = loader.loadDocument(parPath);
  return registerModelAsset(std::move(parLabel), std::move(parPath),
                            std::move(document));
}

std::size_t AssetRegistry::registerModelAsset(std::string parLabel,
                                              std::filesystem::path parPath,
                                              ModelAsset parDocument) {
  AssetLibraryEntry entry;
  entry.id.value = m_asset_library.size();
  entry.label = std::move(parLabel);
  entry.path = std::move(parPath);
  entry.document = std::move(parDocument);
  entry.mesh_handle = entry.id.value;

  m_asset_library.push_back(std::move(entry));
  return m_asset_library.size() - 1;
}

std::string AssetRegistry::reserveInstanceName(std::size_t parAssetIndex) {
  if (parAssetIndex >= m_asset_library.size()) {
    throw std::runtime_error("Asset library index is out of range");
  }

  AssetLibraryEntry& asset = m_asset_library[parAssetIndex];
  ++asset.instance_count;
  ++asset.next_instance_number;
  return asset.label + " " + std::to_string(asset.next_instance_number);
}

void AssetRegistry::releaseInstance(std::size_t parAssetIndex) {
  if (parAssetIndex >= m_asset_library.size()) {
    return;
  }

  AssetLibraryEntry& asset = m_asset_library[parAssetIndex];
  if (asset.instance_count > 0) {
    --asset.instance_count;
  }
}

void AssetRegistry::setInstanceState(std::size_t parAssetIndex,
                                     std::size_t parCount,
                                     std::size_t parNextInstanceNumber) {
  if (parAssetIndex >= m_asset_library.size()) {
    return;
  }

  AssetLibraryEntry& asset = m_asset_library[parAssetIndex];
  asset.instance_count = parCount;
  asset.next_instance_number = parNextInstanceNumber;
}

void AssetRegistry::resetInstanceCounts() {
  for (AssetLibraryEntry& asset : m_asset_library) {
    asset.instance_count = 0;
    asset.next_instance_number = 0;
  }
}

void AssetRegistry::rebuildInstanceCountsFromAssets(
    std::span<const std::size_t> parAssets) {
  resetInstanceCounts();
  for (std::size_t asset_index : parAssets) {
    if (asset_index >= m_asset_library.size()) {
      continue;
    }
    AssetLibraryEntry& asset = m_asset_library[asset_index];
    ++asset.instance_count;
    ++asset.next_instance_number;
  }
}

std::span<const AssetRegistry::AssetLibraryEntry>
AssetRegistry::getAssetLibrary() const {
  return m_asset_library;
}

const AssetRegistry::AssetLibraryEntry* AssetRegistry::getAssetLibraryEntry(
    std::size_t parAssetIndex) const {
  if (parAssetIndex >= m_asset_library.size()) {
    return nullptr;
  }

  return &m_asset_library[parAssetIndex];
}

const StaticModel* AssetRegistry::getStaticMeshSource(
    StaticMeshHandle parHandle) const {
  for (const AssetLibraryEntry& entry : m_asset_library) {
    if (entry.mesh_handle == parHandle) {
      return &entry.document.static_model;
    }
  }
  return nullptr;
}

}  // namespace kage::assets
