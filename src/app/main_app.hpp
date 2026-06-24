#pragma once

#include "assets/gltf_asset_loader.hpp"
#include "platform/runtime_paths.hpp"
#include "render/gpu_buffer.hpp"
#include "render/gpu_mesh.hpp"
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
    glm::vec3 material_color{1.0f};
    std::optional<assets::StaticModel> model;
    std::optional<render::GpuMesh> gpu_mesh;
    std::string error;
  };

  void loadStaticAsset(std::string parLabel, std::filesystem::path parPath,
                       const glm::vec3& parMaterialColor);
  void initializeTestTriangle();
  void initializeStaticMeshShader();
  void drawTestTriangle() const;
  void drawSelectedAsset() const;
  [[nodiscard]] glm::mat4 buildViewProjection(
      const assets::AssetBounds& parBounds) const;
  [[nodiscard]] const LoadedAsset* getSelectedAsset() const;

  platform::RuntimePaths m_runtime_paths;
  std::vector<LoadedAsset> m_assets;
  render::ShaderProgram m_test_triangle_shader;
  render::GpuBuffer m_test_triangle_vertex_buffer;
  render::VertexArray m_test_triangle_vertex_array;
  render::ShaderProgram m_static_mesh_shader;
  int m_selected_asset_index = 0;
};

}  // namespace kage::app
