#include "editor/editor_ui.hpp"

#include "editor/ui_layout.hpp"

#include <imgui.h>

namespace {

constexpr float MILLISECONDS_PER_SECOND = 1000.0f;
constexpr float DIAGNOSTICS_WIDTH = 260.0f;
constexpr float STATUS_HEIGHT = kage::editor::UI_STATUS_HEIGHT;

#ifndef KAGE_BUILD_TYPE
#ifdef NDEBUG
#define KAGE_BUILD_TYPE "Release"
#else
#define KAGE_BUILD_TYPE "Debug"
#endif
#endif

}  // namespace

namespace kage::editor {

void EditorUi::drawRuntimeDiagnostics(engine::EngineCore& parEngine,
                                      const glm::vec2& parViewportSize,
                                      float parDeltaSeconds,
                                      unsigned int parFrameCount) {
  static_cast<void>(parViewportSize);
  const UiWorkArea area = getUiWorkArea();
  const ImVec2 size(DIAGNOSTICS_WIDTH, 224.0f);
  const ImVec2 position = clampPanelPosition(
      area,
      ImVec2(area.position.x + area.size.x - DIAGNOSTICS_WIDTH - 16.0f,
             area.position.y + 16.0f),
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

  ImGui::Text("Build       %s %s %s", KAGE_BUILD_TYPE, __DATE__, __TIME__);
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
  ImGui::Text("Draws       %zu", performance.draw_calls);
  ImGui::Text("Triangles   %zu", performance.submitted_triangles);
  ImGui::Text("Visible     %zu (%zu culled)",
              performance.visible_entities, performance.culled_entities);
  ImGui::Text("Texture GPU %.1f MiB | Stream %zu",
              static_cast<double>(performance.estimated_texture_bytes) /
                  (1024.0 * 1024.0),
              performance.streaming_work_items);
  ImGui::Text("Frame       %u", parFrameCount);
  ImGui::Text("Scene       %zu", parEngine.getActiveSceneIndex() + 1);
  if (parEngine.getSelectedEntity().isValid()) {
    ImGui::Text("Selected    %u", parEngine.getSelectedEntity().value);
  } else {
    ImGui::TextUnformatted("Selected    None");
  }
  ImGui::Text("Fly speed   %.2f m/s",
              parEngine.getCameraSystem().getFlyMoveSpeed());
  ImGui::TextDisabled("Right drag look | WASD move | Space/Shift height");
  trackCurrentPanel();
  ImGui::End();
}

void EditorUi::drawStatusStrip(
    engine::EngineCore& parEngine,
    const PlacementController& parPlacementController,
    const GizmoController& parGizmoController,
    const glm::vec2& parViewportSize) {
  static_cast<void>(parViewportSize);
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
  ImGui::Text("Camera Fly");
  ImGui::SameLine();
  ImGui::TextDisabled("| Speed %.2f",
                      parEngine.getCameraSystem().getFlyMoveSpeed());
  ImGui::SameLine();
  const char* gizmo_mode = "Move";
  if (parGizmoController.getMode() == GizmoController::TransformMode::Scale) {
    gizmo_mode = "Scale";
  } else if (parGizmoController.getMode() ==
             GizmoController::TransformMode::Rotate) {
    gizmo_mode = "Rotate";
  }
  ImGui::TextDisabled("| Gizmo %s", gizmo_mode);
  ImGui::SameLine();
  ImGui::TextDisabled(
      "| Axis %s",
      parGizmoController.getAxisSpace() == GizmoController::AxisSpace::World
          ? "World"
          : "Local");
  ImGui::SameLine();
  ImGui::TextDisabled("| %s", parPlacementController.getStatusLabel());
  if (parPlacementController.isActive()) {
    ImGui::SameLine();
    ImGui::TextDisabled("| Esc cancels, left click places");
  }
  ImGui::SameLine();
  if (ImGui::SmallButton(m_panel_visible ? "Hide Editor" : "Show Editor")) {
    m_panel_visible = !m_panel_visible;
  }
  ImGui::SameLine();
  if (ImGui::SmallButton(m_inspector_visible ? "Hide Inspector"
                                             : "Show Inspector")) {
    m_inspector_visible = !m_inspector_visible;
  }
  ImGui::SameLine();
  if (ImGui::SmallButton(m_timeline_visible ? "Hide Timeline"
                                            : "Show Timeline")) {
    m_timeline_visible = !m_timeline_visible;
  }
  ImGui::SameLine();
  if (ImGui::SmallButton(m_diagnostics_visible ? "Hide Diagnostics"
                                               : "Show Diagnostics")) {
    m_diagnostics_visible = !m_diagnostics_visible;
  }
  ImGui::End();
}

}  // namespace kage::editor
