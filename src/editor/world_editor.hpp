#pragma once

#include "editor/confirmation_dialog.hpp"
#include "editor/editor_session.hpp"
#include "editor/editor_ui.hpp"
#include "editor/gizmo_controller.hpp"
#include "editor/paintbrush_controller.hpp"
#include "editor/placement_controller.hpp"
#include "editor/selection_controller.hpp"
#include "engine/engine_core.hpp"
#include "input/input_events.hpp"
#include "render/viewport_rect.hpp"

#include <glm/glm.hpp>

namespace kage::editor {

class WorldEditor final {
public:
  explicit WorldEditor(engine::EngineCore& parEngine);
  ~WorldEditor();

  WorldEditor(const WorldEditor&) = delete;
  WorldEditor& operator=(const WorldEditor&) = delete;

  void update(float parDeltaSeconds, const input::EditorInputSnapshot& parInput);
  void render(const glm::vec2& parViewportSize);
  void buildImGui(float parDeltaSeconds);
  bool cancelActiveOperation();

private:
  void registerDefaultAssets();
  void applyCameraMovement(const input::EditorInputSnapshot& parInput,
                           bool parCameraSequencePreview);
  void handlePointerInput(const input::EditorInputSnapshot& parInput,
                          bool parCameraSequencePreview);
  void publishGizmoGuide();
  void updateViewportRect(const input::EditorInputSnapshot& parInput);

  engine::EngineCore& m_engine;
  EditorUi m_ui;
  EditorSession m_session;
  ConfirmationDialog m_confirmation_dialog;
  PlacementController m_placement_controller;
  SelectionController m_selection_controller;
  GizmoController m_gizmo_controller;
  PaintbrushController m_paintbrush_controller;
  bool m_right_look_active = false;
  render::ViewportRect m_viewport;
};

} // namespace kage::editor
