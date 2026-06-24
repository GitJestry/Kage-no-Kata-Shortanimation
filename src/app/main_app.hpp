#pragma once

#include "assets/gltf_asset_loader.hpp"
#include "platform/runtime_paths.hpp"
#include "render/gpu_buffer.hpp"
#include "render/shader_program.hpp"
#include "render/vertex_array.hpp"

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
  void initializeTestTriangle();
  void drawTestTriangle() const;

  platform::RuntimePaths m_runtime_paths;
  std::vector<LoadedAsset> m_assets;
  render::ShaderProgram m_test_triangle_shader;
  render::GpuBuffer m_test_triangle_vertex_buffer;
  render::VertexArray m_test_triangle_vertex_array;
};

}  // namespace kage::app
