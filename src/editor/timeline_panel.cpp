#include "editor/timeline_panel.hpp"

#include "editor/ui_layout.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace {

constexpr float MIN_TIMELINE_HEIGHT = 220.0f;
constexpr float TRACK_LABEL_WIDTH = 150.0f;
constexpr float TRACK_HEIGHT = 28.0f;

[[nodiscard]] const char* clipLabel(const kage::film::FilmClip& parClip) {
  if (std::holds_alternative<kage::film::FilmMovement>(parClip.payload)) {
    return "Movement";
  }
  if (std::holds_alternative<kage::film::RigAnimation>(parClip.payload)) {
    return "Rig clip";
  }
  const auto& property = std::get<kage::film::FilmProperty>(parClip.payload);
  switch (property.kind) {
    case kage::film::FilmPropertyKind::CameraFov:
      return "Camera FOV";
    case kage::film::FilmPropertyKind::LightEnabled:
      return "Light enabled";
    case kage::film::FilmPropertyKind::LightIntensity:
      return "Light intensity";
    case kage::film::FilmPropertyKind::LightColor:
      return "Light color";
    case kage::film::FilmPropertyKind::LightRange:
      return "Light range";
  }
  return "Property";
}

[[nodiscard]] int visualLane(const kage::film::FilmClip& parClip) {
  if (std::holds_alternative<kage::film::FilmMovement>(parClip.payload)) {
    return 0;
  }
  if (std::holds_alternative<kage::film::RigAnimation>(parClip.payload)) {
    return 1;
  }
  return 2 + static_cast<int>(
      std::get<kage::film::FilmProperty>(parClip.payload).kind);
}

[[nodiscard]] const char* laneName(int parLane) {
  constexpr const char* NAMES[] = {"Movement", "Animation", "Camera FOV",
                                   "Light enabled", "Light intensity",
                                   "Light color", "Light range"};
  return parLane >= 0 && parLane < 7 ? NAMES[parLane] : "Property";
}

[[nodiscard]] kage::math::Transform cameraTransform(
    const kage::camera::Camera& parCamera) {
  kage::math::Transform transform;
  transform.translation = parCamera.position;
  transform.rotation = parCamera.orientation;
  return transform;
}

}  // namespace

