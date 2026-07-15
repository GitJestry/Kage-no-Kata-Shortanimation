#include "assets/project_asset_catalog.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-literal-operator"
#endif
#include <json.hpp>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <fstream>
#include <system_error>
#include <utility>

namespace {

using json = nlohmann::json;

[[nodiscard]] std::filesystem::path projectRootFromCatalogPath(
    const std::filesystem::path& parCatalogPath) {
  std::error_code error_code;
  const std::filesystem::path absolute_catalog_path =
      std::filesystem::weakly_canonical(parCatalogPath, error_code);
  const std::filesystem::path catalog_path =
      error_code ? std::filesystem::absolute(parCatalogPath)
                 : absolute_catalog_path;
  return catalog_path.parent_path().parent_path();
}

[[nodiscard]] std::filesystem::path readPath(const json& parJson,
                                             const char* parName,
                                             const std::filesystem::path& parRoot) {
  std::filesystem::path path(parJson.value(parName, std::string{}));
  if (!path.empty() && path.is_relative()) {
    path = parRoot / path;
  }
  return path;
}

}  // namespace

namespace kage::assets {

ProjectAssetCatalog loadProjectAssetCatalog(
    const std::filesystem::path& parCatalogPath) {
  ProjectAssetCatalog catalog;
  std::ifstream input(parCatalogPath);
  if (!input) {
    return catalog;
  }

  json document;
  input >> document;
  const std::filesystem::path project_root =
      projectRootFromCatalogPath(parCatalogPath);
  for (const json& asset_json : document.value("assets", json::array())) {
    ProjectAssetEntry asset;
    asset.id.value = asset_json.value("id", AssetId{}.value);
    asset.label = asset_json.value("label", "");
    asset.model_path = readPath(asset_json, "glb", project_root);
    if (asset.model_path.empty()) {
      asset.model_path = readPath(asset_json, "model", project_root);
    }
    asset.source_path = readPath(asset_json, "source", project_root);
    if (!asset.id.isValid() && !asset.model_path.empty()) {
      asset.id = makeStableAssetId("asset", asset.model_path);
    }
    for (const json& pack_json :
         asset_json.value("animation_packs", json::array())) {
      AssetRegistry::AnimationPackEntry pack;
      pack.label = pack_json.value("label", "");
      pack.path = readPath(pack_json, "path", project_root);
      if (!pack.label.empty() && !pack.path.empty()) {
        asset.animation_packs.push_back(std::move(pack));
      }
    }
    if (asset.id.isValid() && !asset.label.empty() &&
        !asset.model_path.empty()) {
      catalog.assets.push_back(std::move(asset));
    }
  }
  for (const json& environment_json :
       document.value("environments", json::array())) {
    EnvironmentAsset environment;
    environment.id.value =
        environment_json.value("id", AssetId{}.value);
    environment.label = environment_json.value("label", "");
    environment.path = readPath(environment_json, "path", project_root);
    environment.hdr = environment_json.value("hdr", false);
    if (!environment.id.isValid() && !environment.path.empty()) {
      environment.id = makeStableAssetId("environment", environment.path);
    }
    if (environment.id.isValid() && !environment.label.empty() &&
        !environment.path.empty()) {
      catalog.environments.push_back(std::move(environment));
    }
  }
  return catalog;
}

void saveProjectAssetCatalog(const std::filesystem::path& parCatalogPath,
                             const ProjectAssetCatalog& parCatalog) {
  std::filesystem::create_directories(parCatalogPath.parent_path());
  json document;
  document["version"] = 2;
  document["assets"] = json::array();
  for (const ProjectAssetEntry& asset : parCatalog.assets) {
    json asset_json = {
        {"id", asset.id.value},
        {"label", asset.label},
        {"glb", asset.model_path.generic_string()},
        {"source", asset.source_path.generic_string()},
    };
    asset_json["animation_packs"] = json::array();
    for (const AssetRegistry::AnimationPackEntry& pack :
         asset.animation_packs) {
      asset_json["animation_packs"].push_back({
          {"label", pack.label},
          {"path", pack.path.generic_string()},
      });
    }
    document["assets"].push_back(std::move(asset_json));
  }
  document["environments"] = json::array();
  for (const EnvironmentAsset& environment : parCatalog.environments) {
    document["environments"].push_back({
        {"id", environment.id.value},
        {"label", environment.label},
        {"path", environment.path.generic_string()},
        {"hdr", environment.hdr},
    });
  }

  std::ofstream output(parCatalogPath);
  output << document.dump(2);
}

}  // namespace kage::assets
