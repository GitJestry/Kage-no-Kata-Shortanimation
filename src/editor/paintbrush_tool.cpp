#include "editor/paintbrush_tool.hpp"

#include "assets/asset_registry.hpp"

#include <imgui.h>

#include <algorithm>
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

  ImGui::TextUnformatted("Scatter asset set");
  if (library.empty()) {
    ImGui::TextDisabled("No models available for procedural scattering");
    return;
  }

  ImGui::TextDisabled("Choose meshes to scatter as procedural foliage");
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
  ImGui::SliderInt("Brush size", &m_settings.brush_size, 1, 64);
  ImGui::SliderInt("Paint density", &m_settings.paint_density, 1, 64);
  ImGui::Checkbox("Randomize scale", &m_settings.randomize_scale);
  ImGui::Checkbox("Randomize rotation", &m_settings.randomize_rotation);

  const std::size_t selected_count = static_cast<std::size_t>(std::count(
      m_selected_assets.begin(), m_selected_assets.end(), true));
  if (selected_count == 0) {
    ImGui::TextDisabled("Select at least one mesh to enable painting");
  } else {
    ImGui::TextDisabled("%zu mesh(es) selected", selected_count);
  }
}


bool PaintbrushTool::isEnabled() const {
  return m_enabled;
}

const PaintbrushSettings& PaintbrushTool::getSettings() const {
  return m_settings;
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
