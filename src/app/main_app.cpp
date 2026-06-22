#include "app/main_app.hpp"

#include <glad/gl.h>
#include <imgui.h>

namespace {

constexpr unsigned int WINDOW_WIDTH = 1280;
constexpr unsigned int WINDOW_HEIGHT = 720;
constexpr float CLEAR_COLOR_RED = 0.025f;
constexpr float CLEAR_COLOR_GREEN = 0.035f;
constexpr float CLEAR_COLOR_BLUE = 0.055f;
constexpr float CLEAR_COLOR_ALPHA = 1.0f;
constexpr float MILLISECONDS_PER_SECOND = 1000.0f;

}  // namespace

namespace kage::app {

MainApp::MainApp() : App(WINDOW_WIDTH, WINDOW_HEIGHT) {
  setTitle("Kage no Kata - The Final Cut");
  setVSync(true);
}

void MainApp::render() {
  glClearColor(CLEAR_COLOR_RED, CLEAR_COLOR_GREEN, CLEAR_COLOR_BLUE,
               CLEAR_COLOR_ALPHA);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void MainApp::buildImGui() {
  ImGui::Begin("Runtime diagnostics");
  ImGui::Text("OpenGL 4.1 Core Profile");
  ImGui::Separator();
  ImGui::Text("Framebuffer: %.0f x %.0f", resolution.x, resolution.y);
  ImGui::Text("Frame time: %.3f ms", delta * MILLISECONDS_PER_SECOND);
  ImGui::Text("Frame: %u", frames);
  ImGui::TextUnformatted("Press Escape to close.");
  ImGui::End();
}

void MainApp::keyCallback(Key parKey, Action parAction, Modifier) {
  if (parKey == Key::ESC && parAction == Action::PRESS) {
    close();
  }
}

}  // namespace kage::app
