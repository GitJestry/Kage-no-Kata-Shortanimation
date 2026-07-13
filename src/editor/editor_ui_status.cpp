#include "editor/editor_ui.hpp"

#include "editor/ui_layout.hpp"

#include <imgui.h>

namespace {

constexpr float MILLISECONDS_PER_SECOND = 1000.0f;
constexpr float DIAGNOSTICS_WIDTH = 260.0f;
constexpr float STATUS_HEIGHT = kage::editor::UI_STATUS_HEIGHT;

}  // namespace

namespace kage::editor {

void EditorUi::drawRuntimeDiagnostics(engine::EngineCore& parEngine,
                                      const glm::vec2& parViewportSize,
                                      float parDeltaSeconds,
                                      unsigned int parFrameCount) {
  static_cast<void>(parViewportSize);
  const UiWorkArea area = getUiWorkArea();
  const ImVec2 size(DIAGNOSTICS_WIDTH, 184.0f);
  const ImVec2 position = clampPanelPosition(
      area,
      ImVec2(area.position.x + area.size.x - DIAGNOSTICS_WIDTH - 16.0f,
             area.position.y + area.size.y - STATUS_HEIGHT - size.y - 20.0f),
      size, true);
  ImGui::SetNextWindowPos(position, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
  const bool diagnostics_open =
      ImGui::Begin("Runtime Diagnostics", &m_diagnostics_visible,
                   ImGuiWindowFlags_NoCollapse);
  clampCurrentPanel("Runtime Diagnostics", true);
  if (!diagnostics_open) {
    trackCurrentPanel();
    ImGui::End();
    return;
  }

  ImGui::Text("Frame time  %.3f ms",
              parDeltaSeconds * MILLISECONDS_PER_SECOND);
  const render::PerformanceSnapshot& performance =
      parEngine.getPerformanceSnapshot();
  ImGui::Text("CPU avg/p95 %.2f / %.2f ms", performance.cpu_average_ms,
              performance.cpu_p95_ms);
  ImGui::Text("GPU avg/p95 %.2f / %.2f ms", performance.gpu_average_ms,
              performance.gpu_p95_ms);
  ImGui::Text("Asset/GPU   %.2f / %.2f ms", performance.asset_load_ms,
              performance.gpu_upload_ms);
  ImGui::Text("Anim/Render %.2f / %.2f ms",
              performance.animation_update_ms, performance.render_ms);
  ImGui::Text("Shadow/Bind/Draw %.2f / %.2f / %.2f ms%s",
              performance.shadow_render_ms, performance.frame_binding_ms,
              performance.material_submission_ms,
              performance.shadows_reused ? " (shadows cached)" : "");
  ImGui::Text("Draws       %zu", performance.draw_calls);
  ImGui::Text("Triangles   %zu", performance.submitted_triangles);
  ImGui::Text("Visible     %zu (%zu culled)",
              performance.visible_entities, performance.culled_entities);
  ImGui::Text("Texture GPU %.1f MiB | Stream %zu",
              static_cast<double>(performance.estimated_texture_bytes) /
                  (1024.0 * 1024.0),
              performance.streaming_work_items);
  static_cast<void>(parFrameCount);
  trackCurrentPanel();
  ImGui::End();
}

void EditorUi::drawStatusStrip(
    engine::EngineCore& parEngine,
    const PlacementController& parPlacementController,
    const GizmoController& parGizmoController,
    const glm::vec2& parViewportSize) {
  static_cast<void>(parViewportSize);
  static_cast<void>(parEngine);
  static_cast<void>(parPlacementController);
  static_cast<void>(parGizmoController);
  const UiWorkArea area = getUiWorkArea();
  ImGui::SetNextWindowPos(
      ImVec2(area.position.x, area.position.y + area.size.y - STATUS_HEIGHT),
      ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(area.size.x, STATUS_HEIGHT),
                           ImGuiCond_Always);
  ImGui::Begin("EditorStatusStrip", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoSavedSettings);
  trackCurrentPanel();
  const char* label = m_diagnostics_visible ? "Close Diagnostics"
                                            : "Diagnostics";
  const float width = 132.0f;
  ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),
                               ImGui::GetWindowWidth() - width - 8.0f));
  if (ImGui::Button(label, ImVec2(width, 0.0f))) {
    m_diagnostics_visible = !m_diagnostics_visible;
  }
  ImGui::End();
}

}  // namespace kage::editor
