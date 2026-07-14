#include "editor/editor_ui.hpp"

#include "editor/lighting_panel.hpp"
#include "editor/timeline_panel.hpp"
#include "editor/ui_layout.hpp"

#include <imgui.h>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr float LEFT_PANEL_WIDTH = 335.0f;
constexpr float INSPECTOR_WIDTH = 410.0f;
constexpr float INSPECTOR_HEIGHT = 440.0f;
constexpr float STATUS_HEIGHT = kage::editor::UI_STATUS_HEIGHT;

using kage::editor::clampPanelPosition;
using kage::editor::clampPanelSize;
using kage::editor::getUiWorkArea;
using kage::editor::UiWorkArea;

void copyToBuffer(std::string_view parText, std::span<char> parBuffer) {
  if (parBuffer.empty()) {
    return;
  }

  const std::size_t copy_size =
      std::min(parText.size(), parBuffer.size() - 1);
  std::memcpy(parBuffer.data(), parText.data(), copy_size);
  parBuffer[copy_size] = '\0';
}

void requestEntityDeletionConfirmation(kage::editor::ConfirmationDialog& parDialog,
                         kage::engine::EngineCore& parEngine,
                         kage::scene::EntityId parEntity,
                         std::string parEntityName) {
  kage::editor::ConfirmationDialog::Request request;
  request.title = "Delete Entity";
  request.message = "Delete \"" + parEntityName + "\" (id " +
                    std::to_string(parEntity.value) +
                    ")? This cannot be undone.";
  request.confirm_text = "Delete";
  request.cancel_text = "Cancel";
  request.destructive = true;
  request.on_confirm = [&parEngine, parEntity]() {
    parEngine.deleteEntity(parEntity);
  };
  parDialog.request(std::move(request));
}

void drawVector3(const char* parLabel, const glm::vec3& parValue) {
  ImGui::Text("%s: %.3f, %.3f, %.3f", parLabel, parValue.x, parValue.y,
              parValue.z);
}

bool drawTransformControls(kage::engine::EngineCore& parEngine,
                           const kage::scene::EntityRecord& parEntity,
                           kage::math::Transform& parTransform) {
  bool changed = false;

  ImGui::TextDisabled("Position  X        Y        Z");
  ImGui::SetNextItemWidth(-1.0f);
  changed |= ImGui::DragFloat3("##PositionXYZ", &parTransform.translation.x,
                               0.05f);

  glm::vec3 rotation_degrees =
      glm::degrees(glm::eulerAngles(parTransform.rotation));
  ImGui::TextDisabled("Rotation  X        Y        Z");
  ImGui::SetNextItemWidth(-1.0f);
  if (ImGui::DragFloat3("##RotationXYZ", &rotation_degrees.x, 0.5f)) {
    parTransform.rotation =
        glm::normalize(glm::quat(glm::radians(rotation_degrees)));
    changed = true;
  }

  ImGui::TextDisabled("Scale     X        Y        Z");
  ImGui::SetNextItemWidth(-1.0f);
  changed |= ImGui::DragFloat3("##ScaleXYZ", &parTransform.scale.x, 0.02f,
                               0.001f, 100.0f);

  if (ImGui::Button("Ground Position")) {
    const kage::math::Bounds3 bounds =
        parEngine.getEntityWorldBounds(parEntity.id);
    if (bounds.is_valid && parEntity.static_mesh.has_value()) {
      parTransform.translation.y -= bounds.min.y;
    } else {
      parTransform.translation.y = 0.0f;
    }
    changed = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset Rotation")) {
    parTransform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    changed = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset Scale")) {
    parTransform.scale = glm::vec3(1.0f);
    changed = true;
  }
  return changed;
}

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

