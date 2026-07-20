#include "check_helpers.hpp"

#include "film/film_exporter.hpp"
#include "film/timeline_edit_service.hpp"
#include "render/film_framebuffer.hpp"
#include "render/viewport_rect.hpp"

#include <framework/app.hpp>

#include <glad/gl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

namespace {

constexpr int WIDTH = 256;
constexpr int HEIGHT = 144;

struct RenderProbe final {
  glm::vec3 hdr;
  std::array<int, 3> expected;
  int samples = 1;
  const char* artifact_name = nullptr;
};

const std::array<RenderProbe, 2> PROBES{{
    {{0.25f, 0.5f, 1.0f}, {123, 155, 186}, 1, "render-smoke-material.png"},
    {{1.0f, 0.25f, 0.1f}, {186, 123, 86}, 4, "render-smoke-final.png"},
}};

[[nodiscard]] kage::film::MovieTimeline makeOneFrameMovie() {
  using namespace kage::film;
  MovieTimeline timeline;
  timeline.name = "CI render smoke";
  TimelineEditService edits(timeline);
  CapturedEntityBaseState base;
  base.camera = CapturedCameraState{45.0f, 0.1f, 100.0f};
  const auto sequence = edits.createSequence("Camera", {TimelineTargetKind::Camera, {100}}, base);
  MovementClip movement;
  movement.start_mode = MovementStartMode::ExplicitPosition;
  movement.explicit_start = kage::math::Transform{};
  movement.end.translation = {0.0f, 0.0f, -1.0f};
  static_cast<void>(edits.appendClipToLane(*sequence, 1, movement));
  static_cast<void>(edits.placeSequence(*sequence, 0));
  return timeline;
}

class RenderSmokeApp final : public App {
public:
  RenderSmokeApp() : App(WIDTH, HEIGHT) {
    imguiEnabled = false;
    setVSync(false);
    glDisable(GL_FRAMEBUFFER_SRGB);
    std::filesystem::create_directories("test-artifacts");
    std::error_code error;
    std::filesystem::remove_all("test-artifacts/export-frames", error);
    std::filesystem::remove("test-artifacts/render-smoke.mp4", error);
    std::string export_error;
    if (!m_export.start(m_timeline, "test-artifacts/export-frames",
                        "test-artifacts/render-smoke.mp4", kage::film::findFfmpegExecutable(),
                        export_error)) {
      m_passed = false;
      m_error = export_error;
    }
  }

  [[nodiscard]] bool passed() const {
    return m_passed;
  }
  [[nodiscard]] const std::string& error() const {
    return m_error;
  }

protected:
  void render() override {
    if (frames >= PROBES.size()) {
      m_export.advance(m_timeline, [](int, int, int, unsigned int) {
        glClearColor(0.2f, 0.1f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      });
      if (m_export.getState() != kage::film::FinalRenderState::Complete ||
          !std::filesystem::is_regular_file("test-artifacts/render-smoke.mp4") ||
          !std::filesystem::is_regular_file("test-artifacts/export-frames/manifest.json")) {
        m_passed = false;
        if (m_error.empty()) {
          m_error = m_export.getError().empty() ? "one-frame production export failed"
                                                : m_export.getError();
        }
      }
      close();
      return;
    }
    const RenderProbe& probe = PROBES[frames];

    m_framebuffer.begin({{0, 0}, {WIDTH, HEIGHT}}, 0, probe.samples);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(probe.hdr.r, probe.hdr.g, probe.hdr.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    m_framebuffer.present();
    glFinish();

    std::vector<unsigned char> pixels(static_cast<std::size_t>(WIDTH * HEIGHT * 3));
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    std::size_t changed_pixels = 0;
    std::size_t total_error = 0;
    int max_error = 0;
    for (std::size_t offset = 0; offset < pixels.size(); offset += 3) {
      bool changed = false;
      for (std::size_t channel = 0; channel < 3; ++channel) {
        const int error =
            std::abs(static_cast<int>(pixels[offset + channel]) - probe.expected[channel]);
        changed |= error > 8;
        total_error += static_cast<std::size_t>(error);
        max_error = std::max(max_error, error);
      }
      changed_pixels += changed ? 1u : 0u;
    }
    const double mean_error = static_cast<double>(total_error) / static_cast<double>(pixels.size());
    const double changed_ratio =
        static_cast<double>(changed_pixels) / static_cast<double>(WIDTH * HEIGHT);
    if (max_error > 16 || mean_error > 2.0 || changed_ratio > 0.005 ||
        glGetError() != GL_NO_ERROR) {
      m_passed = false;
      m_error = "render probe failed: max_error=" + std::to_string(max_error) +
                ", mean_error=" + std::to_string(mean_error) +
                ", changed=" + std::to_string(changed_ratio);
    }
    static_cast<void>(
        takeScreenshot(std::filesystem::path("test-artifacts") / probe.artifact_name));
  }

private:
  kage::render::FilmFramebuffer m_framebuffer;
  kage::film::MovieTimeline m_timeline = makeOneFrameMovie();
  kage::film::FinalRenderJob m_export;
  bool m_passed = true;
  std::string m_error;
};

} // namespace

int main() {
  try {
    RenderSmokeApp app;
    app.run();
    return app.passed() ? 0 : kage::test::fail(app.error());
  } catch (const std::exception& error) {
    return kage::test::fail(error.what());
  }
}
