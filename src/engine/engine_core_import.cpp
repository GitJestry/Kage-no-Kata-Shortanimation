#include "engine/engine_core.hpp"

#include "assets/asset_path.hpp"
#include "assets/gltf_asset_loader.hpp"
#include "assets/project_asset_catalog.hpp"

#include <exception>
#include <filesystem>
#include <string>
#include <utility>

namespace {

constexpr const char* PROJECT_ASSET_CATALOG_PATH =
    "projects/kage_no_kata_assets.kage.json";
constexpr const char* MODEL_DIRECTORY = "assets/models";
constexpr const char* ANIMATION_DIRECTORY = "assets/animations";

[[nodiscard]] std::filesystem::path catalogPath() {
  return std::filesystem::current_path() / PROJECT_ASSET_CATALOG_PATH;
}

[[nodiscard]] kage::assets::ProjectAssetCatalog buildCatalog(
    const kage::assets::AssetRegistry& parRegistry) {
  kage::assets::ProjectAssetCatalog catalog;
  const std::filesystem::path project_root = std::filesystem::current_path();
  const auto project_relative = [&](const std::filesystem::path& parPath) {
    std::error_code error_code;
    const std::filesystem::path relative =
        std::filesystem::relative(parPath, project_root, error_code);
    return error_code ? parPath : relative;
  };
  for (const kage::assets::AssetRegistry::AssetLibraryEntry& asset :
       parRegistry.getAssetLibrary()) {
    kage::assets::ProjectAssetEntry entry;
    entry.id = asset.id;
    entry.label = asset.label;
    entry.model_path = project_relative(asset.path);
    entry.source_path = project_relative(asset.source_path);
    for (const kage::assets::AssetRegistry::AnimationPackEntry& pack :
         asset.animation_packs) {
      entry.animation_packs.push_back(
          {pack.label, project_relative(pack.path)});
    }
    catalog.assets.push_back(std::move(entry));
  }
  return catalog;
}

}  // namespace

namespace kage::engine {

std::size_t EngineCore::registerStaticAsset(
    assets::AssetId parAssetId, std::string parLabel,
    std::filesystem::path parPath, std::filesystem::path parSourcePath) {
  return m_asset_registry.registerStaticAsset(
      parAssetId, std::move(parLabel), std::move(parPath),
      std::move(parSourcePath));
}

void EngineCore::loadProjectAssetCatalog(
    const std::filesystem::path& parCatalogPath) {
  const assets::ProjectAssetCatalog catalog =
      assets::loadProjectAssetCatalog(parCatalogPath);
  for (const assets::ProjectAssetEntry& entry : catalog.assets) {
    const std::size_t index =
        registerStaticAsset(entry.id, entry.label, entry.model_path,
                            entry.source_path);
    assets::AssetRegistry::AssetLibraryEntry* asset =
        m_asset_registry.getAssetLibraryEntry(index);
    if (asset != nullptr) {
      asset->animation_packs = entry.animation_packs;
    }
  }
}

std::optional<std::size_t> EngineCore::importModelAsset(
    const std::filesystem::path& parSourcePath, std::string parLabel,
    std::string& parError) {
  parError.clear();
  if (!std::filesystem::exists(parSourcePath)) {
    parError = "file does not exist";
    return std::nullopt;
  }
  if (!assets::hasGltfExtension(parSourcePath)) {
    parError = "only .glb and .gltf model files can be imported";
    return std::nullopt;
  }
  if (parLabel.empty()) {
    parLabel = assets::defaultAssetLabelFromPath(parSourcePath);
  }

  assets::ModelAsset document;
  try {
    const assets::GltfAssetLoader loader;
    document = loader.loadDocument(parSourcePath);
  } catch (const std::exception& error) {
    parError = error.what();
    return std::nullopt;
  }
  if (document.static_model.primitives.empty()) {
    parError = "imported model has no renderable primitives";
    return std::nullopt;
  }

  const std::filesystem::path destination = assets::copyIntoProjectAssets(
      parSourcePath, std::filesystem::current_path() / MODEL_DIRECTORY,
      parError);
  if (destination.empty()) {
    return std::nullopt;
  }

  const std::size_t asset_index =
      registerModelAsset(std::move(parLabel), destination, std::move(document));
  assets::AssetRegistry::AssetLibraryEntry* asset =
      m_asset_registry.getAssetLibraryEntry(asset_index);
  if (asset != nullptr) {
    asset->source_path = parSourcePath;
  }
  assets::saveProjectAssetCatalog(catalogPath(), buildCatalog(m_asset_registry));
  markProjectDirty();
  return asset_index;
}

bool EngineCore::importAnimationForEntity(
    scene::EntityId parEntity, const std::filesystem::path& parSourcePath,
    std::string parLabel, std::string& parError) {
  parError.clear();
  const scene::EntityRecord* entity = getActiveScene().world.findEntity(parEntity);
  if (entity == nullptr || !entity->static_mesh.has_value()) {
    parError = "select a rigged mesh entity first";
    return false;
  }
  if (!std::filesystem::exists(parSourcePath)) {
    parError = "file does not exist";
    return false;
  }
  if (!assets::hasGltfExtension(parSourcePath)) {
    parError = "only .glb and .gltf animation files can be imported";
    return false;
  }
  if (parLabel.empty()) {
    parLabel = assets::defaultAssetLabelFromPath(parSourcePath);
  }

  const assets::AssetRegistry::AssetLibraryEntry* base_asset =
      m_asset_registry.getAssetLibraryEntry(
          entity->static_mesh->asset_library_index);
  if (base_asset == nullptr) {
    parError = "selected entity has no asset catalog entry";
    return false;
  }

  const std::filesystem::path destination = assets::copyIntoProjectAssets(
      parSourcePath,
      std::filesystem::current_path() / ANIMATION_DIRECTORY, parError);
  if (destination.empty()) {
    return false;
  }

  if (!m_asset_registry.addAnimationPack(base_asset->id, parLabel, destination,
                                         parError)) {
    return false;
  }
  assets::saveProjectAssetCatalog(catalogPath(), buildCatalog(m_asset_registry));
  markProjectDirty();
  return true;
}

}  // namespace kage::engine
