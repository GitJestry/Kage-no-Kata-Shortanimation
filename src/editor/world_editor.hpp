#pragma once

#include "editor/editor_ui.hpp"
#include "editor/gizmo_controller.hpp"
#include "editor/placement_controller.hpp"
#include "editor/selection_controller.hpp"
#include "engine/engine_core.hpp"
#include "input/input_events.hpp"
#include "platform/runtime_paths.hpp"

#include <glm/glm.hpp>

namespace kage::editor {

class WorldEditor final {
 public:
  explicit WorldEditor(engine::EngineCore& parEngine);
  ~WorldEditor();

  WorldEditor(const WorldEditor&) = delete;
  WorldEditor& operator=(const WorldEditor&) = delete;

  void update(float parDeltaSeconds,
              const input::EditorInputSnapshot& parInput);
  void render(const glm::vec2& parViewportSize);
  void buildImGui(const glm::vec2& parViewportSize, float parDeltaSeconds,
                  unsigned int parFrameCount);
  bool cancelActiveOperation();

 private:
  void registerDefaultAssets();
  void applyCameraMovement(const input::EditorInputSnapshot& parInput);
  void handlePointerInput(const input::EditorInputSnapshot& parInput);

  engine::EngineCore& m_engine;
  platform::RuntimePaths m_runtime_paths;
  EditorUi m_ui;
  PlacementController m_placement_controller;
  SelectionController m_selection_controller;
  GizmoController m_gizmo_controller;
  bool m_right_look_active = false;
};

}  // namespace kage::editor
