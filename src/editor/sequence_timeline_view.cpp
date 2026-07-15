#include "editor/sequence_timeline_view.hpp"

#include "editor/movie_editor_controller.hpp"
#include "editor/timeline_view_helpers.hpp"
#include "film/timeline_edit_service.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace kage::editor {
namespace {

enum class SequenceLaneKind {
  Movement,
  Animation,
  Property,
};

struct SequenceLane final {
  const char* label;
  SequenceLaneKind kind;
  film::PropertyKind property_kind = film::PropertyKind::CameraFov;
};

[[nodiscard]] std::vector<SequenceLane> lanesFor(
    const film::TargetSequence& parSequence) {
  using film::PropertyKind;
  using film::TimelineTargetKind;
  switch (parSequence.target.kind) {
    case TimelineTargetKind::RiggedEntity:
      return {{"Movement", SequenceLaneKind::Movement},
              {"Animation", SequenceLaneKind::Animation}};
    case TimelineTargetKind::Camera:
      return {{"Movement", SequenceLaneKind::Movement},
              {"Field of View", SequenceLaneKind::Property,
               PropertyKind::CameraFov}};
    case TimelineTargetKind::PointLight:
      return {{"Movement", SequenceLaneKind::Movement},
              {"Intensity", SequenceLaneKind::Property,
               PropertyKind::PointLightIntensity},
              {"Color", SequenceLaneKind::Property,
               PropertyKind::PointLightColor}};
    case TimelineTargetKind::Sun:
      return {{"Direction", SequenceLaneKind::Property,
               PropertyKind::SunDirection},
              {"Intensity", SequenceLaneKind::Property,
               PropertyKind::SunIntensity},
              {"Color", SequenceLaneKind::Property, PropertyKind::SunColor}};
  }
  return {};
}

[[nodiscard]] bool isInLane(const film::SequenceClip& parClip,
                            const SequenceLane& parLane) {
  if (parLane.kind == SequenceLaneKind::Movement) {
    return std::holds_alternative<film::MovementClip>(parClip.payload);
  }
  if (parLane.kind == SequenceLaneKind::Animation) {
    return std::holds_alternative<film::RigAnimationClip>(parClip.payload);
  }
  const auto* property = std::get_if<film::PropertyClip>(&parClip.payload);
  return property != nullptr && property->kind == parLane.property_kind;
}

[[nodiscard]] glm::vec4 capturedPropertyValue(
    const film::TargetSequence& parSequence, film::PropertyKind parKind) {
  if (const auto* entity =
          std::get_if<film::CapturedEntityBaseState>(&parSequence.captured_base)) {
    if (parKind == film::PropertyKind::CameraFov && entity->camera.has_value()) {
      return glm::vec4(entity->camera->vertical_fov_degrees);
    }
    if (parKind == film::PropertyKind::PointLightIntensity &&
        entity->point_light.has_value()) {
      return glm::vec4(entity->point_light->intensity);
    }
    if (parKind == film::PropertyKind::PointLightColor &&
        entity->point_light.has_value()) {
      return glm::vec4(entity->point_light->color, 1.0f);
    }
  } else if (const auto* sun =
                 std::get_if<film::CapturedSunBaseState>(&parSequence.captured_base)) {
    if (parKind == film::PropertyKind::SunDirection) {
      return glm::vec4(sun->direction_to_sun, 0.0f);
    }
    if (parKind == film::PropertyKind::SunIntensity) {
      return glm::vec4(sun->intensity);
    }
    if (parKind == film::PropertyKind::SunColor) {
      return glm::vec4(sun->color, 1.0f);
    }
  }
  return glm::vec4(0.0f);
}

[[nodiscard]] film::SequenceClipPayload defaultPayloadForLane(
    const engine::EngineCore& parEngine, const film::TargetSequence& parSequence,
    const SequenceLane& parLane) {
  if (parLane.kind == SequenceLaneKind::Movement) {
    film::MovementClip movement;
    if (const auto* entity =
            std::get_if<film::CapturedEntityBaseState>(&parSequence.captured_base)) {
      movement.end = entity->transform;
    }
    return movement;
  }
  if (parLane.kind == SequenceLaneKind::Animation) {
    film::RigAnimationClip animation;
    if (const assets::ModelAsset* asset = movieAnimationAsset(parEngine, parSequence);
        asset != nullptr && !asset->animation_clips.empty()) {
      animation.clip_id = asset->animation_clips.front().id;
    }
    return animation;
  }
  film::PropertyClip property;
  property.kind = parLane.property_kind;
  property.start_value = capturedPropertyValue(parSequence, property.kind);
  property.end_value = property.start_value;
  property.control_1 = glm::mix(property.start_value, property.end_value,
                                1.0f / 3.0f);
  property.control_2 = glm::mix(property.start_value, property.end_value,
                                2.0f / 3.0f);
  return property;
}

}  // namespace

