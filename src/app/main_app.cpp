#include "app/main_app.hpp"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <algorithm>
#include <cstddef>
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
constexpr float CAMERA_FIELD_OF_VIEW_DEGREES = 45.0f;
constexpr float CAMERA_NEAR_PLANE = 0.01f;
constexpr float CAMERA_FAR_PLANE_SCALE = 8.0f;
constexpr float CAMERA_DISTANCE_SCALE = 2.6f;
constexpr float CAMERA_HEIGHT_SCALE = 0.35f;
constexpr GLsizei DEBUG_TRIANGLE_VERTEX_COUNT = 3;
constexpr char TEST_TRIANGLE_VERTEX_SHADER[] = R"(#version 410 core
layout (location = 0) in vec2 inPosition;
layout (location = 1) in vec3 inColor;

out vec3 vertexColor;

void main() {
  vertexColor = inColor;
  gl_Position = vec4(inPosition, 0.0, 1.0);
}
)";
constexpr char TEST_TRIANGLE_FRAGMENT_SHADER[] = R"(#version 410 core
in vec3 vertexColor;

out vec4 fragColor;

void main() {
  fragColor = vec4(vertexColor, 1.0);
}
)";
constexpr char STATIC_MESH_VERTEX_SHADER[] = R"(#version 410 core
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;

uniform mat4 u_model;
uniform mat4 u_view_projection;

out vec3 worldNormal;

void main() {
  vec4 worldPosition = u_model * vec4(inPosition, 1.0);
  worldNormal = mat3(transpose(inverse(u_model))) * inNormal;
  gl_Position = u_view_projection * worldPosition;
}
)";
constexpr char STATIC_MESH_FRAGMENT_SHADER[] = R"(#version 410 core
in vec3 worldNormal;

uniform vec3 u_material_color;

out vec4 fragColor;

void main() {
  vec3 normal = length(worldNormal) > 0.001
      ? normalize(worldNormal)
      : vec3(0.0, 0.0, 1.0);
  vec3 lightDirection = normalize(vec3(-0.35, 0.85, 0.45));
  float light = max(dot(normal, lightDirection), 0.0);
  float shade = 0.28 + light * 0.72;
  fragColor = vec4(u_material_color * shade, 1.0);
}
)";

struct DebugVertex final {
  glm::vec2 position;
  glm::vec3 color;
};

constexpr GLsizei DEBUG_VERTEX_STRIDE =
    static_cast<GLsizei>(sizeof(DebugVertex));
constexpr DebugVertex TEST_TRIANGLE_VERTICES[] = {
    {{-0.55f, -0.35f}, {0.85f, 0.18f, 0.15f}},
    {{0.55f, -0.35f}, {0.15f, 0.62f, 0.95f}},
    {{0.0f, 0.52f}, {0.92f, 0.78f, 0.18f}},
};

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
  initializeTestTriangle();
  initializeStaticMeshShader();
  loadStaticAsset("Sword", m_runtime_paths.getModelPath("sword.glb"),
                  glm::vec3(0.78f, 0.76f, 0.70f));
  loadStaticAsset("Torii gate", m_runtime_paths.getModelPath("torii_gate.glb"),
                  glm::vec3(0.78f, 0.18f, 0.12f));
}

void MainApp::render() {
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glClearColor(CLEAR_COLOR_RED, CLEAR_COLOR_GREEN, CLEAR_COLOR_BLUE,
               CLEAR_COLOR_ALPHA);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  drawSelectedAsset();
}

void MainApp::buildImGui() {
  ImGui::Begin("Runtime diagnostics");
  ImGui::Text("OpenGL 4.1 Core Profile");
  ImGui::Separator();
  ImGui::Text("Framebuffer: %.0f x %.0f", resolution.x, resolution.y);
  ImGui::Text("Frame time: %.3f ms", delta * MILLISECONDS_PER_SECOND);
  ImGui::Text("Frame: %u", frames);
  ImGui::TextUnformatted("Project render layer: static GLB path active");
  ImGui::Separator();
  ImGui::TextUnformatted("Rendered asset");
  for (int asset_index = 0; asset_index < static_cast<int>(m_assets.size());
       ++asset_index) {
    const LoadedAsset& asset =
        m_assets[static_cast<std::size_t>(asset_index)];
    ImGui::RadioButton(asset.label.c_str(), &m_selected_asset_index,
                       asset_index);
  }
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
        ImGui::Text("GPU primitives: %zu",
                    asset.gpu_mesh.has_value()
                        ? asset.gpu_mesh->getPrimitiveCount()
                        : 0);
        ImGui::Text("GPU indices: %zu",
                    asset.gpu_mesh.has_value() ? asset.gpu_mesh->getIndexCount()
                                               : 0);
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
                              std::filesystem::path parPath,
                              const glm::vec3& parMaterialColor) {
  assets::GltfAssetLoader loader;
  LoadedAsset loaded_asset;
  loaded_asset.label = std::move(parLabel);
  loaded_asset.path = std::move(parPath);
  loaded_asset.material_color = parMaterialColor;

  try {
    loaded_asset.model = loader.loadStaticModel(loaded_asset.path);
    loaded_asset.gpu_mesh.emplace();
    loaded_asset.gpu_mesh->upload(loaded_asset.model.value());
  } catch (const std::exception& parException) {
    loaded_asset.error = parException.what();
  }

  m_assets.push_back(std::move(loaded_asset));
}

