#include "assets/gltf_asset_loader.hpp"
#include "assets/model_validation.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

int main(int parArgumentCount, char** parArguments) {
  if (parArgumentCount < 2) {
    std::cerr << "usage: asset_import_check <asset.glb> [--require-rig]\n";
    return 2;
  }

  const std::filesystem::path asset_path = parArguments[1];
  const bool require_rig =
      parArgumentCount >= 3 && std::string(parArguments[2]) == "--require-rig";

  try {
    const kage::assets::GltfAssetLoader loader;
    const kage::assets::ModelAsset asset = loader.loadDocument(asset_path);
    if (asset.static_model.primitives.empty()) {
      std::cerr << asset_path << ": no mesh primitives imported\n";
      return 1;
    }
    if (asset.static_model.stats.vertex_count == 0 ||
        asset.static_model.stats.index_count == 0) {
      std::cerr << asset_path << ": no renderable vertex/index data\n";
      return 1;
    }

    if (require_rig) {
      const kage::assets::RigValidationReport report =
          kage::assets::validateRiggedModelAsset(asset);
      if (!report.passed()) {
        std::cerr << asset_path << ": rig validation failed\n";
        for (const std::string& error : report.errors) {
          std::cerr << "- " << error << '\n';
        }
        return 1;
      }
    }

    std::cout << asset_path.filename().string() << ": primitives "
              << asset.stats.primitive_count << ", vertices "
              << asset.stats.vertex_count << ", skins "
              << asset.stats.skin_count << ", joints "
              << asset.stats.joint_count << ", animations "
              << asset.stats.animation_count << '\n';
  } catch (const std::exception& error) {
    std::cerr << asset_path << ": " << error.what() << '\n';
    return 1;
  }

  return 0;
}
