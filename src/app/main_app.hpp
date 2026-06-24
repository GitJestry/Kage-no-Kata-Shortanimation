#pragma once

#include "assets/gltf_asset_loader.hpp"
#include "platform/runtime_paths.hpp"

#include <framework/app.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace kage::app {

class MainApp final : public App {
 public:
  MainApp();

 protected:
  void render() override;
  void buildImGui() override;
  void keyCallback(Key parKey, Action parAction,
                   Modifier parModifier) override;

 private:
  struct LoadedAsset final {
    std::string label;
    std::filesystem::path path;
    std::optional<assets::StaticModel> model;
    std::string error;
  };

  void loadStaticAsset(std::string parLabel, std::filesystem::path parPath);

  platform::RuntimePaths m_runtime_paths;
  std::vector<LoadedAsset> m_assets;
};

}  // namespace kage::app
