#include "editor/movie_editor_panel.hpp"

#include "editor/master_timeline_view.hpp"
#include "editor/movie_editor_controller.hpp"
#include "editor/movie_imgui_scope.hpp"
#include "editor/movie_inspector.hpp"
#include "editor/movie_target_list.hpp"
#include "editor/sequence_timeline_view.hpp"
#include "editor/ui_layout.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace kage::editor {
namespace {

[[nodiscard]] UiPanelRect beginMovieRegion(const char* parName,
                                           const ImVec2& parPosition,
                                           const ImVec2& parSize) {
  ImGui::SetNextWindowPos(parPosition, ImGuiCond_Always);
  ImGui::SetNextWindowSize(parSize, ImGuiCond_Always);
  ImGui::Begin(parName, nullptr,
               ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
  return getCurrentPanelRect();
}

void drawFilmPreviewBars(const MovieEditorLayout& parLayout) {
  const UiPanelRect& container = parLayout.viewport;
  const UiPanelRect& frame = parLayout.film_preview;
  ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
  const auto fill = [draw_list](const glm::vec2& parMin,
                                const glm::vec2& parMax) {
    if (parMax.x > parMin.x && parMax.y > parMin.y) {
      draw_list->AddRectFilled(ImVec2(parMin.x, parMin.y),
                               ImVec2(parMax.x, parMax.y),
                               IM_COL32(0, 0, 0, 255));
    }
  };
  fill(container.min, {container.max.x, frame.min.y});
  fill({container.min.x, frame.max.y}, container.max);
  fill({container.min.x, frame.min.y},
       {frame.min.x, frame.max.y});
  fill({frame.max.x, frame.min.y},
       {container.max.x, frame.max.y});
}

void drawTimelineArea(engine::EngineCore& parEngine, editor::EditorSession& parSession,
                      bool parFitTimeline, int parZoomDirection,
                      std::string& parError) {
  if (parSession.movie_selection.target.has_value()) {
    drawSequenceTimeline(parEngine, parSession, parFitTimeline,
                         parZoomDirection, parError);
  } else {
    drawMasterTimeline(parEngine, parSession, parFitTimeline,
                       parZoomDirection, parError);
  }
}

void drawTimelineWindow(engine::EngineCore& parEngine,
                        EditorSession& parSession, float parMinHeight,
                        float parMaxHeight, std::string& parError) {
  constexpr float SPLITTER_HIT_HEIGHT = 12.0f;
  const ImVec2 content_cursor = ImGui::GetCursorScreenPos();
  const ImVec2 window_position = ImGui::GetWindowPos();
  const float window_width = ImGui::GetWindowWidth();
  ImGui::SetCursorScreenPos(window_position);
  ImGui::PushClipRect(window_position,
                      ImVec2(window_position.x + window_width,
                             window_position.y + ImGui::GetWindowHeight()),
                      false);
  ImGui::InvisibleButton("##TimelineHeightSplitter",
                         ImVec2(window_width, SPLITTER_HIT_HEIGHT));
  ImGui::PopClipRect();
  const bool splitter_hovered = ImGui::IsItemHovered();
  const bool splitter_active = ImGui::IsItemActive();
  if (splitter_hovered || splitter_active) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
  }
  ImGui::GetWindowDrawList()->AddLine(
      ImVec2(window_position.x, window_position.y + 1.0f),
      ImVec2(window_position.x + window_width, window_position.y + 1.0f),
      splitter_hovered || splitter_active ? IM_COL32(100, 166, 221, 255)
                                           : IM_COL32(76, 86, 91, 255),
      splitter_active ? 2.0f : 1.0f);
  if (splitter_active &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    parSession.film_editor_height = std::clamp(
        parSession.film_editor_height - ImGui::GetIO().MouseDelta.y,
        parMinHeight, parMaxHeight);
  }
  ImGui::SetCursorScreenPos(content_cursor);

  film::MovieTimeline& timeline = parEngine.getMovieTimeline();
  film::FilmPlayback& playback = parEngine.getFilmPlayback();
  updateMoviePreviewContext(parSession, timeline, playback);
  const film::FilmFrame movie_duration = timeline.durationFrames();
  const film::FilmFrame duration = moviePreviewDuration(parSession, timeline);
  if (duration <= 0) {
    resetMoviePreview(parEngine, parSession);
    playback.playhead_frame = 0.0;
  }
  {
    MovieDisabledScope disabled(duration <= 0);
    if (ImGui::Button(playback.playing ? "Pause" : "Play")) {
      static_cast<void>(toggleMoviePlayback(parSession, timeline, playback));
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Stop")) {
    resetMoviePreview(parEngine, parSession);
  }

  const film::TargetSequence* selected_sequence =
      timeline.findSequence(parSession.movie_selection.sequence_id);
  const film::FilmFrame timeline_duration =
      parSession.movie_selection.target.has_value() && selected_sequence != nullptr
          ? selected_sequence->durationFrames()
          : movie_duration;
  const film::FilmFrame cursor = clampMovieAuthoringCursor(
      parSession.authoring_cursor_frame);
  ImGui::SameLine();
  ImGui::Text("%d f / %.2f s  |  %d f / %.2f s", cursor,
              static_cast<double>(cursor) / film::FILM_FPS, timeline_duration,
              static_cast<double>(timeline_duration) / film::FILM_FPS);
  ImGui::SameLine();
  const bool fit_timeline = ImGui::Button("Fit");
  ImGui::SameLine();
  const bool zoom_out = ImGui::SmallButton("-");
  ImGui::SameLine();
  const bool zoom_in = ImGui::SmallButton("+");
  ImGui::SameLine();
  ImGui::TextDisabled("%.0f%%",
                      parSession.target_sequence_pixels_per_frame / 12.0f *
                          100.0f);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Ctrl + scroll to zoom");
  }
  ImGui::Separator();
  drawTimelineArea(parEngine, parSession, fit_timeline,
                   zoom_in ? 1 : (zoom_out ? -1 : 0), parError);
}

}  // namespace

