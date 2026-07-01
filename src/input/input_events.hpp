#pragma once

#include <glm/glm.hpp>

namespace kage::input {

enum class Key {
  Unknown,
  Escape,
  Space,
  W,
  A,
  S,
  D,
  F,
  Delete,
  Tab,
  LeftShift
};

enum class KeyAction {
  Release,
  Press,
  Repeat
};

enum class MouseButton {
  Unknown,
  Left,
  Right,
  Middle
};

enum class PointerAction {
  Release,
  Press
};

struct EditorInputSnapshot final {
  glm::vec2 ui_cursor{0.0f};
  glm::vec2 ui_delta{0.0f};
  glm::vec2 framebuffer_cursor{0.0f};
  glm::vec2 framebuffer_size{1.0f};
  glm::vec2 ui_to_framebuffer_scale{1.0f};
  bool left_mouse_down = false;
  bool right_mouse_down = false;
  bool middle_mouse_down = false;
  bool left_mouse_pressed = false;
  bool right_mouse_pressed = false;
  bool middle_mouse_pressed = false;
  bool left_mouse_released = false;
  bool right_mouse_released = false;
  bool middle_mouse_released = false;
  bool key_w_down = false;
  bool key_a_down = false;
  bool key_s_down = false;
  bool key_d_down = false;
  bool key_space_down = false;
  bool key_shift_down = false;
  bool key_escape_pressed = false;
  bool key_delete_pressed = false;
  bool key_tab_pressed = false;
  bool wants_capture_mouse = false;
  bool wants_capture_keyboard = false;
  bool viewport_hovered = false;
  float scroll_y = 0.0f;
};

}  // namespace kage::input
