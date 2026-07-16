#include "editor/master_timeline_view.hpp"

#include "editor/movie_editor_controller.hpp"
#include "editor/movie_imgui_scope.hpp"
#include "editor/timeline_view_helpers.hpp"
#include "film/timeline_edit_service.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace kage::editor {

void drawMasterTimeline(engine::EngineCore& parEngine,
                        editor::EditorSession& parSession,
                        bool parFitToMovie, int parZoomDirection,
                        std::string& parError) {
  constexpr float ROW_HEIGHT = 44.0f;
  struct TargetRow final {
    film::TimelineTarget target;
    std::vector<const film::TargetSequence*> sequences;
  };
  struct InstanceGesture final {
    film::SequenceInstanceId instance_id = 0;
    film::FilmFrame start_frame = 0;
    float mouse_x = 0.0f;
  };
  static InstanceGesture gesture;

  film::MovieTimeline& timeline = parEngine.getMovieTimeline();
  const film::FilmFrame duration = timeline.durationFrames();

  std::vector<TargetRow> rows;
  for (const film::TargetSequence& sequence : timeline.sequences) {
    const auto row = std::find_if(rows.begin(), rows.end(),
                                  [&](const TargetRow& value) {
                                    return value.target == sequence.target;
                                  });
    if (row == rows.end()) {
      rows.push_back({sequence.target, {&sequence}});
    } else {
      row->sequences.push_back(&sequence);
    }
  }
  const float canvas_height = std::max(1.0f, ImGui::GetContentRegionAvail().y);
  ImGui::BeginChild("MasterTimelineCanvas", ImVec2(0.0f, canvas_height), true,
                    ImGuiWindowFlags_HorizontalScrollbar);
  const TimelineCanvas canvas = prepareTimelineCanvas(
      parSession, canvas_height, rows.size(), ROW_HEIGHT, duration,
      parFitToMovie, parZoomDirection);
  if (rows.empty()) {
    ImGui::EndChild();
    return;
  }
  const auto& [origin, label_width, pixels_per_frame, content_width,
               content_height, draw_list] = canvas;
  drawTimelineRuler(*draw_list, origin, label_width, pixels_per_frame,
                    content_width);
  static_cast<void>(scrubTimelineRuler("##MasterTimelineRuler", origin,
                                       label_width, content_width,
                                       pixels_per_frame, parSession, duration,
                                       parEngine.getFilmPlayback()));
  std::optional<film::TargetSequenceId> sequence_to_place;

  for (std::size_t row_index = 0; row_index < rows.size(); ++row_index) {
    const TargetRow& row = rows[row_index];
    const float y = origin.y + timelineRulerHeight() +
                    static_cast<float>(row_index) * ROW_HEIGHT;
    draw_list->AddRectFilled(ImVec2(origin.x, y),
                             ImVec2(origin.x + content_width, y + ROW_HEIGHT - 1.0f),
                             IM_COL32(30, 36, 39, 255));
    draw_list->AddText(ImVec2(origin.x + 6.0f, y + 7.0f),
                      IM_COL32(205, 210, 212, 255),
                      movieTargetLabel(parEngine, row.target).c_str());
    ImGui::SetCursorScreenPos(
        ImVec2(origin.x + label_width - 32.0f, y + 9.0f));
    pushMovieWidgetId(MovieWidgetIdKind::TimelineTargetRow,
                      static_cast<std::uint64_t>(row_index));
    if (ImGui::SmallButton("+")) {
      ImGui::OpenPopup("PlaceSequence");
    }
    if (ImGui::BeginPopup("PlaceSequence")) {
      for (const film::TargetSequence* sequence : row.sequences) {
        const bool placeable = sequence->durationFrames() > 0;
        {
          MovieDisabledScope disabled(!placeable);
          const std::string label = sequence->name + " (" +
              std::to_string(sequence->durationFrames()) + " frames)";
          pushMovieWidgetId(MovieWidgetIdKind::TargetSequence, sequence->id);
          if (ImGui::Selectable(label.c_str())) {
            sequence_to_place = sequence->id;
          }
          ImGui::PopID();
        }
      }
      ImGui::EndPopup();
    }
    ImGui::PopID();

    for (const film::SequenceInstance& instance : timeline.instances) {
      const film::TargetSequence* sequence = timeline.findSequence(instance.sequence_id);
      if (sequence == nullptr || sequence->target != row.target) {
        continue;
      }
      film::FilmFrame start = instance.start_frame;
      if (gesture.instance_id == instance.id && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const film::FilmFrame delta = timelineFrameDelta(ImGui::GetIO().MousePos.x, gesture.mouse_x,
            pixels_per_frame);
        start = std::clamp(gesture.start_frame + delta, 0,
                           film::MAX_FILM_FRAMES - sequence->durationFrames());
      }
      const ImVec2 bar_min(canvas.frameX(start), y + 7.0f);
      const ImVec2 bar_max(canvas.frameX(start + sequence->durationFrames()),
                           y + 28.0f);
      const bool selected = parSession.movie_selection.instance_id == instance.id;
      const bool camera_warning = [&] {
        if (sequence->target.kind != film::TimelineTargetKind::Camera) {
          return false;
        }
        const film::FilmFrame end =
            instance.start_frame + sequence->durationFrames();
        return std::any_of(
            timeline.instances.begin(), timeline.instances.end(),
            [&](const film::SequenceInstance& other) {
              if (other.id == instance.id) {
                return false;
              }
              const film::TargetSequence* other_sequence =
                  timeline.findSequence(other.sequence_id);
              if (other_sequence == nullptr ||
                  other_sequence->target.kind !=
                      film::TimelineTargetKind::Camera) {
                return false;
              }
              const film::FilmFrame other_end =
                  other.start_frame + other_sequence->durationFrames();
              return instance.start_frame < other_end && end > other.start_frame;
            });
      }();
      const ImU32 color = camera_warning ? IM_COL32(186, 112, 58, 255)
          : selected ? IM_COL32(100, 166, 221, 255) : IM_COL32(65, 122, 171, 255);
      draw_list->AddRectFilled(bar_min, bar_max, color, 3.0f);
      draw_list->AddRect(bar_min, bar_max, IM_COL32(220, 228, 232, 255),
                         3.0f, 0, 1.5f);
      draw_list->AddLine(ImVec2(bar_min.x + 3.0f, bar_min.y),
                         ImVec2(bar_min.x + 3.0f, bar_max.y),
                         IM_COL32(220, 228, 232, 255), 1.5f);
      draw_list->AddLine(ImVec2(bar_max.x - 3.0f, bar_min.y),
                         ImVec2(bar_max.x - 3.0f, bar_max.y),
                         IM_COL32(220, 228, 232, 255), 1.5f);
      draw_list->AddText(ImVec2(bar_min.x + 4.0f, bar_min.y + 3.0f),
                        IM_COL32(245, 248, 250, 255), sequence->name.c_str());
      pushMovieWidgetId(MovieWidgetIdKind::SequenceInstance, instance.id);
      ImGui::SetCursorScreenPos(bar_min);
      ImGui::InvisibleButton("##MoveInstance",
                             ImVec2(std::max(1.0f, bar_max.x - bar_min.x),
                                    bar_max.y - bar_min.y));
      const bool left_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
      const bool double_clicked =
          left_clicked && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
      if (double_clicked) {
        parEngine.clearFilmPreviewState();
        const film::SequenceInstance* selected_instance =
            timeline.findInstance(instance.id);
        const film::TargetSequence* selected_sequence =
            selected_instance == nullptr
                ? nullptr
                : timeline.findSequence(selected_instance->sequence_id);
        if (selected_sequence != nullptr) {
          selectMovieSequence(parSession, *selected_sequence);
          parSession.movie_selection.instance_id = instance.id;
        }
      } else if (left_clicked) {
        parEngine.clearFilmPreviewState();
        selectMovieInstance(parSession, instance.id);
      }
      if (ImGui::IsItemActivated() && !double_clicked) {
        gesture = {instance.id, instance.start_frame, ImGui::GetIO().MousePos.x};
      }
      ImGui::PopID();
    }
  }
  const float playhead_x = canvas.frameX(parSession.authoring_cursor_frame);
  drawTimelinePlayhead(*draw_list, playhead_x, origin.y,
                        origin.y + content_height);
  static_cast<void>(dragTimelinePlayhead(
      "##MasterTimelinePlayhead", origin, label_width, content_height,
      pixels_per_frame, parSession, parEngine.getFilmPlayback(), duration));
  ImGui::SetCursorScreenPos(origin);
  ImGui::Dummy(ImVec2(content_width, content_height));
  ImGui::EndChild();

  const film::FilmFrame authoring_cursor = parSession.authoring_cursor_frame;
  if (sequence_to_place.has_value()) {
    film::TimelineEditService edits(timeline);
    const auto result = edits.placeSequence(*sequence_to_place,
                                            authoring_cursor);
    if (result.has_value()) {
      parEngine.markProjectDirty();
      selectMovieInstance(parSession, *result);
      parError.clear();
      return;
    }
    parError = result.error();
  }
  if (gesture.instance_id != 0 && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    const film::SequenceInstance* instance =
        timeline.findInstance(gesture.instance_id);
    if (instance != nullptr) {
      const film::TargetSequence* sequence = timeline.findSequence(instance->sequence_id);
      if (sequence != nullptr) {
        const film::FilmFrame delta = timelineFrameDelta(ImGui::GetIO().MousePos.x, gesture.mouse_x,
            pixels_per_frame);
        const film::FilmFrame start = std::clamp(
            gesture.start_frame + delta, 0,
            film::MAX_FILM_FRAMES - sequence->durationFrames());
        if (start != gesture.start_frame) {
          film::TimelineEditService edits(timeline);
          const auto result = edits.moveInstance(gesture.instance_id, start);
          if (result.has_value()) {
            parEngine.markProjectDirty();
            parError.clear();
            gesture = {};
            return;
          } else {
            parError = result.error();
          }
        }
      }
    }
    gesture = {};
  }
}

}  // namespace kage::editor