std::vector<UiPanelRect> drawMovieEditorPanel(
    engine::EngineCore& parEngine, EditorSession& parSession,
    const MovieEditorLayout& parLayout) {
  const MovieEditorLayout& layout = parLayout;
  const auto sizeOf = [](const UiPanelRect& rect) {
    return ImVec2(rect.max.x - rect.min.x, rect.max.y - rect.min.y);
  };
  const auto positionOf = [](const UiPanelRect& rect) {
    return ImVec2(rect.min.x, rect.min.y);
  };
  const ImVec2 left_size = sizeOf(layout.left_panel);
  const ImVec2 right_size = sizeOf(layout.right_panel);
  const ImVec2 timeline_size = sizeOf(layout.timeline);

  std::vector<UiPanelRect> panel_rects;
  panel_rects.reserve(3);
  static std::string editor_error;

  drawFilmPreviewBars(layout);

  panel_rects.push_back(beginMovieRegion(
      "Animation Targets###MovieAnimationTargets", positionOf(layout.left_panel),
      left_size));
  drawMovieTargetList(parEngine, parSession, editor_error);
  ImGui::End();

  const char* inspector_title = parSession.movie_selection.target.has_value()
                                    ? "Target Inspector###MovieInspector"
                                    : "Movie Inspector###MovieInspector";
  panel_rects.push_back(beginMovieRegion(inspector_title,
                                         positionOf(layout.right_panel),
                                         right_size));
  drawMovieInspector(parEngine, parSession, editor_error);
  ImGui::End();

  const char* timeline_title = parSession.movie_selection.target.has_value()
                                   ? "Target Sequence Timeline###MovieTimeline"
                                   : "Movie Timeline###MovieTimeline";
  panel_rects.push_back(
      beginMovieRegion(timeline_title, positionOf(layout.timeline), timeline_size));
  drawTimelineWindow(parEngine, parSession, layout.timeline_min_height,
                     layout.timeline_max_height, editor_error);
  ImGui::End();
  return panel_rects;
}

}  // namespace kage::editor
