#include "assets/asset_registry.hpp"

#include "assets/gltf_asset_loader.hpp"

#include <stdexcept>
#include <utility>

namespace kage::assets {

std::size_t AssetRegistry::registerStaticAsset(
    std::string parLabel, std::filesystem::path parPath) {
  AssetLibraryEntry entry;
  entry.id.value = m_asset_library.size();
  entry.label = std::move(parLabel);
  entry.path = std::move(parPath);
  entry.mesh_handle = entry.id.value;

  m_asset_library.push_back(std::move(entry));
  return m_asset_library.size() - 1;
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

ModelAsset& AssetRegistry::loadAsset(std::size_t parAssetIndex) {
  if (parAssetIndex >= m_asset_library.size()) {
    throw std::runtime_error("Asset library index is out of range");
  }

  AssetLibraryEntry& asset = m_asset_library[parAssetIndex];
  if (!asset.document.has_value()) {
    try {
      GltfAssetLoader loader;
      asset.document = loader.loadDocument(asset.path);
      asset.load_error.clear();
    } catch (const std::exception& error) {
      asset.load_error = error.what();
      throw;
    }
  }
  return *asset.document;
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

AssetRegistry::AssetLibraryEntry* AssetRegistry::getAssetLibraryEntry(
    std::size_t parAssetIndex) {
  if (parAssetIndex >= m_asset_library.size()) {
    return nullptr;
  }

  return &m_asset_library[parAssetIndex];
}

const ModelAsset* AssetRegistry::getLoadedAsset(
    std::size_t parAssetIndex) const {
  const AssetLibraryEntry* asset = getAssetLibraryEntry(parAssetIndex);
  return asset != nullptr && asset->document.has_value()
             ? &*asset->document
             : nullptr;
}

const StaticModel* AssetRegistry::getStaticMeshSource(
    StaticMeshHandle parHandle) const {
  for (const AssetLibraryEntry& entry : m_asset_library) {
    if (entry.mesh_handle == parHandle && entry.document.has_value()) {
      return &entry.document->static_model;
    }
  }
  return nullptr;
}

}  // namespace kage::assets
