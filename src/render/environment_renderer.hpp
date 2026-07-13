#pragma once

#include "assets/asset_types.hpp"
#include "camera/camera.hpp"
#include "render/shader_program.hpp"
#include "render/texture_2d.hpp"
#include "render/environment_image.hpp"

#include <filesystem>
#include <future>
#include <optional>
#include <vector>

namespace kage::render {

enum class EnvironmentLoadState { None, Loading, Ready, Error };

struct EnvironmentSettings final {
  assets::AssetId asset_id;
  float intensity = 1.0f;
  float yaw_degrees = 0.0f;
  bool visible = true;
};

class EnvironmentRenderer final {
 public:
  EnvironmentRenderer();
  ~EnvironmentRenderer();

  EnvironmentRenderer(const EnvironmentRenderer&) = delete;
  EnvironmentRenderer& operator=(const EnvironmentRenderer&) = delete;

  void request(assets::AssetId parAsset,
               const std::filesystem::path& parPath);
  [[nodiscard]] bool draw(const camera::Camera& parCamera,
                          const glm::vec2& parViewportSize,
                          const EnvironmentSettings& parSettings);
  [[nodiscard]] EnvironmentLoadState getState() const;
  [[nodiscard]] const std::string& getError() const;

 private:
  void pollUpload();

  ShaderProgram m_shader;
  Texture2D m_panorama;
  assets::AssetId m_requested_asset;
  assets::AssetId m_loaded_asset;
  std::future<DecodedEnvironmentImage> m_decode;
  std::uint64_t m_generation = 0;
  EnvironmentLoadState m_state = EnvironmentLoadState::None;
  std::string m_error;
  GLuint m_vertex_array = 0;
};

}  // namespace kage::render
