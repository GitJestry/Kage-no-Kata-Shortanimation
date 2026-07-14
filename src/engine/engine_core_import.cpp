#include "engine/engine_core.hpp"

#include "assets/asset_path.hpp"
#include "assets/gltf_asset_loader.hpp"
#include "assets/project_asset_catalog.hpp"

#include <exception>
#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>

namespace {

[[nodiscard]] kage::assets::ProjectAssetCatalog buildCatalog(
    const kage::assets::AssetRegistry& parRegistry,
    std::span<const kage::assets::EnvironmentAsset> parEnvironments,
    const std::filesystem::path& parRegistryProjectRoot) {
  kage::assets::ProjectAssetCatalog catalog;
  const std::filesystem::path project_root = parRegistryProjectRoot;
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
  for (const kage::assets::EnvironmentAsset& environment : parEnvironments) {
    kage::assets::EnvironmentAsset entry = environment;
    entry.path = project_relative(entry.path);
    catalog.environments.push_back(std::move(entry));
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
  m_environment_assets = catalog.environments;
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
      parSourcePath, m_runtime_paths.getModelDirectory(),
      m_runtime_paths.getAssetDirectory(), parError);
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
  assets::saveProjectAssetCatalog(m_runtime_paths.getProjectAssetCatalogPath(),
                                  buildCatalog(m_asset_registry,
                                               m_environment_assets,
                                               m_runtime_paths.getProjectRoot()));
  markProjectDirty();
  return asset_index;
}

std::optional<assets::AssetId> EngineCore::importPanorama(
    const std::filesystem::path& parSourcePath, std::string parLabel,
    std::string& parError) {
  parError.clear();
  if (!std::filesystem::exists(parSourcePath) ||
      !assets::hasPanoramaExtension(parSourcePath)) {
    parError = "choose an .hdr, .png, .jpg, or .jpeg panorama";
    return std::nullopt;
  }
  if (parLabel.empty()) {
    parLabel = assets::defaultAssetLabelFromPath(parSourcePath);
  }
  const std::filesystem::path destination = assets::copyIntoProjectAssets(
      parSourcePath, m_runtime_paths.getTexturePath("environments"),
      m_runtime_paths.getAssetDirectory(), parError);
  if (destination.empty()) {
    return std::nullopt;
  }
  const assets::AssetId id =
      assets::makeStableAssetId("environment", destination);
  const auto existing = std::find_if(
      m_environment_assets.begin(), m_environment_assets.end(),
      [id](const assets::EnvironmentAsset& item) { return item.id == id; });
  if (existing == m_environment_assets.end()) {
    m_environment_assets.push_back(
        {id, std::move(parLabel), destination,
         assets::hasHdrExtension(destination)});
  }
  assets::saveProjectAssetCatalog(
      m_runtime_paths.getProjectAssetCatalogPath(),
      buildCatalog(m_asset_registry, m_environment_assets,
                   m_runtime_paths.getProjectRoot()));
  render::EnvironmentSettings settings = m_render_settings.scene.environment;
  settings.asset_id = id;
  settings.visible = true;
  setEnvironmentSettings(settings);
  return id;
}

}  // namespace kage::engine
