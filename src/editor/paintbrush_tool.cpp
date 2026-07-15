#include "editor/paintbrush_tool.hpp"

#include "assets/asset_registry.hpp"

#include <imgui.h>

#include <algorithm>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <random>
#include <string>

namespace kage::editor {
namespace {

[[nodiscard]] std::string getAssetCapabilityLabel(
    const kage::assets::AssetRegistry::AssetLibraryEntry& parAsset) {
  if (!parAsset.document.has_value()) {
    return "model";
  }
  const kage::assets::ModelAsset& document = *parAsset.document;
  std::string label = document.static_model.primitives.empty() ? "data" : "mesh";
  if (!document.skins.empty()) {
    label += " + rig";
  }
  if (!document.animation_clips.empty()) {
    label += " + clips";
  }
  return label;
}

}  // namespace

void PaintbrushTool::drawUi(engine::EngineCore& parEngine) {
  ImGui::Checkbox("Enable paintbrush", &m_enabled);

  if (!m_enabled) {
    ImGui::TextDisabled("Enable paintbrush to configure mesh placement");
    return;
  }

  const auto library = parEngine.getAssetRegistry().getAssetLibrary();
  if (m_selected_assets.size() != library.size()) {
    m_selected_assets.resize(library.size(), false);
  }

  ImGui::TextUnformatted("Mesh set");
  if (library.empty()) {
    ImGui::TextDisabled("No models available");
    return;
  }

  ImGui::TextDisabled("Choose meshes to paint with");
  for (std::size_t asset_index = 0; asset_index < library.size(); ++asset_index) {
    const auto& asset = library[asset_index];
    const std::string label = asset.label + "  (" +
                              getAssetCapabilityLabel(asset) + ")";
    bool selected = m_selected_assets[asset_index];
    ImGui::PushID(static_cast<int>(asset_index));
    if (ImGui::Checkbox(label.c_str(), &selected)) {
      m_selected_assets[asset_index] = selected;
    }
    ImGui::PopID();
  }

  ImGui::Separator();
  ImGui::SliderInt("Brush size", &m_brush_size, 1, 64);
  ImGui::SliderInt("Paint density", &m_paint_density, 1, 64);
  ImGui::Checkbox("Randomize scale", &m_randomize_scale);
  ImGui::Checkbox("Randomize rotation", &m_randomize_rotation);

  const std::size_t selected_count = static_cast<std::size_t>(std::count(
      m_selected_assets.begin(), m_selected_assets.end(), true));
  if (selected_count == 0) {
    ImGui::TextDisabled("Select at least one mesh to enable painting");
  } else {
    ImGui::TextDisabled("%zu mesh(es) selected", selected_count);
  }
}

void PaintbrushTool::paintAssets(engine::EngineCore& parEngine,
                                  const glm::vec3& parCenter,
                                  const std::vector<std::size_t>& parAssetIndices) const {
  if (parAssetIndices.empty()) {
    return;
  }

  std::mt19937 rng(std::random_device{}());
  std::uniform_real_distribution<float> offset_distribution(
      -static_cast<float>(m_brush_size), static_cast<float>(m_brush_size));
  std::uniform_real_distribution<float> rotation_distribution(
      0.0f, glm::two_pi<float>());
  std::uniform_real_distribution<float> scale_distribution(0.75f, 1.25f);
  std::uniform_int_distribution<std::size_t> asset_distribution(
      0, parAssetIndices.size() - 1);

  const int spawn_count = std::max(1, m_paint_density);
  for (int spawn_index = 0; spawn_index < spawn_count; ++spawn_index) {
    glm::vec3 offset(0.0f);
    for (int attempt = 0; attempt < 8; ++attempt) {
      offset = glm::vec3(offset_distribution(rng), 0.0f,
                         offset_distribution(rng));
      if (glm::length(glm::vec2(offset.x, offset.z)) <=
          static_cast<float>(m_brush_size)) {
        break;
      }
    }

    const glm::vec3 position = parCenter + offset;
    const float yaw_radians = rotation_distribution(rng);
    const glm::quat rotation = m_randomize_rotation
                                   ? glm::angleAxis(yaw_radians,
                                                    glm::vec3(0.0f, 1.0f, 0.0f))
                                   : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    const glm::vec3 scale = m_randomize_scale
                                ? glm::vec3(scale_distribution(rng),
                                            scale_distribution(rng),
                                            scale_distribution(rng))
                                : glm::vec3(1.0f);

    const std::size_t asset_index = parAssetIndices[asset_distribution(rng)];
    const scene::EntityId entity = parEngine.instantiateAssetAt(asset_index, position);
    parEngine.setEntityTransform(
        entity, kage::math::Transform{position, rotation, scale});
  }
}

bool PaintbrushTool::isEnabled() const {
  return m_enabled;
}

int PaintbrushTool::getBrushSize() const {
  return m_brush_size;
}

int PaintbrushTool::getPaintDensity() const {
  return m_paint_density;
}

std::vector<std::size_t> PaintbrushTool::getSelectedAssetIndices() const {
  std::vector<std::size_t> selected_assets;
  selected_assets.reserve(m_selected_assets.size());
  for (std::size_t asset_index = 0; asset_index < m_selected_assets.size();
       ++asset_index) {
    if (m_selected_assets[asset_index]) {
      selected_assets.push_back(asset_index);
    }
  }
  return selected_assets;
}

}  // namespace kage::editor