namespace kage::editor {

bool EditorUi::isPaintbrushEnabled() const {
  return m_paintbrush_enabled;
}

int EditorUi::getPaintbrushBrushSize() const {
  return m_paintbrush_brush_size;
}

int EditorUi::getPaintbrushPaintDensity() const {
  return m_paintbrush_paint_density;
}

std::vector<std::size_t> EditorUi::getPaintbrushSelectedAssetIndices() const {
  std::vector<std::size_t> selected_assets;
  selected_assets.reserve(m_paintbrush_selected_assets.size());
  for (std::size_t asset_index = 0;
       asset_index < m_paintbrush_selected_assets.size(); ++asset_index) {
    if (m_paintbrush_selected_assets[asset_index]) {
      selected_assets.push_back(asset_index);
    }
  }
  return selected_assets;
}

void EditorUi::draw(engine::EngineCore& parEngine,
                    EditorSession& parSession,
                    PlacementController& parPlacementController,
                    SelectionController& parSelectionController,
                    GizmoController& parGizmoController,
                    ConfirmationDialog& parConfirmationDialog,
                    const glm::vec2& parViewportSize, float parDeltaSeconds,
                    unsigned int parFrameCount) {
  applyStyle();
  beginPanelTracking();
  drawTopBar(parEngine, parSession, parViewportSize);
  const bool world_edit = parSession.workspace == Workspace::WorldEdit;
  if (world_edit && m_panel_visible) {
    drawLeftPanel(parEngine, parPlacementController,
                  parSelectionController, parGizmoController,
                  parConfirmationDialog, parViewportSize);
  } else if (world_edit) {
    drawHiddenPanelButton();
  }

  if (m_diagnostics_visible) {
    drawRuntimeDiagnostics(parEngine, parViewportSize, parDeltaSeconds,
                           parFrameCount);
  }
  if (world_edit && m_inspector_visible) {
    drawInspector(parEngine, parGizmoController, parConfirmationDialog,
                  parViewportSize);
  } else if (world_edit) {
    drawHiddenInspectorButton();
  }
  if (parSession.workspace == Workspace::Movie) {
    if (std::optional<UiPanelRect> timeline_rect =
            drawTimelinePanel(parEngine, parSession, parViewportSize,
                              m_animation_import_browser,
                              m_animation_import_label_buffer,
                              m_animation_import_error)) {
      m_panel_rects.push_back(*timeline_rect);
    }
  }

  drawStatusStrip(parEngine, parPlacementController, parGizmoController,
                  parViewportSize);
  drawImportDialogs(parEngine, parPlacementController);
}

void EditorUi::drawTopBar(engine::EngineCore& parEngine,
                          EditorSession& parSession,
                          const glm::vec2& parViewportSize) {
  static_cast<void>(parViewportSize);
  const UiWorkArea area = getUiWorkArea();
  constexpr float WORKSPACE_WIDTH = 206.0f;
  ImGui::SetNextWindowPos(
      ImVec2(area.position.x + (area.size.x - WORKSPACE_WIDTH) * 0.5f,
             area.position.y + 12.0f),
      ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(WORKSPACE_WIDTH, 0.0f), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.92f);
  ImGui::Begin("WorkspaceStrip", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_AlwaysAutoResize |
                   ImGuiWindowFlags_NoSavedSettings);
  const auto workspace_button = [&](const char* label, Workspace workspace) {
    const bool active = parSession.workspace == workspace;
    if (active) {
      ImGui::PushStyleColor(ImGuiCol_Button,
                            ImVec4(0.18f, 0.42f, 0.68f, 1.0f));
    }
    if (ImGui::Button(label, ImVec2(87.0f, 24.0f)) && !active) {
      parSession.workspace = workspace;
      parSession.shot_preview = false;
      parSession.solo_clip_preview = false;
      parEngine.getFilmPlayback().playing = false;
    }
    if (active) {
      ImGui::PopStyleColor();
    }
  };
  workspace_button("World Edit", Workspace::WorldEdit);
  ImGui::SameLine();
  workspace_button("Movie", Workspace::Movie);
  trackCurrentPanel();
  ImGui::End();

  constexpr float WIDTH = 276.0f;
  const float mode_x = std::max(area.position.x + area.size.x - WIDTH - 12.0f,
                                area.position.x + 12.0f);
  const float workspace_right =
      area.position.x + (area.size.x + WORKSPACE_WIDTH) * 0.5f;
  const float mode_y = mode_x < workspace_right + 12.0f
                           ? area.position.y + 58.0f
                           : area.position.y + 12.0f;
  ImGui::SetNextWindowPos(ImVec2(
                              mode_x, mode_y),
                          ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.88f);
  ImGui::Begin("ViewportModeStrip", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_AlwaysAutoResize |
                   ImGuiWindowFlags_NoSavedSettings);
  trackCurrentPanel();
  struct ModeButton {
    const char* label;
    const char* tooltip;
    render::ViewportMode mode;
  };
  constexpr std::array<ModeButton, 4> MODES{{
      {"Bounds", "Bounds only: fastest navigation",
       render::ViewportMode::Bounds},
      {"Solid", "Untextured solid viewport", render::ViewportMode::Solid},
      {"Material", "Material preview",
       render::ViewportMode::Material},
      {"Final", "Full material and lighting preview",
       render::ViewportMode::Final},
  }};
  for (std::size_t index = 0; index < MODES.size(); ++index) {
    if (index != 0) {
      ImGui::SameLine();
    }
    const bool active = parEngine.getViewportMode() == MODES[index].mode;
    if (active) {
      ImGui::PushStyleColor(ImGuiCol_Button,
                            ImVec4(0.18f, 0.42f, 0.68f, 1.0f));
    }
    if (ImGui::SmallButton(MODES[index].label)) {
      parEngine.setViewportMode(MODES[index].mode);
    }
    if (active) {
      ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", MODES[index].tooltip);
    }
  }
  ImGui::End();
}

bool EditorUi::isCursorOverPanel(const glm::vec2& parUiCursor) const {
  for (const UiPanelRect& panel : m_panel_rects) {
    if (panel.contains(parUiCursor)) {
      return true;
    }
  }
  return false;
}

void EditorUi::applyStyle() {
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 4.0f;
  style.ChildRounding = 4.0f;
  style.FrameRounding = 3.0f;
  style.PopupRounding = 4.0f;
  style.ScrollbarRounding = 4.0f;
  style.GrabRounding = 3.0f;
  style.WindowBorderSize = 1.0f;
  style.FrameBorderSize = 0.0f;
  style.ItemSpacing = ImVec2(8.0f, 6.0f);
  style.WindowPadding = ImVec2(12.0f, 12.0f);

  ImVec4* colors = style.Colors;
  colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.12f, 0.13f, 0.94f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.10f, 0.11f, 1.0f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.15f, 0.16f, 1.0f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.18f, 0.19f, 1.0f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.25f, 0.26f, 1.0f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.30f, 0.31f, 1.0f);
  colors[ImGuiCol_Button] = ImVec4(0.17f, 0.20f, 0.21f, 1.0f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.24f, 0.29f, 0.30f, 1.0f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.31f, 0.38f, 0.38f, 1.0f);
  colors[ImGuiCol_Header] = ImVec4(0.18f, 0.23f, 0.24f, 1.0f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.31f, 0.32f, 1.0f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.29f, 0.37f, 0.38f, 1.0f);
  colors[ImGuiCol_CheckMark] = ImVec4(0.76f, 0.84f, 0.62f, 1.0f);
  colors[ImGuiCol_SliderGrab] = ImVec4(0.76f, 0.84f, 0.62f, 1.0f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.90f, 0.92f, 0.70f, 1.0f);
}

