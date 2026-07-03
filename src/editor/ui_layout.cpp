#include "editor/ui_layout.hpp"

#include <algorithm>

namespace kage::editor {

UiWorkArea getUiWorkArea() {
  if (const ImGuiViewport* viewport = ImGui::GetMainViewport();
      viewport != nullptr) {
    return {viewport->WorkPos, viewport->WorkSize};
  }

  const ImGuiIO& io = ImGui::GetIO();
  return {ImVec2(0.0f, 0.0f), io.DisplaySize};
}

ImVec2 clampPanelPosition(const UiWorkArea& parArea,
                          ImVec2 parDesiredPosition,
                          ImVec2 parPanelSize,
                          bool parKeepAboveStatusStrip) {
  const float min_x = parArea.position.x;
  const float min_y = parArea.position.y;
  const float max_x =
      std::max(min_x, parArea.position.x + parArea.size.x - parPanelSize.x);
  const float available_height =
      parArea.size.y - (parKeepAboveStatusStrip ? UI_STATUS_HEIGHT : 0.0f);
  const float max_y =
      std::max(min_y, parArea.position.y + available_height - parPanelSize.y);
  return ImVec2(std::clamp(parDesiredPosition.x, min_x, max_x),
                std::clamp(parDesiredPosition.y, min_y, max_y));
}

ImVec2 clampPanelSize(const UiWorkArea& parArea, ImVec2 parPanelSize,
                      bool parKeepAboveStatusStrip) {
  const float available_height =
      std::max(120.0f,
               parArea.size.y -
                   (parKeepAboveStatusStrip ? UI_STATUS_HEIGHT : 0.0f));
  return ImVec2(std::min(parPanelSize.x, parArea.size.x),
                std::min(parPanelSize.y, available_height));
}

UiPanelRect getCurrentPanelRect() {
  const ImVec2 min = ImGui::GetWindowPos();
  const ImVec2 size = ImGui::GetWindowSize();
  return {glm::vec2(min.x, min.y),
          glm::vec2(min.x + size.x, min.y + size.y)};
}

void pushDestructiveButtonStyle() {
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.58f, 0.10f, 0.09f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(0.76f, 0.16f, 0.13f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        ImVec4(0.92f, 0.22f, 0.17f, 1.0f));
}

void popDestructiveButtonStyle() {
  ImGui::PopStyleColor(3);
}

}  // namespace kage::editor
