#include "assets/asset_registry.hpp"

#include <filesystem>
#include <iostream>

int main(int parArgumentCount, char** parArguments) {
  if (parArgumentCount != 2) {
    std::cerr << "usage: animation_import_compatibility_check <samurai.glb>\n";
    return 2;
  }

  kage::assets::AssetRegistry registry;
  const std::filesystem::path samurai_path(parArguments[1]);
  const std::size_t asset_index =
      registry.registerStaticAsset("Samurai", samurai_path);
  const kage::assets::AssetRegistry::AssetLibraryEntry* entry =
      registry.getAssetLibraryEntry(asset_index);
  if (entry == nullptr) {
    std::cerr << "registered Samurai asset was not found\n";
    return 1;
  }

  std::string error;
  if (!registry.addAnimationPack(entry->id, "Compatibility", samurai_path,
                                 error)) {
    std::cerr << "compatible animation import failed: " << error << '\n';
    return 1;
  }

  const kage::assets::ModelAsset* model = registry.getLoadedAsset(asset_index);
  if (model == nullptr || model->animation_clips.size() < 4) {
    std::cerr << "compatible animation pack did not append clips\n";
    return 1;
  }

  return 0;
}