void EditorUi::beginPanelTracking() {
  m_panel_rects.clear();
}

void EditorUi::trackCurrentPanel() {
  const ImVec2 min = ImGui::GetWindowPos();
  const ImVec2 size = ImGui::GetWindowSize();
  m_panel_rects.push_back(
      {glm::vec2(min.x, min.y), glm::vec2(min.x + size.x, min.y + size.y)});
}

void EditorUi::clampCurrentPanel(const char* parPanelName,
                                 bool parKeepAboveStatusStrip) {
  const UiWorkArea area = getUiWorkArea();
  const ImVec2 size = clampPanelSize(area, ImGui::GetWindowSize(),
                                    parKeepAboveStatusStrip);
  if (size.x != ImGui::GetWindowSize().x ||
      size.y != ImGui::GetWindowSize().y) {
    ImGui::SetWindowSize(parPanelName, size, ImGuiCond_Always);
  }

  const ImVec2 position = clampPanelPosition(
      area, ImGui::GetWindowPos(), ImGui::GetWindowSize(),
      parKeepAboveStatusStrip);
  if (position.x != ImGui::GetWindowPos().x ||
      position.y != ImGui::GetWindowPos().y) {
    ImGui::SetWindowPos(parPanelName, position, ImGuiCond_Always);
  }
}

void EditorUi::drawHiddenPanelButton() {
  const UiWorkArea area = getUiWorkArea();
  ImGui::SetNextWindowPos(area.position, ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(116.0f, 42.0f), ImGuiCond_Always);
  ImGui::Begin("EditorPanelToggle", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoSavedSettings);
  if (ImGui::Button("Editor", ImVec2(92.0f, 24.0f))) {
    m_panel_visible = true;
  }
  trackCurrentPanel();
  ImGui::End();
}

void EditorUi::drawHiddenInspectorButton() {
  const UiWorkArea area = getUiWorkArea();
  ImGui::SetNextWindowPos(
      ImVec2(area.position.x + area.size.x - 132.0f,
             area.position.y + area.size.y - STATUS_HEIGHT - 52.0f),
      ImGuiCond_Always);
  ImGui::Begin("InspectorFold", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_AlwaysAutoResize |
                   ImGuiWindowFlags_NoSavedSettings);
  if (ImGui::Button("Inspector", ImVec2(104.0f, 24.0f))) {
    m_inspector_visible = true;
  }
  trackCurrentPanel();
  ImGui::End();
}

