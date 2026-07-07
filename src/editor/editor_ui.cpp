#include "editor/editor_ui.hpp"

#include "editor/lighting_panel.hpp"
#include "editor/timeline_panel.hpp"
#include "editor/ui_layout.hpp"
#include "math/screen_projection.hpp"

#include <imgui.h>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
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

[[nodiscard]] float getLargestExtent(const kage::math::Bounds3& parBounds) {
  const glm::vec3 size = parBounds.getSize();
  return std::max({size.x, size.y, size.z, 1.0f});
}

void drawAxisLabels(kage::engine::EngineCore& parEngine,
                    const glm::vec2& parViewportSize) {
  const kage::scene::EntityRecord* entity =
      parEngine.getWorld().findEntity(parEngine.getSelectedEntity());
  if (entity == nullptr || entity->id == parEngine.getEditorCameraEntity()) {
    return;
  }

  const ImGuiIO& io = ImGui::GetIO();
  const glm::vec2 ui_size =
      glm::max(glm::vec2(io.DisplaySize.x, io.DisplaySize.y), glm::vec2(1.0f));
  const glm::vec2 framebuffer_size =
      glm::max(parViewportSize, glm::vec2(1.0f));
  const glm::vec2 framebuffer_to_ui = ui_size / framebuffer_size;
  const float axis_length =
      getLargestExtent(parEngine.getEntityWorldBounds(entity->id)) * 0.42f;
  const glm::vec3 origin = entity->transform.transform.translation;
  const glm::mat4 view_projection =
      parEngine.getCameraSystem().getCamera().getViewProjectionMatrix(
          framebuffer_size);
  const std::array<glm::vec3, 3> axes =
      parEngine.getGizmoAxisSpace() == kage::render::GizmoAxisSpace::World
          ? std::array<glm::vec3, 3>{glm::vec3(1.0f, 0.0f, 0.0f),
                                     glm::vec3(0.0f, 1.0f, 0.0f),
                                     glm::vec3(0.0f, 0.0f, 1.0f)}
          : std::array<glm::vec3, 3>{
                entity->transform.transform.rotation *
                    glm::vec3(1.0f, 0.0f, 0.0f),
                entity->transform.transform.rotation *
                    glm::vec3(0.0f, 1.0f, 0.0f),
                entity->transform.transform.rotation *
                    glm::vec3(0.0f, 0.0f, 1.0f)};
  constexpr std::array<const char*, 3> LABELS = {"X", "Y", "Z"};
  constexpr std::array<ImU32, 3> COLORS = {
      IM_COL32(46, 209, 72, 255),
      IM_COL32(255, 219, 56, 255),
      IM_COL32(56, 122, 255, 255),
  };

  ImDrawList* draw_list = ImGui::GetForegroundDrawList();
  for (std::size_t axis_index = 0; axis_index < axes.size(); ++axis_index) {
    const kage::math::ScreenPoint point = kage::math::projectPoint(
        origin + axes[axis_index] * axis_length, view_projection,
        framebuffer_size);
    if (!point.valid) {
      continue;
    }
    const glm::vec2 ui_point = point.pixel * framebuffer_to_ui;
    draw_list->AddText(ImVec2(ui_point.x + 4.0f, ui_point.y + 4.0f),
                       COLORS[axis_index], LABELS[axis_index]);
  }
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

void EditorUi::draw(engine::EngineCore& parEngine,
                    PlacementController& parPlacementController,
                    SelectionController& parSelectionController,
                    GizmoController& parGizmoController,
                    ConfirmationDialog& parConfirmationDialog,
                    const glm::vec2& parViewportSize, float parDeltaSeconds,
                    unsigned int parFrameCount) {
  applyStyle();
  beginPanelTracking();
  if (!m_panel_visible) {
    drawHiddenPanelButton();
  } else {
    drawLeftPanel(parEngine, parPlacementController, parSelectionController,
                  parGizmoController, parConfirmationDialog, parViewportSize);
  }

  if (m_diagnostics_visible) {
    drawRuntimeDiagnostics(parEngine, parViewportSize, parDeltaSeconds,
                           parFrameCount);
  }
  if (m_inspector_visible) {
    drawInspector(parEngine, parGizmoController, parConfirmationDialog,
                  parViewportSize);
  }
  if (m_timeline_visible) {
    if (std::optional<UiPanelRect> timeline_rect =
            drawTimelinePanel(parEngine, m_timeline_visible, parViewportSize,
                              m_animation_import_browser,
                              m_animation_import_label_buffer,
                              m_animation_import_error)) {
      m_panel_rects.push_back(*timeline_rect);
    }
  }

  drawStatusStrip(parEngine, parPlacementController, parGizmoController,
                  parViewportSize);
  drawAxisLabels(parEngine, parViewportSize);
  drawImportDialogs(parEngine, parPlacementController);
}

void EditorUi::togglePanelVisibility() {
  m_panel_visible = !m_panel_visible;
}

bool EditorUi::isPanelVisible() const {
  return m_panel_visible;
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
  ImGui::SetNextWindowPos(
      ImVec2(area.position.x + 12.0f, area.position.y + 12.0f),
      ImGuiCond_Always);
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

void EditorUi::drawLeftPanel(engine::EngineCore& parEngine,
                             PlacementController& parPlacementController,
                             SelectionController& parSelectionController,
                             GizmoController& parGizmoController,
                             ConfirmationDialog& parConfirmationDialog,
                             const glm::vec2& parViewportSize) {
  static_cast<void>(parViewportSize);
  static_cast<void>(parGizmoController);
  const UiWorkArea area = getUiWorkArea();
  ImGui::SetNextWindowPos(area.position, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(
      ImVec2(LEFT_PANEL_WIDTH, area.size.y - STATUS_HEIGHT),
      ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(280.0f, 320.0f),
      ImVec2(420.0f, std::max(320.0f, area.size.y - STATUS_HEIGHT)));
  if (!ImGui::Begin("Editor", &m_panel_visible,
                    ImGuiWindowFlags_NoCollapse)) {
    clampCurrentPanel("Editor", true);
    trackCurrentPanel();
    ImGui::End();
    return;
  }
  clampCurrentPanel("Editor", true);

  drawSceneControls(parEngine, parConfirmationDialog);
  ImGui::Separator();
  drawCreationPalette(parEngine, parPlacementController);
  ImGui::Separator();
  drawWorldControls(parEngine);
  ImGui::Separator();
  drawLightingPanel(parEngine);
  ImGui::Separator();
  drawOutliner(parEngine, parSelectionController, parConfirmationDialog);
  trackCurrentPanel();
  ImGui::End();
}

void EditorUi::drawSceneControls(engine::EngineCore& parEngine,
                                 ConfirmationDialog& parConfirmationDialog) {
  ImGui::TextUnformatted("Scenes");
  ImGui::SameLine();
  const bool dirty = parEngine.isProjectDirty();
  ImGui::TextColored(dirty ? ImVec4(1.0f, 0.70f, 0.22f, 1.0f)
                           : ImVec4(0.34f, 0.90f, 0.46f, 1.0f),
                     "%s", dirty ? "Unsaved" : "Saved");
  if (ImGui::Button("Save Project", ImVec2(-1.0f, 0.0f))) {
    parEngine.saveProject();
  }
  refreshSceneNameBuffer(parEngine);
  ImGui::SetNextItemWidth(-1.0f);
  if (ImGui::InputText("##SceneName", m_scene_name_buffer.data(),
                       m_scene_name_buffer.size())) {
    parEngine.renameScene(parEngine.getActiveSceneIndex(),
                          m_scene_name_buffer.data());
  }

  const auto scenes = parEngine.getScenes();
  const std::size_t active_scene = parEngine.getActiveSceneIndex();
  if (ImGui::BeginTable("SceneListTable", 4,
                        ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Name");
    ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthFixed, 48.0f);
    ImGui::TableSetupColumn("Use", ImGuiTableColumnFlags_WidthFixed, 44.0f);
    ImGui::TableSetupColumn("Del", ImGuiTableColumnFlags_WidthFixed, 64.0f);
    ImGui::TableHeadersRow();

    for (std::size_t scene_index = 0; scene_index < scenes.size();
         ++scene_index) {
      ImGui::PushID(static_cast<int>(scene_index));
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(scenes[scene_index].name.c_str());
      ImGui::TableSetColumnIndex(1);
      ImGui::TextDisabled("%s", scenes[scene_index].local_only ? "Local"
                                                               : "Shared");
      ImGui::TableSetColumnIndex(2);
      if (ImGui::Selectable("Open", scene_index == active_scene)) {
        parEngine.setActiveScene(scene_index);
        m_scene_name_buffer_index = static_cast<std::size_t>(-1);
      }
      ImGui::TableSetColumnIndex(3);
      if (scenes.size() <= 1) {
        ImGui::BeginDisabled();
      }
      pushDestructiveButtonStyle();
      if (ImGui::Button("Delete##DeleteScene", ImVec2(58.0f, 0.0f))) {
        ConfirmationDialog::Request request;
        request.title = "Delete Scene";
        request.message = "Delete scene \"" + scenes[scene_index].name +
                          "\"? This cannot be undone.";
        request.confirm_text = "Delete";
        request.cancel_text = "Cancel";
        request.destructive = true;
        request.on_confirm = [&parEngine, scene_index, this]() {
          parEngine.deleteScene(scene_index);
          m_scene_name_buffer_index = static_cast<std::size_t>(-1);
        };
        parConfirmationDialog.request(std::move(request));
      }
      popDestructiveButtonStyle();
      if (scenes.size() <= 1) {
        ImGui::EndDisabled();
      }
      ImGui::PopID();
    }
    ImGui::EndTable();
  }

  if (ImGui::Button("+ Scene", ImVec2(-1.0f, 0.0f))) {
    parEngine.createScene("Scene " + std::to_string(scenes.size() + 1));
    m_scene_name_buffer_index = static_cast<std::size_t>(-1);
  }
  if (ImGui::Button("+ Local Test Scene", ImVec2(-1.0f, 0.0f))) {
    parEngine.createLocalScene("Local Test " +
                               std::to_string(scenes.size() + 1));
    m_scene_name_buffer_index = static_cast<std::size_t>(-1);
  }
}

void EditorUi::drawCreationPalette(
    engine::EngineCore& parEngine,
    PlacementController& parPlacementController) {
  ImGui::TextUnformatted("Create");
  if (parPlacementController.isActive()) {
    ImGui::SameLine();
    ImGui::TextDisabled("%s", parPlacementController.getStatusLabel());
  }

  if (ImGui::BeginTable("CreationPaletteTable", 3,
                        ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Item");
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 78.0f);
    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 76.0f);
    ImGui::TableHeadersRow();

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("Entities");
    ImGui::TableSetColumnIndex(1);
    ImGui::TextDisabled("built-in");
    ImGui::TableSetColumnIndex(2);
    ImGui::TextDisabled("-");

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    if (ImGui::Selectable("Camera", false,
                          ImGuiSelectableFlags_SpanAllColumns)) {
      parPlacementController.beginCamera(parEngine);
    }
    ImGui::TableSetColumnIndex(1);
    ImGui::TextDisabled("view");
    ImGui::TableSetColumnIndex(2);
    ImGui::TextDisabled("-");

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    if (ImGui::Selectable("Light Source", false,
                          ImGuiSelectableFlags_SpanAllColumns)) {
      parPlacementController.beginPointLight(parEngine);
    }
    ImGui::TableSetColumnIndex(1);
    ImGui::TextDisabled("point");
    ImGui::TableSetColumnIndex(2);
    ImGui::TextDisabled("-");

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("Assets");
    ImGui::TableSetColumnIndex(1);
    ImGui::TextDisabled("world");
    ImGui::TableSetColumnIndex(2);
    ImGui::TextDisabled("loaded");

    const auto assets = parEngine.getAssetLibrary();
    for (std::size_t asset_index = 0; asset_index < assets.size();
         ++asset_index) {
      const assets::AssetRegistry::AssetLibraryEntry& asset =
          assets[asset_index];
      ImGui::PushID(static_cast<int>(asset_index));
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      const bool selected = m_selected_asset_index == asset_index;
      if (ImGui::Selectable(asset.label.c_str(), selected,
                            ImGuiSelectableFlags_SpanAllColumns)) {
        m_selected_asset_index = asset_index;
        parPlacementController.beginStaticAsset(parEngine, asset_index);
      }
      ImGui::TableSetColumnIndex(1);
      const std::string capability = getAssetCapabilityLabel(asset);
      ImGui::TextDisabled("%s", capability.c_str());
      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%s  %zu", assets::getAssetLoadStateLabel(asset.load_state),
                  asset.instance_count);
      ImGui::PopID();
    }

    ImGui::EndTable();
  }

  ImGui::SeparatorText("Import");
  ImGui::SetNextItemWidth(-1.0f);
  ImGui::InputText("Label##ModelImportLabel",
                   m_model_import_label_buffer.data(),
                   m_model_import_label_buffer.size());
  if (ImGui::Button("Import Model...", ImVec2(-1.0f, 0.0f))) {
    m_model_import_browser.open("Import Model",
                                parEngine.getRuntimePaths().getModelDirectory());
  }
  if (!m_model_import_error.empty()) {
    ImGui::TextWrapped("Import error: %s", m_model_import_error.c_str());
  }
}

void EditorUi::drawWorldControls(engine::EngineCore& parEngine) {
  ImGui::TextUnformatted("World");
  int sky_preset = static_cast<int>(parEngine.getSkyPreset());
  if (ImGui::Combo("Sky", &sky_preset,
                   "Clear day\0Mountain dawn\0Warm dusk\0Dark studio\0Dark void\0")) {
    parEngine.setSkyPreset(static_cast<render::SkyPreset>(sky_preset));
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
  if (ImGui::BeginTable("WorldHierarchyTable", 4,
                        ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Entity");
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 68.0f);
    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 38.0f);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 64.0f);
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
      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(getEntityTypeLabel(entity));
      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%u", entity.id.value);
      ImGui::TableSetColumnIndex(3);
      if (entity.id == parEngine.getEditorCameraEntity()) {
        ImGui::BeginDisabled();
      }
      pushDestructiveButtonStyle();
      if (ImGui::Button("Delete##DeleteEntity", ImVec2(58.0f, 0.0f))) {
        requestEntityDeletionConfirmation(parConfirmationDialog, parEngine, entity.id,
                            entity.name.name);
      }
      popDestructiveButtonStyle();
      if (entity.id == parEngine.getEditorCameraEntity()) {
        ImGui::EndDisabled();
      }
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
  ImGui::EndChild();
}

void EditorUi::drawInspector(engine::EngineCore& parEngine,
                             GizmoController& parGizmoController,
                             ConfirmationDialog& parConfirmationDialog,
                             const glm::vec2& parViewportSize) {
  static_cast<void>(parViewportSize);
  const UiWorkArea area = getUiWorkArea();
  const ImVec2 size(INSPECTOR_WIDTH, INSPECTOR_HEIGHT);
  const ImVec2 position = clampPanelPosition(
      area,
      ImVec2(area.position.x + area.size.x - INSPECTOR_WIDTH - 16.0f,
             area.position.y + area.size.y - INSPECTOR_HEIGHT -
                 STATUS_HEIGHT - 18.0f),
      size, true);
  ImGui::SetNextWindowPos(
      position, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
  const bool inspector_open =
      ImGui::Begin("Inspector", &m_inspector_visible,
                   ImGuiWindowFlags_NoCollapse);
  clampCurrentPanel("Inspector", true);
  if (!inspector_open) {
    trackCurrentPanel();
    ImGui::End();
    return;
  }

  scene::EntityRecord* entity =
      parEngine.getWorld().findEntity(parEngine.getSelectedEntity());
  if (entity == nullptr) {
    ImGui::TextDisabled("No entity selected");
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
  const bool can_delete = entity->id != parEngine.getEditorCameraEntity();
  if (!can_delete) {
    ImGui::BeginDisabled();
  }
  pushDestructiveButtonStyle();
  if (ImGui::Button("Delete", ImVec2(-1.0f, 0.0f))) {
    requestEntityDeletionConfirmation(parConfirmationDialog, parEngine, entity->id,
                        entity->name.name);
  }
  popDestructiveButtonStyle();
  if (!can_delete) {
    ImGui::EndDisabled();
  }

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
    float opacity = entity->static_mesh->opacity;
    if (ImGui::DragFloat("Opacity", &opacity, 0.01f, 0.05f, 1.0f)) {
      parEngine.setStaticMeshOpacity(entity->id, opacity);
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
        ImGui::TextDisabled("Playback is controlled in the Timeline panel");
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
        parEngine.importModelAsset(
            *path, m_model_import_label_buffer.data(), m_model_import_error);
    if (imported_index.has_value()) {
      m_selected_asset_index = *imported_index;
      parPlacementController.beginStaticAsset(parEngine, *imported_index);
      m_model_import_error.clear();
      m_model_import_label_buffer.fill('\0');
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
