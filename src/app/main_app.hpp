#pragma once

#include "editor/world_editor.hpp"
#include "engine/engine_core.hpp"
#include "input/input_events.hpp"

#include <framework/app.hpp>

#include <glm/glm.hpp>

#include <string>

namespace kage::app {

class MainApp final : public App {
 public:
  MainApp();

 protected:
  void render() override;
  void buildImGui() override;
  void keyCallback(Key parKey, Action parAction,
                   Modifier parModifier) override;
  void clickCallback(Button parButton, Action parAction,
                     Modifier parModifier) override;
  void moveCallback(const glm::vec2& parMovement, bool parLeftButton,
                    bool parRightButton, bool parMiddleButton) override;
  void scrollCallback(float parXAmount, float parYAmount) override;

 private:
  [[nodiscard]] input::EditorInputSnapshot collectInputSnapshot();
  [[nodiscard]] bool isKeyPressed(Key parKey) const;
  [[nodiscard]] bool isMouseButtonPressed(Button parButton) const;

  engine::EngineCore m_engine;
  editor::WorldEditor m_editor;
  glm::vec2 m_last_ui_cursor{0.0f};
  bool m_last_left_mouse_down = false;
  bool m_last_right_mouse_down = false;
  bool m_last_middle_mouse_down = false;
  bool m_last_escape_down = false;
  bool m_last_delete_down = false;
  bool m_last_tab_down = false;
  float m_scroll_y = 0.0f;
  std::string m_imgui_ini_path;
};

}  // namespace kage::app
