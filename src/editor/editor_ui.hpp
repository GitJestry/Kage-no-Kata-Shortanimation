#pragma once

#include "editor/placement_controller.hpp"
#include "editor/selection_controller.hpp"
#include "editor/gizmo_controller.hpp"
#include "editor/confirmation_dialog.hpp"
#include "editor/file_browser_dialog.hpp"
#include "editor/editor_session.hpp"
#include "editor/ui_panel_rect.hpp"
#include "engine/engine_core.hpp"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <vector>

namespace kage::editor {

class EditorUi final {
 public:
  void draw(engine::EngineCore& parEngine,
            EditorSession& parSession,
            PlacementController& parPlacementController,
            SelectionController& parSelectionController,
            GizmoController& parGizmoController,
            ConfirmationDialog& parConfirmationDialog,
            const glm::vec2& parViewportSize, float parDeltaSeconds,
            unsigned int parFrameCount);
  [[nodiscard]] bool isCursorOverPanel(const glm::vec2& parUiCursor) const;

  [[nodiscard]] bool isPaintbrushEnabled() const;
  [[nodiscard]] int getPaintbrushBrushSize() const;
  [[nodiscard]] int getPaintbrushPaintDensity() const;
  [[nodiscard]] std::vector<std::size_t> getPaintbrushSelectedAssetIndices() const;

 private:
  void applyStyle();
  void beginPanelTracking();
  void trackCurrentPanel();
  void clampCurrentPanel(const char* parPanelName,
                         bool parKeepAboveStatusStrip);
  void drawHiddenPanelButton();
  void drawHiddenInspectorButton();
  void drawTopBar(engine::EngineCore& parEngine, EditorSession& parSession,
                             const glm::vec2& parViewportSize);
  void drawLeftPanel(engine::EngineCore& parEngine,
                     PlacementController& parPlacementController,
                     SelectionController& parSelectionController,
                     GizmoController& parGizmoController,
                     ConfirmationDialog& parConfirmationDialog,
                     const glm::vec2& parViewportSize);
  void drawSceneControls(engine::EngineCore& parEngine,
                         ConfirmationDialog& parConfirmationDialog);
  void drawCreationPalette(engine::EngineCore& parEngine,
                           PlacementController& parPlacementController);
  void drawPaintbrushPalette(engine::EngineCore& parEngine);
  void drawWorldControls(engine::EngineCore& parEngine);
  void drawOutliner(engine::EngineCore& parEngine,
                    SelectionController& parSelectionController,
                    ConfirmationDialog& parConfirmationDialog);
  void drawInspector(engine::EngineCore& parEngine,
                     GizmoController& parGizmoController,
                     ConfirmationDialog& parConfirmationDialog,
                     const glm::vec2& parViewportSize);
  void drawRuntimeDiagnostics(engine::EngineCore& parEngine,
                              const glm::vec2& parViewportSize,
                              float parDeltaSeconds,
                              unsigned int parFrameCount);
  void drawStatusStrip(engine::EngineCore& parEngine,
                       const PlacementController& parPlacementController,
                       const GizmoController& parGizmoController,
                       const glm::vec2& parViewportSize);
  void drawImportDialogs(engine::EngineCore& parEngine,
                         PlacementController& parPlacementController);
  void refreshSceneNameBuffer(engine::EngineCore& parEngine);
  void refreshEntityNameBuffer(const scene::EntityRecord& parEntity);

  [[nodiscard]] const char* getEntityTypeLabel(
      const scene::EntityRecord& parEntity) const;

  std::array<char, 128> m_scene_name_buffer{};
  std::array<char, 128> m_entity_name_buffer{};
  std::array<char, 128> m_animation_import_label_buffer{};
  std::array<char, 128> m_panorama_import_label_buffer{};
  std::size_t m_scene_name_buffer_index = static_cast<std::size_t>(-1);
  std::uint32_t m_entity_name_buffer_id =
      std::numeric_limits<std::uint32_t>::max();
  std::size_t m_selected_asset_index = 0;
  std::vector<bool> m_paintbrush_selected_assets;
  bool m_paintbrush_enabled = false;
  int m_paintbrush_brush_size = 4;
  int m_paintbrush_paint_density = 3;
  std::string m_model_import_error;
  std::string m_animation_import_error;
  std::string m_panorama_import_error;
  FileBrowserDialog m_model_import_browser;
  FileBrowserDialog m_animation_import_browser;
  FileBrowserDialog m_panorama_import_browser;
  bool m_panel_visible = true;
  bool m_inspector_visible = true;
  bool m_diagnostics_visible = false;
  std::vector<UiPanelRect> m_panel_rects;
};

}  // namespace kage::editor