void EditorUi::drawLeftPanel(engine::EngineCore& parEngine,
                             PlacementController& parPlacementController,
                             SelectionController& parSelectionController,
                             GizmoController& parGizmoController,
                             ConfirmationDialog& parConfirmationDialog,
                             const glm::vec2& parViewportSize) {
  static_cast<void>(parViewportSize);
  static_cast<void>(parGizmoController);
  const UiWorkArea area = getUiWorkArea();
  const float panel_height = std::max(320.0f, area.size.y - STATUS_HEIGHT);
  ImGui::SetNextWindowPos(area.position, ImGuiCond_Always);
  ImGui::SetNextWindowSize(
      ImVec2(LEFT_PANEL_WIDTH, panel_height),
      ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(280.0f, panel_height), ImVec2(area.size.x, panel_height));
  if (!ImGui::Begin("Editor", &m_panel_visible,
                    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove)) {
    trackCurrentPanel();
    ImGui::End();
    return;
  }

  drawSceneControls(parEngine, parConfirmationDialog);
  ImGui::Separator();
  drawOutliner(parEngine, parSelectionController, parConfirmationDialog);
  if (ImGui::CollapsingHeader("Add")) {
    drawCreationPalette(parEngine, parPlacementController);
  }
  if(ImGui::CollapsingHeader("Paintbrush")) {
    drawPaintbrushPalette(parEngine);
  }
  if (ImGui::CollapsingHeader("World")) {
    drawWorldControls(parEngine);
    ImGui::Separator();
    drawLightingPanel(parEngine);
  }
  trackCurrentPanel();
  ImGui::End();
}