void drawSequenceTimeline(engine::EngineCore& parEngine,
                          editor::EditorSession& parSession,
                          bool parFitToSequence, int parZoomDirection,
                          std::string& parError) {
  const film::TargetSequenceId sequence_id =
      parSession.movie_selection.sequence_id;
  film::TargetSequence* sequence = parEngine.getMovieTimeline().findSequence(
      sequence_id);
  if (sequence == nullptr) {
    const float canvas_height = std::max(1.0f, ImGui::GetContentRegionAvail().y);
    ImGui::BeginChild("TargetSequenceCanvas", ImVec2(0.0f, canvas_height), true);
    ImGui::EndChild();
    return;
  }
  constexpr float LABEL_WIDTH = 176.0f;
  constexpr float LANE_HEIGHT = 44.0f;
  constexpr float HANDLE_WIDTH = 12.0f;
  constexpr film::FilmFrame DEFAULT_CLIP_DURATION = 30;
  struct ClipGesture final {
    enum class Mode { None, Move, TrimStart, TrimEnd };
    film::SequenceClipId clip_id = 0;
    Mode mode = Mode::None;
    film::FilmFrame start_frame = 0;
    film::FilmFrame end_frame = 0;
    float mouse_x = 0.0f;
  };
  static ClipGesture gesture;

  const std::vector<SequenceLane> lanes = lanesFor(*sequence);
  const film::FilmFrame duration = sequence->durationFrames();
  const float canvas_height = std::max(1.0f, ImGui::GetContentRegionAvail().y);
  ImGui::BeginChild("TargetSequenceCanvas", ImVec2(0.0f, canvas_height), true,
                    ImGuiWindowFlags_HorizontalScrollbar);
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const float canvas_width = ImGui::GetContentRegionAvail().x;
  if (parFitToSequence) {
    setTimelinePixelsPerFrame(parSession,
                              timelineFitPixelsPerFrame(canvas_width,
                                                        LABEL_WIDTH, duration),
                              canvas_width, LABEL_WIDTH);
    ImGui::SetScrollX(0.0f);
  } else if (parZoomDirection != 0) {
    setTimelinePixelsPerFrame(
        parSession, parSession.target_sequence_pixels_per_frame *
                        (parZoomDirection > 0 ? 1.2f : 1.0f / 1.2f),
        canvas_width, LABEL_WIDTH);
  } else {
    setTimelinePixelsPerFrame(parSession,
                              parSession.target_sequence_pixels_per_frame,
                              canvas_width, LABEL_WIDTH);
  }
  static_cast<void>(updateTimelineZoom(parSession, origin, canvas_width,
                                        LABEL_WIDTH));
  const float pixels_per_frame = parSession.target_sequence_pixels_per_frame;
  const float content_width = std::max(
      canvas_width, LABEL_WIDTH +
                        static_cast<float>(film::MAX_FILM_FRAMES) *
                            pixels_per_frame);
  const float content_height = std::max(
      canvas_height - ImGui::GetStyle().WindowPadding.y * 2.0f,
      timelineRulerHeight() + static_cast<float>(lanes.size()) * LANE_HEIGHT);
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  drawTimelineRuler(*draw_list, origin, LABEL_WIDTH, pixels_per_frame,
                    content_width);
  static_cast<void>(scrubTimelineRuler("##TargetSequenceTimelineRuler", origin,
                                       LABEL_WIDTH, content_width,
                                       pixels_per_frame, parSession, duration,
                                       parEngine.getFilmPlayback()));
  const auto frame_x = [&](film::FilmFrame frame) {
    return timelineFrameX(origin, LABEL_WIDTH, frame, pixels_per_frame);
  };

  for (std::size_t lane_index = 0; lane_index < lanes.size(); ++lane_index) {
    const float y = origin.y + timelineRulerHeight() +
                    static_cast<float>(lane_index) * LANE_HEIGHT;
    draw_list->AddRectFilled(ImVec2(origin.x, y),
                             ImVec2(origin.x + content_width, y + LANE_HEIGHT - 1.0f),
                             IM_COL32(30, 36, 39, 255));
    draw_list->AddText(ImVec2(origin.x + 6.0f, y + 14.0f), IM_COL32(205, 210, 212, 255),
                      lanes[lane_index].label);
    pushMovieWidgetId(MovieWidgetIdKind::TimelineLane,
                      static_cast<std::uint64_t>(lane_index));
    if (lanes[lane_index].kind == SequenceLaneKind::Movement) {
      const bool showing_all =
          parSession.shown_movement_paths_sequence_id == sequence_id;
      ImGui::SetCursorScreenPos(
          ImVec2(origin.x + LABEL_WIDTH - 76.0f, y + 9.0f));
      if (ImGui::SmallButton(showing_all ? "Hide" : "Show")) {
        parSession.shown_movement_paths_sequence_id =
            showing_all ? 0 : sequence_id;
      }
      ImGui::SameLine();
    } else {
      ImGui::SetCursorScreenPos(
          ImVec2(origin.x + LABEL_WIDTH - 28.0f, y + 9.0f));
    }
    if (ImGui::SmallButton("Add")) {
      const film::SequenceClipPayload payload =
          defaultPayloadForLane(parEngine, *sequence, lanes[lane_index]);
      film::TimelineEditService edits(parEngine.getMovieTimeline());
      const auto added = edits.appendClipToLane(
          sequence_id, DEFAULT_CLIP_DURATION, payload);
      if (added.has_value()) {
        parEngine.markProjectDirty();
        parSession.movie_selection.clip_id = *added;
        parSession.movie_selection.instance_id = 0;
        parError.clear();
        sequence = parEngine.getMovieTimeline().findSequence(sequence_id);
        if (sequence == nullptr) {
          parSession.movie_selection.sequence_id = 0;
        }
      } else {
        parError = added.error();
      }
      ImGui::PopID();
      ImGui::EndChild();
      return;
    }
    ImGui::PopID();
  }

  const auto previewRange = [&](const film::SequenceClip& clip) {
    film::FilmFrame start = clip.start_frame;
    film::FilmFrame end = clip.end_frame;
    if (gesture.clip_id != clip.id || gesture.mode == ClipGesture::Mode::None) {
      return std::pair{start, end};
    }
    const film::FilmFrame delta = timelineFrameDelta(ImGui::GetIO().MousePos.x, gesture.mouse_x,
          pixels_per_frame);
    if (gesture.mode == ClipGesture::Mode::Move) {
      start = std::clamp(gesture.start_frame + delta, 0,
                         film::MAX_FILM_FRAMES - (gesture.end_frame - gesture.start_frame));
      end = start + (gesture.end_frame - gesture.start_frame);
    } else if (gesture.mode == ClipGesture::Mode::TrimStart) {
      start = std::clamp(gesture.start_frame + delta, 0, gesture.end_frame - 1);
      end = gesture.end_frame;
    } else if (gesture.mode == ClipGesture::Mode::TrimEnd) {
      start = gesture.start_frame;
      end = std::clamp(gesture.end_frame + delta, gesture.start_frame + 1,
                       film::MAX_FILM_FRAMES);
    }
    return std::pair{start, end};
  };

  const auto selectClip = [&](film::SequenceClipId clip_id) {
    parSession.movie_selection.clip_id = clip_id;
    parSession.movie_selection.instance_id = 0;
  };
  const auto resolved_movements = film::resolveMovementSegments(*sequence);

  for (std::size_t lane_index = 0; lane_index < lanes.size(); ++lane_index) {
    const float y = origin.y + timelineRulerHeight() +
                    static_cast<float>(lane_index) * LANE_HEIGHT;
    if (lanes[lane_index].kind == SequenceLaneKind::Movement) {
      for (const film::ResolvedMovementSegment& movement : resolved_movements) {
        if (!movement.transition_before || !movement.transition_before->enabled) {
          continue;
        }
        const auto& transition = movement.transition_before->spline;
        const ImVec2 transition_min(frame_x(transition.start_frame), y + 29.0f);
        const ImVec2 transition_max(frame_x(transition.end_frame), y + 40.0f);
        draw_list->AddRectFilled(transition_min, transition_max,
                                 IM_COL32(132, 83, 190, 255), 2.0f);
        ImGui::SetCursorScreenPos(transition_min);
        pushMovieWidgetId(MovieWidgetIdKind::SequenceClip, movement.clip_id);
        if (ImGui::InvisibleButton("##Transition",
              ImVec2(std::max(1.0f, transition_max.x - transition_min.x),
                     transition_max.y - transition_min.y))) {
          selectClip(movement.clip_id);
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Transition before this movement clip");
        }
        ImGui::PopID();
      }
    }
    for (const film::SequenceClip& clip : sequence->clips) {
      if (!isInLane(clip, lanes[lane_index])) {
        continue;
      }

      const auto [start, end] = previewRange(clip);
      const ImVec2 bar_min(frame_x(start), y + 6.0f);
      const ImVec2 bar_max(frame_x(end), y + 27.0f);
      const bool selected = parSession.movie_selection.clip_id == clip.id;
      const ImU32 color = selected ? IM_COL32(100, 166, 221, 255)
                                   : IM_COL32(65, 122, 171, 255);
      draw_list->AddRectFilled(bar_min, bar_max, color, 3.0f);
      draw_list->AddText(ImVec2(bar_min.x + 4.0f, bar_min.y + 3.0f),
                        IM_COL32(245, 248, 250, 255), movieClipLabel(clip.payload));

      pushMovieWidgetId(MovieWidgetIdKind::SequenceClip, clip.id);
      ImGui::SetCursorScreenPos(ImVec2(bar_min.x + HANDLE_WIDTH, bar_min.y));
      if (ImGui::InvisibleButton("##Move", ImVec2(
              std::max(1.0f, bar_max.x - bar_min.x - HANDLE_WIDTH * 2.0f),
              bar_max.y - bar_min.y))) {
        selectClip(clip.id);
      }
      if (ImGui::IsItemActivated()) {
        gesture = {clip.id, ClipGesture::Mode::Move, clip.start_frame,
                   clip.end_frame, ImGui::GetIO().MousePos.x};
      }

      ImGui::SetCursorScreenPos(bar_min);
      ImGui::InvisibleButton("##TrimStart", ImVec2(HANDLE_WIDTH, bar_max.y - bar_min.y));
      if (ImGui::IsItemActivated()) {
        selectClip(clip.id);
        gesture = {clip.id, ClipGesture::Mode::TrimStart, clip.start_frame,
                   clip.end_frame, ImGui::GetIO().MousePos.x};
      }
      if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        draw_list->AddLine(ImVec2(bar_min.x + 1.0f, bar_min.y + 2.0f),
                           ImVec2(bar_min.x + 1.0f, bar_max.y - 2.0f),
                           IM_COL32(245, 248, 250, 255), 2.0f);
      }
      ImGui::SetCursorScreenPos(ImVec2(bar_max.x - HANDLE_WIDTH, bar_min.y));
      ImGui::InvisibleButton("##TrimEnd", ImVec2(HANDLE_WIDTH, bar_max.y - bar_min.y));
      if (ImGui::IsItemActivated()) {
        selectClip(clip.id);
        gesture = {clip.id, ClipGesture::Mode::TrimEnd, clip.start_frame,
                   clip.end_frame, ImGui::GetIO().MousePos.x};
      }
      if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        draw_list->AddLine(ImVec2(bar_max.x - 1.0f, bar_min.y + 2.0f),
                           ImVec2(bar_max.x - 1.0f, bar_max.y - 2.0f),
                           IM_COL32(245, 248, 250, 255), 2.0f);
      }
      ImGui::PopID();
    }
  }

  const float playhead_x = frame_x(parSession.authoring_cursor_frame);
  drawTimelinePlayhead(*draw_list, playhead_x, origin.y,
                        origin.y + content_height);
  static_cast<void>(dragTimelinePlayhead(
      "##TargetSequenceTimelinePlayhead", origin, LABEL_WIDTH, content_height,
      pixels_per_frame, parSession, parEngine.getFilmPlayback(), duration));
  ImGui::SetCursorScreenPos(origin);
  ImGui::Dummy(ImVec2(content_width, content_height));

  if (gesture.mode != ClipGesture::Mode::None &&
      !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    const film::SequenceClip* clip =
        parEngine.getMovieTimeline().findClip(gesture.clip_id);
    if (clip != nullptr) {
      const auto [start, end] = previewRange(*clip);
      if (start != gesture.start_frame || end != gesture.end_frame) {
        film::TimelineEditService edits(parEngine.getMovieTimeline());
        const auto result = edits.moveClip(gesture.clip_id, start, end);
        if (result.has_value()) {
          parEngine.markProjectDirty();
          parError.clear();
          sequence = parEngine.getMovieTimeline().findSequence(sequence_id);
          if (sequence == nullptr) {
            parSession.movie_selection.sequence_id = 0;
          }
          gesture = {};
          ImGui::EndChild();
          return;
        } else {
          parError = result.error();
        }
      }
    }
    gesture = {};
  }
  ImGui::EndChild();

}

}  // namespace kage::editor
