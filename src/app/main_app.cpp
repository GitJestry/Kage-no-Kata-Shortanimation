#include "app/main_app.hpp"

#include <imgui.h>

#include <algorithm>
#include <filesystem>
#include <utility>

namespace {

constexpr unsigned int WINDOW_WIDTH = 1280;
constexpr unsigned int WINDOW_HEIGHT = 720;

[[nodiscard]] bool isCursorInside(const glm::vec2& parCursor,
                                  const glm::vec2& parSize) {
  return parCursor.x >= 0.0f && parCursor.y >= 0.0f &&
         parCursor.x < parSize.x && parCursor.y < parSize.y;
}

}  // namespace

namespace kage::app {

MainApp::MainApp()
    : App(WINDOW_WIDTH, WINDOW_HEIGHT),
      m_editor(m_engine) {
  std::filesystem::create_directories(".kage_local");
  m_imgui_ini_path = ".kage_local/imgui.ini";
  ImGui::GetIO().IniFilename = m_imgui_ini_path.c_str();
  setTitle("Kage no Kata - The Final Cut");
  setVSync(true);
}

void MainApp::render() {
  input::EditorInputSnapshot input_snapshot = collectInputSnapshot();
  if (input_snapshot.key_escape_pressed &&
      !input_snapshot.wants_capture_keyboard) {
    if (!m_editor.cancelActiveOperation()) {
      close();
    }
  }

  m_editor.update(delta, input_snapshot);
  m_editor.render(resolution);
}

void MainApp::buildImGui() {
  m_editor.buildImGui(resolution, delta, frames);
}

void MainApp::keyCallback(Key parKey, Action parAction, Modifier) {
  static_cast<void>(parKey);
  static_cast<void>(parAction);
}

void MainApp::clickCallback(Button parButton, Action parAction, Modifier) {
  static_cast<void>(parButton);
  static_cast<void>(parAction);
}

void MainApp::moveCallback(const glm::vec2& parMovement, bool parLeftButton,
                           bool parRightButton, bool parMiddleButton) {
  static_cast<void>(parMovement);
  static_cast<void>(parLeftButton);
  static_cast<void>(parRightButton);
  static_cast<void>(parMiddleButton);
}

void MainApp::scrollCallback(float parXAmount, float parYAmount) {
  static_cast<void>(parXAmount);
  m_scroll_y += parYAmount;
}

input::EditorInputSnapshot MainApp::collectInputSnapshot() {
  double cursor_x = 0.0;
  double cursor_y = 0.0;
  glfwGetCursorPos(window, &cursor_x, &cursor_y);

  int window_width = 1;
  int window_height = 1;
  glfwGetWindowSize(window, &window_width, &window_height);

  int framebuffer_width = 1;
  int framebuffer_height = 1;
  glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);

  const glm::vec2 ui_size(std::max(window_width, 1), std::max(window_height, 1));
  const glm::vec2 framebuffer_size(std::max(framebuffer_width, 1),
                                   std::max(framebuffer_height, 1));
  const glm::vec2 ui_cursor(static_cast<float>(cursor_x),
                            static_cast<float>(cursor_y));
  const glm::vec2 ui_to_framebuffer = framebuffer_size / ui_size;

  const bool left_down = isMouseButtonPressed(Button::LEFT);
  const bool right_down = isMouseButtonPressed(Button::RIGHT);
  const bool middle_down = isMouseButtonPressed(Button::MIDDLE);
  const bool escape_down = isKeyPressed(Key::ESC);
  const bool delete_down =
      isKeyPressed(Key::DELETE) || isKeyPressed(Key::BACKSPACE);
  const bool tab_down = isKeyPressed(Key::TAB);

  const ImGuiIO& io = ImGui::GetIO();
  input::EditorInputSnapshot snapshot;
  snapshot.ui_cursor = ui_cursor;
  snapshot.ui_delta = ui_cursor - m_last_ui_cursor;
  snapshot.framebuffer_cursor = ui_cursor * ui_to_framebuffer;
  snapshot.framebuffer_size = framebuffer_size;
  snapshot.ui_to_framebuffer_scale = ui_to_framebuffer;
  snapshot.left_mouse_down = left_down;
  snapshot.right_mouse_down = right_down;
  snapshot.middle_mouse_down = middle_down;
  snapshot.left_mouse_pressed = left_down && !m_last_left_mouse_down;
  snapshot.right_mouse_pressed = right_down && !m_last_right_mouse_down;
  snapshot.middle_mouse_pressed = middle_down && !m_last_middle_mouse_down;
  snapshot.left_mouse_released = !left_down && m_last_left_mouse_down;
  snapshot.right_mouse_released = !right_down && m_last_right_mouse_down;
  snapshot.middle_mouse_released = !middle_down && m_last_middle_mouse_down;
  snapshot.key_w_down = isKeyPressed(Key::W);
  snapshot.key_a_down = isKeyPressed(Key::A);
  snapshot.key_s_down = isKeyPressed(Key::S);
  snapshot.key_d_down = isKeyPressed(Key::D);
  snapshot.key_space_down = isKeyPressed(Key::SPACE);
  snapshot.key_shift_down =
      isKeyPressed(Key::LEFT_SHIFT) || isKeyPressed(Key::RIGHT_SHIFT);
  snapshot.key_escape_pressed = escape_down && !m_last_escape_down;
  snapshot.key_delete_pressed = delete_down && !m_last_delete_down;
  snapshot.key_tab_pressed = tab_down && !m_last_tab_down;
  snapshot.wants_capture_mouse = io.WantCaptureMouse;
  snapshot.wants_capture_keyboard = io.WantCaptureKeyboard;
  snapshot.viewport_hovered = isCursorInside(ui_cursor, ui_size);
  snapshot.scroll_y = std::exchange(m_scroll_y, 0.0f);

  m_last_ui_cursor = ui_cursor;
  m_last_left_mouse_down = left_down;
  m_last_right_mouse_down = right_down;
  m_last_middle_mouse_down = middle_down;
  m_last_escape_down = escape_down;
  m_last_delete_down = delete_down;
  m_last_tab_down = tab_down;
  return snapshot;
}

bool MainApp::isKeyPressed(Key parKey) const {
  return glfwGetKey(window, static_cast<int>(parKey)) == GLFW_PRESS;
}

bool MainApp::isMouseButtonPressed(Button parButton) const {
  return glfwGetMouseButton(window, static_cast<int>(parButton)) == GLFW_PRESS;
}

}  // namespace kage::app
