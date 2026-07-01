#pragma once

#include "editor/placement_controller.hpp"
#include "editor/selection_controller.hpp"
#include "editor/gizmo_controller.hpp"
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
            PlacementController& parPlacementController,
            SelectionController& parSelectionController,
            GizmoController& parGizmoController,
            const glm::vec2& parViewportSize, float parDeltaSeconds,
            unsigned int parFrameCount);
  void togglePanelVisibility();
  [[nodiscard]] bool isPanelVisible() const;
  [[nodiscard]] bool isCursorOverPanel(const glm::vec2& parUiCursor) const;

 private:
  struct PanelRect final {
    glm::vec2 min{0.0f};
    glm::vec2 max{0.0f};

    [[nodiscard]] bool contains(const glm::vec2& parPoint) const;
  };

  void applyStyle();
  void beginPanelTracking();
  void trackCurrentPanel();
  void clampCurrentPanel(const char* parPanelName,
                         bool parKeepAboveStatusStrip);
  void drawHiddenPanelButton();
  void drawLeftPanel(engine::EngineCore& parEngine,
                     PlacementController& parPlacementController,
                     SelectionController& parSelectionController,
                     GizmoController& parGizmoController,
                     const glm::vec2& parViewportSize);
  void drawSceneControls(engine::EngineCore& parEngine);
  void drawCreationPalette(engine::EngineCore& parEngine,
                           PlacementController& parPlacementController);
  void drawAssetLibrary(engine::EngineCore& parEngine,
                        PlacementController& parPlacementController);
  void drawWorldControls(engine::EngineCore& parEngine);
  void drawOutliner(engine::EngineCore& parEngine,
                    SelectionController& parSelectionController);
  void drawInspector(engine::EngineCore& parEngine,
                     GizmoController& parGizmoController,
                     const glm::vec2& parViewportSize);
  void drawRuntimeDiagnostics(engine::EngineCore& parEngine,
                              const glm::vec2& parViewportSize,
                              float parDeltaSeconds,
                              unsigned int parFrameCount);
  void drawStatusStrip(engine::EngineCore& parEngine,
                       const PlacementController& parPlacementController,
                       const GizmoController& parGizmoController,
                       const glm::vec2& parViewportSize);
  void drawImportDiagnostics(const assets::StaticModel& parModel);
  void refreshSceneNameBuffer(engine::EngineCore& parEngine);
  void refreshEntityNameBuffer(const scene::EntityRecord& parEntity);

  [[nodiscard]] const char* getEntityTypeLabel(
      const scene::EntityRecord& parEntity) const;

  std::array<char, 128> m_scene_name_buffer{};
  std::array<char, 128> m_entity_name_buffer{};
  std::size_t m_scene_name_buffer_index = static_cast<std::size_t>(-1);
  std::uint32_t m_entity_name_buffer_id =
      std::numeric_limits<std::uint32_t>::max();
  std::size_t m_selected_asset_index = 0;
  bool m_panel_visible = true;
  bool m_inspector_visible = true;
  bool m_diagnostics_visible = false;
  std::vector<PanelRect> m_panel_rects;
};

inline bool EditorUi::PanelRect::contains(const glm::vec2& parPoint) const {
  return parPoint.x >= min.x && parPoint.x <= max.x &&
         parPoint.y >= min.y && parPoint.y <= max.y;
}

}  // namespace kage::editor
