#pragma once

#include "editor/movie_editor_layout.hpp"
#include "editor/ui_panel_rect.hpp"

#include <imgui.h>

namespace kage::editor {

inline constexpr float UI_STATUS_HEIGHT = 34.0f;

struct UiWorkArea final {
  ImVec2 position{0.0f, 0.0f};
  ImVec2 size{1.0f, 1.0f};
};

[[nodiscard]] UiWorkArea getUiWorkArea();
[[nodiscard]] ImVec2 clampPanelPosition(const UiWorkArea& parArea,
                                        ImVec2 parDesiredPosition,
                                        ImVec2 parPanelSize,
                                        bool parKeepAboveStatusStrip);
[[nodiscard]] ImVec2 clampPanelSize(const UiWorkArea& parArea,
                                    ImVec2 parPanelSize,
                                    bool parKeepAboveStatusStrip);
[[nodiscard]] UiPanelRect getCurrentPanelRect();
void pushDestructiveButtonStyle();
void popDestructiveButtonStyle();

}  // namespace kage::editor