void EditorUi::drawSceneControls(engine::EngineCore& parEngine,
                                 ConfirmationDialog& parConfirmationDialog) {
  const auto scenes = parEngine.getScenes();
  const std::size_t active_scene = parEngine.getActiveSceneIndex();
  const char* scene_name = active_scene < scenes.size()
                               ? scenes[active_scene].name.c_str()
                               : "No scene";
  ImGui::SetNextItemWidth(-86.0f);
  if (ImGui::BeginCombo("##Scene", scene_name)) {
    for (std::size_t scene_index = 0; scene_index < scenes.size();
         ++scene_index) {
      if (ImGui::Selectable(scenes[scene_index].name.c_str(),
                            scene_index == active_scene)) {
        parEngine.setActiveScene(scene_index);
        m_scene_name_buffer_index = static_cast<std::size_t>(-1);
      }
    }
    ImGui::Separator();
    if (ImGui::Selectable("New scene")) {
      parEngine.createScene("Scene " + std::to_string(scenes.size() + 1));
      m_scene_name_buffer_index = static_cast<std::size_t>(-1);
    }
    if (ImGui::Selectable("New local test scene")) {
      parEngine.createLocalScene("Local Test " +
                                 std::to_string(scenes.size() + 1));
      m_scene_name_buffer_index = static_cast<std::size_t>(-1);
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  const bool dirty = parEngine.isProjectDirty();
  if (ImGui::Button(dirty ? "Save *" : "Save", ImVec2(76.0f, 0.0f))) {
    parEngine.saveProject();
  }
  refreshSceneNameBuffer(parEngine);
  ImGui::SetNextItemWidth(160.0f);
  if (ImGui::InputText("Scene name", m_scene_name_buffer.data(),
                       m_scene_name_buffer.size())) {
    parEngine.renameScene(parEngine.getActiveSceneIndex(),
                          m_scene_name_buffer.data());
  }
  if (scenes.size() > 1 && ImGui::SmallButton("Delete current scene")) {
    ConfirmationDialog::Request request;
    request.title = "Delete Scene";
    request.message = "Delete scene \"" + std::string(scene_name) +
                      "\"? This cannot be undone.";
    request.confirm_text = "Delete";
    request.cancel_text = "Cancel";
    request.destructive = true;
    request.on_confirm = [&parEngine, active_scene, this]() {
      parEngine.deleteScene(active_scene);
      m_scene_name_buffer_index = static_cast<std::size_t>(-1);
    };
    parConfirmationDialog.request(std::move(request));
  }
}

void EditorUi::drawCreationPalette(
    engine::EngineCore& parEngine,
    PlacementController& parPlacementController) {
  ImGui::TextUnformatted("Create");
  if (ImGui::Button("Camera", ImVec2(-1.0f, 0.0f))) {
    parPlacementController.beginCamera(parEngine);
  }
  if (ImGui::Button("Point Light", ImVec2(-1.0f, 0.0f))) {
    parPlacementController.beginPointLight(parEngine);
  }
  const auto library = parEngine.getAssetLibrary();
  const char* selected_asset = m_selected_asset_index < library.size()
                                   ? library[m_selected_asset_index].label.c_str()
                                   : "Choose model";
  if (ImGui::BeginCombo("Model", selected_asset)) {
    for (std::size_t asset_index = 0; asset_index < library.size();
         ++asset_index) {
      const auto& asset = library[asset_index];
      const std::string label = asset.label + "  (" +
          getAssetCapabilityLabel(asset) + ")";
      if (ImGui::Selectable(label.c_str(),
                            asset_index == m_selected_asset_index)) {
        m_selected_asset_index = asset_index;
        parPlacementController.beginStaticAsset(parEngine, asset_index);
      }
    }
    ImGui::EndCombo();
  }
  if (ImGui::Button("Import Model...", ImVec2(-1.0f, 0.0f))) {
    m_model_import_browser.open("Import Model",
                                parEngine.getRuntimePaths().getModelDirectory());
  }
  if (!m_model_import_error.empty()) {
    ImGui::TextWrapped("Import error: %s", m_model_import_error.c_str());
  }
}
void EditorUi::drawPaintbrushPalette(engine::EngineCore& parEngine) {
  ImGui::Checkbox("Enable paintbrush", &m_paintbrush_enabled);

  if (!m_paintbrush_enabled) {
    ImGui::TextDisabled("Enable paintbrush to configure mesh placement");
    return;
  }

  const auto library = parEngine.getAssetLibrary();
  if (m_paintbrush_selected_assets.size() != library.size()) {
    m_paintbrush_selected_assets.resize(library.size(), false);
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
    bool selected = m_paintbrush_selected_assets[asset_index];
    ImGui::PushID(static_cast<int>(asset_index));
    if (ImGui::Checkbox(label.c_str(), &selected)) {
      m_paintbrush_selected_assets[asset_index] = selected;
    }
    ImGui::PopID();
  }

  ImGui::Separator();
  ImGui::SliderInt("Brush size", &m_paintbrush_brush_size, 1, 64);
  ImGui::SliderInt("Paint density", &m_paintbrush_paint_density, 1, 64);

  const std::size_t selected_count = static_cast<std::size_t>(std::count(
      m_paintbrush_selected_assets.begin(), m_paintbrush_selected_assets.end(),
      true));
  if (selected_count == 0) {
    ImGui::TextDisabled("Select at least one mesh to enable painting");
  } else {
    ImGui::TextDisabled("%zu mesh(es) selected", selected_count);
  }
}
void EditorUi::drawWorldControls(engine::EngineCore& parEngine) {
  ImGui::TextUnformatted("World");
  if (ImGui::Button("Frame World", ImVec2(-1.0f, 0.0f))) {
    parEngine.frameWorld();
  }
  int sky_preset = static_cast<int>(parEngine.getSkyPreset());
  if (ImGui::Combo("Sky", &sky_preset,
                   "Clear day\0Mountain dawn\0Warm dusk\0Dark studio\0Dark void\0")) {
    parEngine.setSkyPreset(static_cast<render::SkyPreset>(sky_preset));
  }

  render::EnvironmentSettings environment =
      parEngine.getEnvironmentSettings();
  const auto environments = parEngine.getEnvironmentAssets();
  const char* selected_environment = "None";
  for (const assets::EnvironmentAsset& asset : environments) {
    if (asset.id == environment.asset_id) {
      selected_environment = asset.label.c_str();
      break;
    }
  }
  bool environment_changed = false;
  if (ImGui::BeginCombo("Environment", selected_environment)) {
    if (ImGui::Selectable("None", !environment.asset_id.isValid())) {
      environment.asset_id = {};
      environment_changed = true;
    }
    for (const assets::EnvironmentAsset& asset : environments) {
      const bool selected = asset.id == environment.asset_id;
      const std::string label = asset.label + (asset.hdr ? " (HDR)" : " (LDR)");
      if (ImGui::Selectable(label.c_str(), selected)) {
        environment.asset_id = asset.id;
        environment_changed = true;
      }
    }
    ImGui::EndCombo();
  }
  ImGui::InputText("Panorama label", m_panorama_import_label_buffer.data(),
                   m_panorama_import_label_buffer.size());
  if (ImGui::Button("Import Panorama", ImVec2(-1.0f, 0.0f))) {
    m_panorama_import_browser.open(
        "Import Panorama", parEngine.getRuntimePaths().getTexturePath(""),
        FileBrowserFilter::Panorama);
  }
  if (!m_panorama_import_error.empty()) {
    ImGui::TextWrapped("%s", m_panorama_import_error.c_str());
  }
  switch (parEngine.getEnvironmentLoadState()) {
    case render::EnvironmentLoadState::None:
      break;
    case render::EnvironmentLoadState::Loading:
      ImGui::TextDisabled("Loading panorama...");
      break;
    case render::EnvironmentLoadState::Ready:
      ImGui::TextDisabled("Panorama ready");
      break;
    case render::EnvironmentLoadState::Error:
      ImGui::TextWrapped("Panorama error: %s",
                         parEngine.getEnvironmentError().c_str());
      break;
  }
  environment_changed |=
      ImGui::Checkbox("Panorama visible", &environment.visible);
  environment_changed |= ImGui::DragFloat(
      "Panorama intensity", &environment.intensity, 0.02f, 0.0f, 20.0f);
  environment_changed |= ImGui::DragFloat(
      "Panorama yaw", &environment.yaw_degrees, 0.5f, -360.0f, 360.0f);
  if (environment_changed) {
    parEngine.setEnvironmentSettings(std::move(environment));
  }

  bool floor_visible = parEngine.isFloorGridVisible();
  if (ImGui::Checkbox("Floor grid", &floor_visible)) {
    parEngine.setFloorGridVisible(floor_visible);
  }
  int grid_radius = parEngine.getFloorGridRadius();
  if (ImGui::DragInt("Grid radius", &grid_radius, 1.0f, 8, 1000)) {
    parEngine.setFloorGridRadius(grid_radius);
  }
  float view_distance = parEngine.getEditorViewDistance();
  if (ImGui::DragFloat("View distance", &view_distance, 2.0f, 5.0f,
                       5000.0f)) {
    parEngine.setEditorViewDistance(view_distance);
  }
}

void EditorUi::drawOutliner(engine::EngineCore& parEngine,
                            SelectionController& parSelectionController,
                            ConfirmationDialog& parConfirmationDialog) {
  ImGui::TextUnformatted("World Hierarchy");
  ImGui::SameLine();
  if (ImGui::SmallButton("Clear Selection")) {
    parEngine.clearSelection();
  }
  ImGui::BeginChild("WorldHierarchyScroll", ImVec2(0.0f, 220.0f), true);
  if (ImGui::BeginTable("WorldHierarchyTable", 3,
                        ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Entity");
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 68.0f);
    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 38.0f);
    ImGui::TableHeadersRow();

    for (const scene::EntityRecord& entity : parEngine.getWorld().getEntities()) {
      if (!entity.alive) {
        continue;
      }

      ImGui::PushID(static_cast<int>(entity.id.value));
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      const bool selected = entity.id == parEngine.getSelectedEntity();
      if (ImGui::Selectable(entity.name.name.c_str(), selected,
                            ImGuiSelectableFlags_SpanAllColumns |
                                ImGuiSelectableFlags_AllowDoubleClick)) {
        const bool frame_entity =
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        parSelectionController.selectFromOutliner(parEngine, entity.id,
                                                  frame_entity);
      }
      if (ImGui::BeginPopupContextItem("EntityActions")) {
        if (ImGui::MenuItem("Delete")) {
          requestEntityDeletionConfirmation(parConfirmationDialog, parEngine,
                                              entity.id, entity.name.name);
        }
        ImGui::EndPopup();
      }
      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(getEntityTypeLabel(entity));
      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%u", entity.id.value);
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
  ImGui::EndChild();
  const scene::EntityRecord* selected =
      parEngine.getWorld().findEntity(parEngine.getSelectedEntity());
  if (selected == nullptr) {
    ImGui::BeginDisabled();
  }
  pushDestructiveButtonStyle();
  if (ImGui::Button("Delete Selected", ImVec2(-1.0f, 28.0f)) &&
      selected != nullptr) {
    requestEntityDeletionConfirmation(parConfirmationDialog, parEngine,
                                       selected->id, selected->name.name);
  }
  popDestructiveButtonStyle();
  if (selected == nullptr) {
    ImGui::EndDisabled();
  }
}

void EditorUi::drawInspector(engine::EngineCore& parEngine,
                             GizmoController& parGizmoController,
                             ConfirmationDialog& parConfirmationDialog,
                             const glm::vec2& parViewportSize) {
  static_cast<void>(parViewportSize);
  const UiWorkArea area = getUiWorkArea();
  const ImVec2 size = clampPanelSize(
      area, ImVec2(INSPECTOR_WIDTH, INSPECTOR_HEIGHT), true);
  const ImVec2 position = clampPanelPosition(
      area,
      ImVec2(area.position.x + area.size.x - size.x,
             area.position.y + area.size.y - STATUS_HEIGHT - size.y),
      size, true);
  ImGui::SetNextWindowPos(
      position, ImGuiCond_Always);
  ImGui::SetNextWindowSize(size, ImGuiCond_Always);
  const bool inspector_open =
      ImGui::Begin("Inspector", &m_inspector_visible,
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_NoResize);
  if (!inspector_open) {
    trackCurrentPanel();
    ImGui::End();
    return;
  }

  scene::EntityRecord* entity =
      parEngine.getWorld().findEntity(parEngine.getSelectedEntity());
  if (entity == nullptr) {
    camera::Camera& camera = parEngine.getCameraSystem().getEditorCamera();
    ImGui::TextUnformatted("Editor Camera");
    ImGui::Separator();
    ImGui::PushID("EditorCamera");
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
      bool changed = false;
      ImGui::TextDisabled("Position  X        Y        Z");
      ImGui::SetNextItemWidth(-1.0f);
      changed |= ImGui::DragFloat3("##PositionXYZ", &camera.position.x, 0.05f);

      glm::vec3 rotation_degrees =
          glm::degrees(glm::eulerAngles(camera.orientation));
      ImGui::TextDisabled("Rotation  X        Y        Z");
      ImGui::SetNextItemWidth(-1.0f);
      if (ImGui::DragFloat3("##RotationXYZ", &rotation_degrees.x, 0.5f)) {
        camera.orientation =
            glm::normalize(glm::quat(glm::radians(rotation_degrees)));
        changed = true;
      }
      if (changed) {
        parEngine.getCameraSystem().syncFlyControllerFromCamera();
      }
    }
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
      float fly_speed = parEngine.getCameraSystem().getFlyMoveSpeed();
      if (ImGui::DragFloat("Fly speed", &fly_speed, 0.1f, 0.1f, 1000.0f)) {
        parEngine.getCameraSystem().setFlyMoveSpeed(fly_speed);
      }
      ImGui::TextDisabled("Move: WASD | Shift down | Space up");
      ImGui::DragFloat("Field of view", &camera.vertical_fov_degrees, 0.2f,
                       10.0f, 120.0f);
      ImGui::DragFloat("Near", &camera.near_plane, 0.001f, 0.001f, 10.0f);
      ImGui::DragFloat("Far", &camera.far_plane, 0.5f, 1.0f, 5000.0f);
      camera.far_plane = std::max(camera.far_plane, camera.near_plane + 0.001f);
    }
    ImGui::PopID();
    trackCurrentPanel();
    ImGui::End();
    return;
  }

  refreshEntityNameBuffer(*entity);
  ImGui::Text("ID %u  |  %s", entity->id.value, getEntityTypeLabel(*entity));
  ImGui::SetNextItemWidth(-1.0f);
  if (ImGui::InputText("##EntityName", m_entity_name_buffer.data(),
                       m_entity_name_buffer.size())) {
    parEngine.setEntityName(entity->id, m_entity_name_buffer.data());
  }
  pushDestructiveButtonStyle();
  if (ImGui::Button("Delete", ImVec2(-1.0f, 0.0f))) {
    requestEntityDeletionConfirmation(parConfirmationDialog, parEngine, entity->id,
                        entity->name.name);
  }
  popDestructiveButtonStyle();

  if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
    int gizmo_mode = static_cast<int>(parGizmoController.getMode());
    if (ImGui::Combo("Gizmo", &gizmo_mode, "Move\0Scale\0Rotate\0")) {
      parGizmoController.setMode(
          static_cast<GizmoController::TransformMode>(gizmo_mode));
    }
    int axis_space = static_cast<int>(parGizmoController.getAxisSpace());
    if (ImGui::Combo("Axis space", &axis_space, "Local\0World\0")) {
      parGizmoController.setAxisSpace(
          static_cast<GizmoController::AxisSpace>(axis_space));
      parEngine.setGizmoAxisSpace(
          parGizmoController.getAxisSpace() == GizmoController::AxisSpace::World
              ? render::GizmoAxisSpace::World
              : render::GizmoAxisSpace::Local);
    }
    math::Transform edited_transform = entity->transform.transform;
    if (drawTransformControls(parEngine, *entity, edited_transform)) {
      parEngine.setEntityTransform(entity->id, edited_transform);
    }
    const math::Bounds3 world_bounds = parEngine.getEntityWorldBounds(entity->id);
    if (world_bounds.is_valid) {
      drawVector3("World size", world_bounds.getSize());
    }
  }

  if (entity->static_mesh.has_value() &&
      ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
    bool visible = entity->static_mesh->visible;
    if (ImGui::Checkbox("Visible", &visible)) {
      parEngine.setStaticMeshVisible(entity->id, visible);
    }
    drawVector3("Local bounds", entity->static_mesh->local_bounds.getSize());
    const assets::StaticModel* source =
        parEngine.getStaticMeshSource(entity->static_mesh->mesh_handle);
    if (source != nullptr) {
      int material_debug_mode =
          static_cast<int>(parEngine.getMaterialDebugMode());
      if (ImGui::Combo("Material view", &material_debug_mode,
                       "Lit\0Base Color\0Normal\0Roughness\0Metallic\0UV\0")) {
        parEngine.setMaterialDebugMode(
            static_cast<render::MaterialDebugMode>(material_debug_mode));
      }
      ImGui::Text("Source: %s",
                  source->source_path.filename().string().c_str());
      ImGui::Text("Primitives: %zu", source->stats.primitive_count);
      ImGui::Text("Vertices: %zu", source->stats.vertex_count);
      ImGui::Text("Materials: %zu", source->stats.material_count);
      ImGui::Text("Textures: %zu", source->stats.texture_count);
      if (entity->rig.has_value()) {
        ImGui::Text("Rig: skins %zu, joints %zu, clips %zu",
                    source->stats.skin_count, source->stats.joint_count,
                    source->stats.animation_count);
        ImGui::TextDisabled("Playback is controlled in the Movie Editor");
      }
      if (!source->import_warning.empty()) {
        ImGui::TextWrapped("Import warning: %s",
                           source->import_warning.c_str());
      }
    }
  }

  if (entity->camera.has_value() &&
      ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
    scene::CameraComponent camera = *entity->camera;
    bool changed = false;
    changed |= ImGui::DragFloat("Field of view", &camera.vertical_fov_degrees,
                                0.2f,
                                10.0f, 120.0f);
    changed |= ImGui::DragFloat("Near", &camera.near_plane, 0.001f,
                                0.001f, 10.0f);
    changed |= ImGui::DragFloat("Far", &camera.far_plane, 0.5f, 1.0f,
                                5000.0f);
    if (changed) {
      parEngine.setEntityCamera(entity->id, camera);
    }
  }

  if (entity->light.has_value() &&
      ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
    scene::LightComponent light = *entity->light;
    bool changed = false;
    ImGui::TextDisabled("Point light source");
    light.type = scene::LightType::Point;
    changed |= ImGui::Checkbox("Enabled", &light.enabled);
    changed |= ImGui::Checkbox("Cast shadows", &light.casts_shadows);
    changed |= ImGui::ColorEdit3("Color", &light.color.x);
    changed |= ImGui::DragFloat("Intensity", &light.intensity, 0.02f, 0.0f,
                                50.0f);
    changed |= ImGui::DragFloat("Range", &light.range, 0.1f, 0.1f,
                                200.0f);
    if (changed) {
      parEngine.setLight(entity->id, light);
    }
  }

  trackCurrentPanel();
  ImGui::End();
}