namespace kage::editor {

std::optional<UiPanelRect> drawTimelinePanel(
    engine::EngineCore& parEngine, EditorSession& parSession,
    const glm::vec2& parViewportSize, FileBrowserDialog& parImportBrowser,
    std::array<char, 128>& parImportLabelBuffer,
    std::string& parImportError) {
  static_cast<void>(parViewportSize);
  const UiWorkArea area = getUiWorkArea();
  const float max_height = std::max(
      MIN_TIMELINE_HEIGHT, (area.size.y - UI_STATUS_HEIGHT) * 0.7f);
  parSession.film_editor_height = std::clamp(
      parSession.film_editor_height, MIN_TIMELINE_HEIGHT, max_height);
  const ImVec2 size(area.size.x, parSession.film_editor_height);
  const ImVec2 position(
      area.position.x,
      area.position.y + area.size.y - parSession.film_editor_height -
          UI_STATUS_HEIGHT);
  ImGui::SetNextWindowPos(position, ImGuiCond_Always);
  ImGui::SetNextWindowSize(size, ImGuiCond_Always);
  const bool open = ImGui::Begin(
      "FilmEditor", nullptr,
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
          ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
  const UiPanelRect panel_rect = getCurrentPanelRect();
  if (!open) {
    ImGui::End();
    return panel_rect;
  }

  ImGui::SetCursorPosY(0.0f);
  ImGui::InvisibleButton("##FilmEditorResize", ImVec2(size.x, 8.0f));
  if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
    parSession.film_editor_height = std::clamp(
        parSession.film_editor_height - ImGui::GetIO().MouseDelta.y,
        MIN_TIMELINE_HEIGHT, max_height);
  }

  film::FilmTimeline& timeline = parEngine.getFilmSequence();
  film::FilmPlayback& playback = parEngine.getFilmPlayback();
  static float pixels_per_frame = 3.0f;
  static int scroll_frame = 0;
  film::FilmClipId& selected_clip = parSession.selected_film_clip;

  if (ImGui::Button(playback.playing ? "Pause" : "Play")) {
    playback.playing = !playback.playing;
  }
  ImGui::SameLine();
  if (ImGui::Button("Stop")) {
    playback.playing = false;
    playback.playhead_frame = 0.0;
  }
  ImGui::SameLine();
  if (ImGui::Button("<")) {
    playback.playhead_frame =
        std::max(0.0, std::floor(playback.playhead_frame) - 1.0);
  }
  ImGui::SameLine();
  if (ImGui::Button(">")) {
    playback.playhead_frame = std::min(
        static_cast<double>(timeline.duration_frames - 1),
        std::floor(playback.playhead_frame) + 1.0);
  }
  ImGui::SameLine();
  ImGui::Checkbox("Loop", &playback.looping);
  ImGui::SameLine();
  ImGui::Checkbox("Preview Camera", &parSession.shot_preview);
  ImGui::SameLine();
  static std::string final_render_error;
  const film::FinalRenderJob& final_render = parEngine.getFinalRenderJob();
  if (final_render.isActive()) {
    ImGui::ProgressBar(final_render.getProgress(), ImVec2(120.0f, 0.0f),
                       "Final Render");
    ImGui::SameLine();
    if (ImGui::Button("Cancel Render")) {
      parEngine.cancelFilmExport();
    }
  } else if (ImGui::Button("Bake Movie 2160p30")) {
    if (parEngine.exportFilmSequence(final_render_error)) {
      final_render_error.clear();
    }
  }
  if (final_render.getState() == film::FinalRenderState::Complete) {
    ImGui::SameLine();
    ImGui::TextDisabled("MP4 complete");
  }
  if (!final_render_error.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.25f, 1.0f), "%s",
                       final_render_error.c_str());
  } else if (final_render.getState() == film::FinalRenderState::Error) {
    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.25f, 1.0f), "%s",
                       final_render.getError().c_str());
  }

  float duration_seconds =
      static_cast<float>(timeline.duration_frames) /
      film::FILM_FRAMES_PER_SECOND;
  ImGui::SameLine();
  ImGui::SetNextItemWidth(110.0f);
  if (ImGui::DragFloat("Duration (s)", &duration_seconds, 0.1f, 1.0f,
                       60.0f * 60.0f, "%.1f")) {
    timeline.duration_frames = std::max(
        1, static_cast<int>(std::lround(duration_seconds *
                                        film::FILM_FRAMES_PER_SECOND)));
    playback.playhead_frame = std::min(
        playback.playhead_frame,
        static_cast<double>(timeline.duration_frames - 1));
    parEngine.markProjectDirty();
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80.0f);
  ImGui::SliderFloat("Zoom", &pixels_per_frame, 0.5f, 12.0f, "%.1fx");

  const int frame = static_cast<int>(std::floor(playback.playhead_frame));
  ImGui::SameLine();
  ImGui::Text("%02d:%02d:%02d  f%d", frame / 1800, (frame / 30) % 60,
              frame % 30, frame);

  const camera::Camera& editor_camera =
      parEngine.getCameraSystem().getEditorCamera();
  if (ImGui::Button("New Shot From View")) {
    const scene::EntityId camera = parEngine.createCameraEntityAt(
        "Film Camera", editor_camera.position);
    const int shot_start =
        std::clamp(frame, 0, timeline.duration_frames - 1);
    int shot_end = timeline.duration_frames;
    film::CameraCut* active_cut = nullptr;
    for (film::CameraCut& cut : timeline.camera_cuts) {
      if (shot_start >= cut.start_frame && shot_start < cut.end_frame) {
        active_cut = &cut;
        shot_end = cut.end_frame;
      } else if (cut.start_frame > shot_start) {
        shot_end = std::min(shot_end, cut.start_frame);
      }
    }
    film::FilmMovement movement;
    movement.start = cameraTransform(editor_camera);
    movement.end = movement.start;
    static_cast<void>(timeline.addClip(camera, shot_start, shot_end, movement));
    film::FilmProperty fov;
    fov.kind = film::FilmPropertyKind::CameraFov;
    fov.start_value = fov.control_1 = fov.control_2 = fov.end_value =
        glm::vec4(editor_camera.vertical_fov_degrees);
    static_cast<void>(timeline.addClip(camera, shot_start, shot_end, fov));
    if (active_cut != nullptr && active_cut->start_frame == shot_start) {
      active_cut->camera = camera;
    } else {
      if (active_cut != nullptr) {
        active_cut->end_frame = shot_start;
      }
      timeline.camera_cuts.push_back(
          {timeline.next_cut_id++, shot_start, shot_end, camera});
      std::sort(timeline.camera_cuts.begin(), timeline.camera_cuts.end(),
                [](const film::CameraCut& left,
                   const film::CameraCut& right) {
                  return left.start_frame < right.start_frame;
                });
    }
    parEngine.markProjectDirty();
  }
  ImGui::SameLine();
  const scene::EntityId selected_entity = parEngine.getSelectedEntity();
  scene::EntityRecord* selected =
      parEngine.getWorld().findEntity(selected_entity);
  const bool selected_camera = selected != nullptr && selected->camera.has_value();
  if (!selected_camera) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button("Update Camera Key From View") && selected_camera) {
    film::FilmTrack* track = timeline.findTrack(selected_entity);
    if (track == nullptr) {
      film::FilmMovement movement;
      movement.start = cameraTransform(editor_camera);
      movement.end = movement.start;
      static_cast<void>(timeline.addClip(selected_entity, 0,
                                         timeline.duration_frames, movement));
    } else {
      for (film::FilmClip& clip : track->clips) {
        if (frame < clip.start_frame || frame >= clip.end_frame) {
          continue;
        }
        if (auto* movement = std::get_if<film::FilmMovement>(&clip.payload)) {
          const int midpoint = (clip.start_frame + clip.end_frame) / 2;
          (frame <= midpoint ? movement->start : movement->end) =
              cameraTransform(editor_camera);
        }
        break;
      }
    }
    parEngine.markProjectDirty();
  }
  if (!selected_camera) {
    ImGui::EndDisabled();
  }
  ImGui::SameLine();
  if (selected == nullptr) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button("Add Movement") && selected != nullptr) {
    const int start = std::clamp(frame, 0, timeline.duration_frames - 1);
    const int end = std::min(start + film::FILM_FRAMES_PER_SECOND,
                             timeline.duration_frames);
    film::FilmMovement movement;
    movement.start = selected->transform.transform;
    movement.end = movement.start;
    if (timeline.addClip(selected->id, start, end, movement) != nullptr) {
      parEngine.markProjectDirty();
    }
  }
  if (selected == nullptr) {
    ImGui::EndDisabled();
  }

  ImGui::SameLine();
  if (ImGui::Button("Add Camera / Light")) {
    ImGui::OpenPopup("FilmPropertyMenu");
  }
  if (ImGui::BeginPopup("FilmPropertyMenu")) {
    const int start = std::clamp(frame, 0, timeline.duration_frames - 1);
    const int end = std::min(start + film::FILM_FRAMES_PER_SECOND,
                             timeline.duration_frames);
    const auto add_property = [&](film::FilmPropertyKind kind,
                                  const glm::vec4& value) {
      film::FilmProperty property;
      property.kind = kind;
      property.start_value = property.control_1 = value;
      property.control_2 = property.end_value = value;
      if (selected != nullptr &&
          timeline.addClip(selected->id, start, end, property) != nullptr) {
        parEngine.markProjectDirty();
      }
    };
    if (selected_camera && ImGui::MenuItem("Camera FOV")) {
      add_property(film::FilmPropertyKind::CameraFov,
                   glm::vec4(selected->camera->vertical_fov_degrees));
    }
    if (selected_camera && ImGui::MenuItem("Camera Cut")) {
      film::CameraCut* current = nullptr;
      int cut_end = timeline.duration_frames;
      for (film::CameraCut& cut : timeline.camera_cuts) {
        if (start >= cut.start_frame && start < cut.end_frame) {
          current = &cut;
        } else if (cut.start_frame > start) {
          cut_end = std::min(cut_end, cut.start_frame);
        }
      }
      if (current != nullptr && current->start_frame == start) {
        current->camera = selected->id;
      } else {
        if (current != nullptr) {
          cut_end = current->end_frame;
          current->end_frame = start;
        }
        timeline.camera_cuts.push_back(
            {timeline.next_cut_id++, start, cut_end, selected->id});
        std::sort(timeline.camera_cuts.begin(), timeline.camera_cuts.end(),
                  [](const film::CameraCut& left,
                     const film::CameraCut& right) {
                    return left.start_frame < right.start_frame;
                  });
      }
      parEngine.markProjectDirty();
    }
    if (selected != nullptr && selected->light.has_value()) {
      if (ImGui::MenuItem("Light intensity")) {
        add_property(film::FilmPropertyKind::LightIntensity,
                     glm::vec4(selected->light->intensity));
      }
      if (ImGui::MenuItem("Light color")) {
        add_property(film::FilmPropertyKind::LightColor,
                     glm::vec4(selected->light->color, 1.0f));
      }
      if (ImGui::MenuItem("Light range")) {
        add_property(film::FilmPropertyKind::LightRange,
                     glm::vec4(selected->light->range));
      }
      if (ImGui::MenuItem("Light enabled")) {
        add_property(film::FilmPropertyKind::LightEnabled,
                     glm::vec4(selected->light->enabled ? 1.0f : 0.0f));
      }
    }
    if (selected == nullptr ||
        (!selected_camera && !selected->light.has_value())) {
      ImGui::TextDisabled("Select a camera or light in the viewport");
    }
    ImGui::EndPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Animation Library")) {
    ImGui::OpenPopup("AnimationLibrary");
  }
  if (ImGui::BeginPopup("AnimationLibrary")) {
    ImGui::InputText("Import label", parImportLabelBuffer.data(),
                     parImportLabelBuffer.size());
    if (ImGui::Button("Import Animation...")) {
      parImportBrowser.open(
          "Import Animation",
          parEngine.getRuntimePaths().getAnimationDirectory());
    }
    if (!parImportError.empty()) {
      ImGui::TextWrapped("%s", parImportError.c_str());
    }
    if (selected != nullptr && selected->static_mesh.has_value()) {
      const auto* asset = parEngine.getAssetLibraryEntry(
          selected->static_mesh->asset_library_index);
      if (asset != nullptr && asset->document.has_value()) {
        for (std::size_t clip_index = 0;
             clip_index < asset->document->animation_clips.size();
             ++clip_index) {
          const assets::AnimationClip& animation =
              asset->document->animation_clips[clip_index];
          const std::string label = animation.name.empty()
                                        ? "Unnamed animation"
                                        : animation.name;
          if (ImGui::Selectable(label.c_str())) {
            const int start =
                std::clamp(frame, 0, timeline.duration_frames - 1);
            const int length = std::max(
                1, static_cast<int>(std::lround(
                       animation.duration_seconds *
                       film::FILM_FRAMES_PER_SECOND)));
            film::RigAnimation rig;
            rig.clip_id = animation.id;
            rig.legacy_clip_index = clip_index;
            if (timeline.addClip(selected->id, start,
                                 std::min(start + length,
                                          timeline.duration_frames),
                                 rig) != nullptr) {
              parEngine.markProjectDirty();
            }
          }
        }
      }
    }
    ImGui::EndPopup();
  }

  film::FilmClip* selected_clip_data = nullptr;
  for (film::FilmTrack& track : timeline.tracks) {
    const auto found = std::find_if(
        track.clips.begin(), track.clips.end(),
        [&](const film::FilmClip& clip) { return clip.id == selected_clip; });
    if (found != track.clips.end()) {
      selected_clip_data = &*found;
      break;
    }
  }
  if (selected_clip_data != nullptr) {
    ImGui::SameLine();
    ImGui::TextDisabled("%s  %d-%d", clipLabel(*selected_clip_data),
                        selected_clip_data->start_frame,
                        selected_clip_data->end_frame);
    ImGui::SameLine();
    if (ImGui::SmallButton("Delete Clip")) {
      if (timeline.removeClip(selected_clip)) {
        selected_clip = 0;
        selected_clip_data = nullptr;
        parEngine.markProjectDirty();
      }
    }
    if (selected_clip_data != nullptr) {
      bool changed = false;
      if (auto* movement =
              std::get_if<film::FilmMovement>(&selected_clip_data->payload)) {
        changed |= ImGui::Checkbox("Auto Bezier",
                                   &movement->automatic_position_controls);
        if (!movement->automatic_position_controls) {
          ImGui::SetNextItemWidth(190.0f);
          changed |= ImGui::DragFloat3("Bezier in",
                                       &movement->position_control_1.x, 0.02f);
          ImGui::SameLine();
          ImGui::SetNextItemWidth(190.0f);
          changed |= ImGui::DragFloat3("Bezier out",
                                       &movement->position_control_2.x, 0.02f);
        }
        ImGui::SetNextItemWidth(110.0f);
        changed |= ImGui::SliderFloat("Ease start",
                                      &movement->timing_control_1,
                                      0.0f, movement->timing_control_2);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        changed |= ImGui::SliderFloat("Ease end",
                                      &movement->timing_control_2,
                                      movement->timing_control_1, 1.0f);
      } else if (auto* rig =
                     std::get_if<film::RigAnimation>(
                         &selected_clip_data->payload)) {
        ImGui::SetNextItemWidth(90.0f);
        changed |= ImGui::DragFloat("Speed", &rig->speed, 0.02f, 0.0f, 8.0f);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        changed |= ImGui::SliderFloat("Source in", &rig->source_in,
                                      0.0f, rig->source_out, "%.2f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        changed |= ImGui::SliderFloat("Source out", &rig->source_out,
                                      rig->source_in, 1.0f, "%.2f");
        ImGui::SameLine();
        changed |= ImGui::Checkbox("Loop clip", &rig->looping);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        changed |= ImGui::DragFloat("Blend in", &rig->blend_in_seconds,
                                    0.02f, 0.0f, 5.0f);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        changed |= ImGui::DragFloat("Blend out", &rig->blend_out_seconds,
                                    0.02f, 0.0f, 5.0f);
      } else if (auto* property =
                     std::get_if<film::FilmProperty>(
                         &selected_clip_data->payload)) {
        ImGui::SetNextItemWidth(150.0f);
        changed |= ImGui::DragFloat4("Property start",
                                     &property->start_value.x, 0.02f);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        changed |= ImGui::DragFloat4("Property end",
                                     &property->end_value.x, 0.02f);
      }
      if (changed) {
        parEngine.markProjectDirty();
      }
      ImGui::SameLine();
      if (ImGui::Button(parSession.solo_clip_preview
                            ? "Stop Clip Preview"
                            : "Play Selected Clip")) {
        parSession.solo_clip_preview = !parSession.solo_clip_preview;
        playback.playing = parSession.solo_clip_preview;
        playback.playhead_frame = selected_clip_data->start_frame;
        parSession.shot_preview = false;
      }
    }
  }

  ImGui::Separator();
  const float available_width = ImGui::GetContentRegionAvail().x;
  const int visible_frames = std::max(
      1, static_cast<int>((available_width - TRACK_LABEL_WIDTH) /
                          pixels_per_frame));
  scroll_frame = std::clamp(scroll_frame, 0,
                            std::max(0, timeline.duration_frames - visible_frames));
  ImGui::SetNextItemWidth(available_width - TRACK_LABEL_WIDTH);
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + TRACK_LABEL_WIDTH);
  ImGui::SliderInt("##TimelineScroll", &scroll_frame, 0,
                   std::max(0, timeline.duration_frames - visible_frames));

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const float timeline_x = ImGui::GetCursorScreenPos().x + TRACK_LABEL_WIDTH;
  const float timeline_width = visible_frames * pixels_per_frame;
  const auto scrub_to_mouse = [&]() {
    const float local_x = std::clamp(
        ImGui::GetIO().MousePos.x - timeline_x, 0.0f, timeline_width);
    const int scrubbed = std::clamp(
        scroll_frame + static_cast<int>(std::lround(local_x /
                                                    pixels_per_frame)),
        0, timeline.duration_frames - 1);
    playback.playhead_frame = scrubbed;
    playback.playing = false;
  };

  const ImVec2 ruler_cursor = ImGui::GetCursorScreenPos();
  ImGui::SetCursorScreenPos(ImVec2(timeline_x, ruler_cursor.y));
  ImGui::InvisibleButton("##FilmTimeRuler", ImVec2(timeline_width, 22.0f));
  if (ImGui::IsItemClicked() ||
      (ImGui::IsItemActive() && ImGui::IsMouseDragging(0))) {
    scrub_to_mouse();
  }
  draw_list->AddRectFilled(ImVec2(timeline_x, ruler_cursor.y),
                           ImVec2(timeline_x + timeline_width,
                                  ruler_cursor.y + 22.0f),
                           IM_COL32(36, 41, 44, 255));
  for (int tick = ((scroll_frame + 29) / 30) * 30;
       tick <= scroll_frame + visible_frames; tick += 30) {
    const float x = timeline_x + (tick - scroll_frame) * pixels_per_frame;
    draw_list->AddLine(ImVec2(x, ruler_cursor.y + 10.0f),
                       ImVec2(x, ruler_cursor.y + 22.0f),
                       IM_COL32(130, 138, 142, 255));
    const std::string seconds = std::to_string(tick / 30) + "s";
    draw_list->AddText(ImVec2(x + 3.0f, ruler_cursor.y + 1.0f),
                       IM_COL32(190, 198, 202, 255), seconds.c_str());
  }
  ImGui::SetCursorScreenPos(
      ImVec2(ruler_cursor.x, ruler_cursor.y + 24.0f));

  auto draw_lane = [&](const char* label, auto&& draw_items) {
    const ImVec2 lane_pos = ImGui::GetCursorScreenPos();
    ImGui::PushID(label);
    ImGui::SetCursorScreenPos(ImVec2(timeline_x, lane_pos.y));
    ImGui::InvisibleButton("##LaneScrub",
                           ImVec2(timeline_width, TRACK_HEIGHT - 3.0f));
    if (ImGui::IsItemClicked() ||
        (ImGui::IsItemActive() && ImGui::IsMouseDragging(0))) {
      scrub_to_mouse();
    }
    ImGui::SetCursorScreenPos(lane_pos);
    ImGui::TextUnformatted(label);
    draw_list->AddRectFilled(
        ImVec2(timeline_x, lane_pos.y),
        ImVec2(timeline_x + visible_frames * pixels_per_frame,
               lane_pos.y + TRACK_HEIGHT - 3.0f),
        IM_COL32(30, 34, 37, 255));
    draw_items(lane_pos.y);
    ImGui::Dummy(ImVec2(available_width, TRACK_HEIGHT));
    ImGui::PopID();
  };

  draw_lane("Camera Cuts", [&](float y) {
    for (const film::CameraCut& cut : timeline.camera_cuts) {
      const float x1 = timeline_x +
          (cut.start_frame - scroll_frame) * pixels_per_frame;
      const float x2 = timeline_x +
          (cut.end_frame - scroll_frame) * pixels_per_frame;
      draw_list->AddRectFilled(ImVec2(x1, y + 2.0f),
                               ImVec2(x2, y + TRACK_HEIGHT - 5.0f),
                               IM_COL32(102, 87, 180, 255), 3.0f);
      draw_list->AddText(ImVec2(x1 + 4.0f, y + 5.0f), IM_COL32_WHITE,
                         "Camera");
    }
  });

  film::FilmClipId pending_clip = 0;
  int pending_start = 0;
  int pending_end = 0;
  for (film::FilmTrack& track : timeline.tracks) {
    const scene::EntityRecord* target =
        parEngine.getWorld().findEntity(track.target);
    const std::string target_label =
        target != nullptr ? target->name.name : "Missing target";
    for (int lane = 0; lane < 7; ++lane) {
      const bool has_lane = std::any_of(
          track.clips.begin(), track.clips.end(),
          [lane](const film::FilmClip& clip) {
            return visualLane(clip) == lane;
          });
      if (!has_lane) {
        continue;
      }
      const std::string label = target_label + " / " + laneName(lane);
      draw_lane(label.c_str(), [&](float y) {
      for (film::FilmClip& clip : track.clips) {
        if (visualLane(clip) != lane) {
          continue;
        }
        if (clip.end_frame <= scroll_frame ||
            clip.start_frame >= scroll_frame + visible_frames) {
          continue;
        }
        const float x1 = timeline_x +
            (clip.start_frame - scroll_frame) * pixels_per_frame;
        const float x2 = timeline_x +
            (clip.end_frame - scroll_frame) * pixels_per_frame;
        const ImVec2 clip_min(x1, y + 2.0f);
        const ImVec2 clip_max(x2, y + TRACK_HEIGHT - 5.0f);
        draw_list->AddRectFilled(
            clip_min, clip_max,
            clip.id == selected_clip ? IM_COL32(67, 137, 190, 255)
                                     : IM_COL32(54, 95, 126, 255),
            3.0f);
        draw_list->AddText(ImVec2(x1 + 5.0f, y + 5.0f), IM_COL32_WHITE,
                           clipLabel(clip));
        ImGui::PushID(static_cast<int>(clip.id));
        const float clip_width = std::max(8.0f, x2 - x1);
        const float edge_width = std::min(6.0f, clip_width * 0.25f);
        ImGui::SetCursorScreenPos(
            ImVec2(clip_min.x + edge_width, clip_min.y));
        ImGui::InvisibleButton(
            "clip", ImVec2(std::max(1.0f, clip_width - edge_width * 2.0f),
                            TRACK_HEIGHT - 7.0f));
        if (ImGui::IsItemClicked()) {
          selected_clip = clip.id;
        }
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
          const int delta = static_cast<int>(std::lround(
              ImGui::GetIO().MouseDelta.x / pixels_per_frame));
          if (delta != 0) {
            const int length = clip.end_frame - clip.start_frame;
            const int start = std::clamp(clip.start_frame + delta, 0,
                                         timeline.duration_frames - length);
            pending_clip = clip.id;
            pending_start = start;
            pending_end = start + length;
          }
        }
        ImGui::SetCursorScreenPos(clip_min);
        ImGui::InvisibleButton("trim_start",
                               ImVec2(edge_width, TRACK_HEIGHT - 7.0f));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
          const int delta = static_cast<int>(std::lround(
              ImGui::GetIO().MouseDelta.x / pixels_per_frame));
          if (delta != 0) {
            pending_clip = clip.id;
            pending_start = std::clamp(clip.start_frame + delta, 0,
                                       clip.end_frame - 1);
            pending_end = clip.end_frame;
          }
        }
        ImGui::SetCursorScreenPos(
            ImVec2(clip_max.x - edge_width, clip_min.y));
        ImGui::InvisibleButton("trim_end",
                               ImVec2(edge_width, TRACK_HEIGHT - 7.0f));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
          const int delta = static_cast<int>(std::lround(
              ImGui::GetIO().MouseDelta.x / pixels_per_frame));
          if (delta != 0) {
            pending_clip = clip.id;
            pending_start = clip.start_frame;
            pending_end = std::clamp(clip.end_frame + delta,
                                     clip.start_frame + 1,
                                     timeline.duration_frames);
          }
        }
        ImGui::PopID();
      }
      });
    }
  }
  if (pending_clip != 0 &&
      timeline.moveClip(pending_clip, pending_start, pending_end)) {
    parEngine.markProjectDirty();
  }

  const float playhead_x =
      timeline_x + (frame - scroll_frame) * pixels_per_frame;
  draw_list->AddLine(ImVec2(playhead_x, ImGui::GetWindowPos().y + 96.0f),
                     ImVec2(playhead_x,
                            ImGui::GetWindowPos().y +
                                parSession.film_editor_height),
                     IM_COL32(255, 102, 82, 255), 1.5f);

  if (const std::optional<std::string> error = timeline.validate()) {
    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.25f, 1.0f), "%s",
                       error->c_str());
  }

  ImGui::End();
  return panel_rect;
}

}  // namespace kage::editor
