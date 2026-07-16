#pragma once

#include <glm/glm.hpp>

namespace kage::input {

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
  bool key_w_down = false;
  bool key_a_down = false;
  bool key_s_down = false;
  bool key_d_down = false;
  bool key_space_down = false;
  bool key_shift_down = false;
  bool key_escape_pressed = false;
  bool key_delete_pressed = false;
  bool key_z_pressed = false;
  bool wants_capture_mouse = false;
  bool wants_capture_keyboard = false;
  bool ui_item_active = false;
  bool ui_popup_open = false;
  float scroll_y = 0.0f;
};

}  // namespace kage::input