void EditorUi::drawImportDialogs(engine::EngineCore& parEngine,
                                 PlacementController& parPlacementController) {
  if (const std::optional<std::filesystem::path> path =
          m_model_import_browser.draw()) {
    const std::optional<std::size_t> imported_index =
        parEngine.importModelAsset(*path, {}, m_model_import_error);
    if (imported_index.has_value()) {
      m_selected_asset_index = *imported_index;
      parPlacementController.beginStaticAsset(parEngine, *imported_index);
      m_model_import_error.clear();
    }
  }

  if (const std::optional<std::filesystem::path> path =
          m_animation_import_browser.draw()) {
    if (parEngine.importAnimationForEntity(
            parEngine.getSelectedEntity(), *path,
            m_animation_import_label_buffer.data(),
            m_animation_import_error)) {
      m_animation_import_error.clear();
      m_animation_import_label_buffer.fill('\0');
    }
  }

  if (const std::optional<std::filesystem::path> path =
          m_panorama_import_browser.draw()) {
    if (parEngine.importPanorama(*path,
                                 m_panorama_import_label_buffer.data(),
                                 m_panorama_import_error)
            .has_value()) {
      m_panorama_import_error.clear();
      m_panorama_import_label_buffer.fill('\0');
    }
  }
}

void EditorUi::refreshSceneNameBuffer(engine::EngineCore& parEngine) {
  const std::size_t active_scene = parEngine.getActiveSceneIndex();
  if (m_scene_name_buffer_index == active_scene) {
    return;
  }

  m_scene_name_buffer.fill('\0');
  const auto scenes = parEngine.getScenes();
  if (active_scene < scenes.size()) {
    copyToBuffer(scenes[active_scene].name, m_scene_name_buffer);
  }
  m_scene_name_buffer_index = active_scene;
}

void EditorUi::refreshEntityNameBuffer(const scene::EntityRecord& parEntity) {
  if (m_entity_name_buffer_id == parEntity.id.value) {
    return;
  }

  m_entity_name_buffer.fill('\0');
  copyToBuffer(parEntity.name.name, m_entity_name_buffer);
  m_entity_name_buffer_id = parEntity.id.value;
}

const char* EditorUi::getEntityTypeLabel(
    const scene::EntityRecord& parEntity) const {
  if (parEntity.camera.has_value()) {
    return "Camera";
  }
  if (parEntity.light.has_value()) {
    return "Light";
  }
  if (parEntity.static_mesh.has_value()) {
    return "Mesh";
  }

  return "Entity";
}

}  // namespace kage::editor