void MainApp::initializeTestTriangle() {
  m_test_triangle_shader.create(TEST_TRIANGLE_VERTEX_SHADER,
                                TEST_TRIANGLE_FRAGMENT_SHADER);
  m_test_triangle_vertex_array.create();
  m_test_triangle_vertex_buffer.create(GL_ARRAY_BUFFER);
  m_test_triangle_vertex_buffer.setData(sizeof(TEST_TRIANGLE_VERTICES),
                                        TEST_TRIANGLE_VERTICES,
                                        GL_STATIC_DRAW);

  m_test_triangle_vertex_array.bind();
  m_test_triangle_vertex_buffer.bind();
  m_test_triangle_vertex_array.setFloatAttribute(
      0, 2, GL_FLOAT, DEBUG_VERTEX_STRIDE, offsetof(DebugVertex, position));
  m_test_triangle_vertex_array.setFloatAttribute(
      1, 3, GL_FLOAT, DEBUG_VERTEX_STRIDE, offsetof(DebugVertex, color));
  render::VertexArray::unbind();
  render::GpuBuffer::unbind(GL_ARRAY_BUFFER);
}

void MainApp::initializeStaticMeshShader() {
  m_static_mesh_shader.create(STATIC_MESH_VERTEX_SHADER,
                              STATIC_MESH_FRAGMENT_SHADER);
}

void MainApp::drawTestTriangle() const {
  m_test_triangle_shader.use();
  m_test_triangle_vertex_array.bind();
  glDrawArrays(GL_TRIANGLES, 0, DEBUG_TRIANGLE_VERTEX_COUNT);
  render::VertexArray::unbind();
}

void MainApp::drawSelectedAsset() const {
  const LoadedAsset* selected_asset = getSelectedAsset();
  if (selected_asset == nullptr || !selected_asset->model.has_value() ||
      !selected_asset->gpu_mesh.has_value() ||
      !selected_asset->gpu_mesh->isValid()) {
    drawTestTriangle();
    return;
  }

  const glm::mat4 view_projection =
      buildViewProjection(selected_asset->model->bounds);
  selected_asset->gpu_mesh->draw(m_static_mesh_shader, view_projection,
                                 selected_asset->material_color);
}

glm::mat4 MainApp::buildViewProjection(
    const assets::AssetBounds& parBounds) const {
  const glm::vec3 bounds_size = parBounds.getSize();
  const float max_extent =
      std::max({bounds_size.x, bounds_size.y, bounds_size.z, 1.0f});
  const glm::vec3 center =
      parBounds.is_valid ? (parBounds.min + parBounds.max) * 0.5f
                         : glm::vec3(0.0f);
  const float camera_distance = max_extent * CAMERA_DISTANCE_SCALE;
  const glm::vec3 eye =
      center + glm::vec3(0.0f, max_extent * CAMERA_HEIGHT_SCALE,
                         camera_distance);
  const glm::mat4 view = glm::lookAt(eye, center, glm::vec3(0.0f, 1.0f, 0.0f));
  const float aspect_ratio =
      resolution.y > 0.0f ? resolution.x / resolution.y : 1.0f;
  const glm::mat4 projection = glm::perspective(
      glm::radians(CAMERA_FIELD_OF_VIEW_DEGREES), aspect_ratio,
      CAMERA_NEAR_PLANE, max_extent * CAMERA_FAR_PLANE_SCALE);

  return projection * view;
}

const MainApp::LoadedAsset* MainApp::getSelectedAsset() const {
  if (m_selected_asset_index < 0 ||
      static_cast<std::size_t>(m_selected_asset_index) >= m_assets.size()) {
    return nullptr;
  }

  return &m_assets[static_cast<std::size_t>(m_selected_asset_index)];
}

}  // namespace kage::app
