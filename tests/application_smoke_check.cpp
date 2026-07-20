#include "check_helpers.hpp"

#include "editor/world_editor.hpp"
#include "engine/engine_core.hpp"
#include "input/input_events.hpp"

#include <framework/app.hpp>

#include <glad/gl.h>

#include <exception>

namespace {

class ApplicationSmokeApp final : public App {
public:
  ApplicationSmokeApp() : App(640, 360), m_editor(m_engine) {
    setVSync(false);
  }

  [[nodiscard]] bool passed() const {
    return m_passed && m_imgui_built;
  }

protected:
  void render() override {
    kage::input::EditorInputSnapshot input;
    input.framebuffer_size = resolution;
    input.ui_to_framebuffer_scale = {1.0f, 1.0f};
    m_editor.update(1.0f / 30.0f, input);
    m_editor.render(resolution);

    if (glGetError() != GL_NO_ERROR) {
      m_passed = false;
      close();
      return;
    }
    if (frames >= 2) {
      close();
    }
  }

  void buildImGui() override {
    m_editor.buildImGui(1.0f / 30.0f);
    m_imgui_built = true;
  }

private:
  kage::engine::EngineCore m_engine;
  kage::editor::WorldEditor m_editor;
  bool m_passed = true;
  bool m_imgui_built = false;
};

} // namespace

int main() {
  try {
    ApplicationSmokeApp app;
    app.run();
    return app.passed() ? 0
                        : kage::test::fail("application startup, rendering, or ImGui regression");
  } catch (const std::exception& error) {
    return kage::test::fail(error.what());
  }
}
