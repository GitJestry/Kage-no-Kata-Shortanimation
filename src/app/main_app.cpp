#include "app/main_app.hpp"

#include <glad/gl.h>
#include <imgui.h>

#include <exception>
#include <filesystem>
#include <utility>

namespace {

constexpr unsigned int WINDOW_WIDTH = 1280;
constexpr unsigned int WINDOW_HEIGHT = 720;
constexpr float CLEAR_COLOR_RED = 0.025f;
constexpr float CLEAR_COLOR_GREEN = 0.035f;
constexpr float CLEAR_COLOR_BLUE = 0.055f;
constexpr float CLEAR_COLOR_ALPHA = 1.0f;
constexpr float MILLISECONDS_PER_SECOND = 1000.0f;

void showPathStatus(const char* parLabel,
                    const std::filesystem::path& parPath) {
  const bool path_exists = std::filesystem::exists(parPath);
  const std::string path_string = parPath.string();
  ImGui::Text("%s: %s", parLabel, path_exists ? "found" : "missing");
  ImGui::TextWrapped("%s", path_string.c_str());
}

void showVector3(const char* parLabel, const glm::vec3& parValue) {
  ImGui::Text("%s: %.3f, %.3f, %.3f", parLabel, parValue.x, parValue.y,
              parValue.z);
}

}  // namespace

namespace kage::app {

MainApp::MainApp()
    : App(WINDOW_WIDTH, WINDOW_HEIGHT),
      m_runtime_paths(platform::RuntimePaths::fromExecutable()) {
  setTitle("Kage no Kata - The Final Cut");
  setVSync(true);
  loadStaticAsset("Sword", m_runtime_paths.getModelPath("sword.glb"));
  loadStaticAsset("Torii gate", m_runtime_paths.getModelPath("torii_gate.glb"));
}

void MainApp::render() {
  glClearColor(CLEAR_COLOR_RED, CLEAR_COLOR_GREEN, CLEAR_COLOR_BLUE,
               CLEAR_COLOR_ALPHA);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void MainApp::buildImGui() {
  ImGui::Begin("Runtime diagnostics");
  ImGui::Text("OpenGL 4.1 Core Profile");
  ImGui::Separator();
  ImGui::Text("Framebuffer: %.0f x %.0f", resolution.x, resolution.y);
  ImGui::Text("Frame time: %.3f ms", delta * MILLISECONDS_PER_SECOND);
  ImGui::Text("Frame: %u", frames);
  ImGui::Separator();
  ImGui::TextUnformatted("Runtime assets");
  showPathStatus("Sword GLB", m_runtime_paths.getModelPath("sword.glb"));
  showPathStatus("Torii gate GLB",
                 m_runtime_paths.getModelPath("torii_gate.glb"));
  ImGui::Separator();
  ImGui::TextUnformatted("GLB import diagnostics");
  for (const LoadedAsset& asset : m_assets) {
    if (ImGui::TreeNode(asset.label.c_str())) {
      if (asset.model.has_value()) {
        const assets::StaticModel& model = asset.model.value();
        const assets::GltfAssetStats& stats = model.stats;
        ImGui::Text("Scene: %s", model.scene_name.c_str());
        if (!model.import_warning.empty()) {
          ImGui::TextWrapped("Warning: %s", model.import_warning.c_str());
        }
        ImGui::Text("Meshes: %zu", stats.mesh_count);
        ImGui::Text("Primitives: %zu", stats.primitive_count);
        ImGui::Text("Vertices: %zu", stats.vertex_count);
        ImGui::Text("Indices: %zu", stats.index_count);
        ImGui::Text("Triangles: %zu", stats.triangle_count);
        ImGui::Text("Materials: %zu", stats.material_count);
        ImGui::Text("Textures: %zu", stats.texture_count);
        ImGui::Text("Images: %zu", stats.image_count);
        ImGui::Text("Skins: %zu", stats.skin_count);
        ImGui::Text("Animations: %zu", stats.animation_count);
        showVector3("Bounds min", model.bounds.min);
        showVector3("Bounds max", model.bounds.max);
        showVector3("Bounds size", model.bounds.getSize());
      } else {
        ImGui::TextWrapped("Import failed: %s", asset.error.c_str());
      }
      ImGui::TreePop();
    }
  }
  ImGui::TextUnformatted("Press Escape to close.");
  ImGui::End();
}

void MainApp::keyCallback(Key parKey, Action parAction, Modifier) {
  if (parKey == Key::ESC && parAction == Action::PRESS) {
    close();
  }
}

void MainApp::loadStaticAsset(std::string parLabel,
                              std::filesystem::path parPath) {
  assets::GltfAssetLoader loader;
  LoadedAsset loaded_asset;
  loaded_asset.label = std::move(parLabel);
  loaded_asset.path = std::move(parPath);

  try {
    loaded_asset.model = loader.loadStaticModel(loaded_asset.path);
  } catch (const std::exception& parException) {
    loaded_asset.error = parException.what();
  }

  m_assets.push_back(std::move(loaded_asset));
}

}  // namespace kage::app
