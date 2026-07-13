#include "editor/timeline_panel.hpp"

#include "editor/ui_layout.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace kage::editor {

std::optional<UiPanelRect> drawTimelinePanel(
    engine::EngineCore& parEngine, EditorSession& parSession,
    const glm::vec2& parViewportSize, FileBrowserDialog& parImportBrowser,
    std::array<char, 128>& parImportLabelBuffer, std::string& parImportError) {
  static_cast<void>(parViewportSize);
  static_cast<void>(parImportBrowser);
  static_cast<void>(parImportLabelBuffer);
  static_cast<void>(parImportError);

  constexpr float MIN_HEIGHT = 220.0f;
  const UiWorkArea area = getUiWorkArea();
  const float max_height =
      std::max(MIN_HEIGHT, (area.size.y - UI_STATUS_HEIGHT) * 0.7f);
  parSession.film_editor_height = std::clamp(
      parSession.film_editor_height, MIN_HEIGHT, max_height);
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
        MIN_HEIGHT, max_height);
  }

  film::MovieTimeline& timeline = parEngine.getMovieTimeline();
  film::FilmPlayback& playback = parEngine.getFilmPlayback();
  const film::FilmFrame duration = timeline.durationFrames();
  if (duration <= 0) {
    playback.playing = false;
    playback.previewing = false;
    playback.playhead_frame = 0.0;
  }

  ImGui::BeginDisabled(duration <= 0);
  if (ImGui::Button(playback.playing ? "Pause" : "Play")) {
    playback.playing = !playback.playing;
    playback.previewing = true;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::Button("Stop")) {
    playback.playing = false;
    playback.previewing = false;
    playback.playhead_frame = 0.0;
    parSession.shot_preview = false;
  }
  ImGui::SameLine();
  ImGui::Checkbox("Loop", &playback.looping);
  ImGui::SameLine();
  if (ImGui::Checkbox("Preview Camera", &parSession.shot_preview) &&
      !parSession.shot_preview && !playback.playing) {
    playback.previewing = false;
  }

  static std::string render_error;
  const film::FinalRenderJob& render_job = parEngine.getFinalRenderJob();
  const film::TimelineValidation bake_validation =
      film::validateMovieTimeline(timeline, true);
  ImGui::SameLine();
  if (render_job.isActive()) {
    ImGui::ProgressBar(render_job.getProgress(), ImVec2(130.0f, 0.0f),
                       "Baking");
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      parEngine.cancelFilmExport();
    }
  } else {
    ImGui::BeginDisabled(bake_validation.hasErrors());
    if (ImGui::Button("Bake Movie 2160p30")) {
      if (parEngine.exportFilmSequence(render_error)) {
        render_error.clear();
      }
    }
    ImGui::EndDisabled();
  }

  const int frame = duration > 0
                        ? std::clamp(static_cast<int>(std::floor(
                                         playback.playhead_frame)),
                                     0, duration - 1)
                        : 0;
  ImGui::SameLine();
  ImGui::Text("%02d:%02d:%02d  f%d / %d", frame / 1800,
              (frame / 30) % 60, frame % 30, frame, duration);
  if (duration > 0) {
    int edited_frame = frame;
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderInt("##MoviePlayhead", &edited_frame, 0, duration - 1)) {
      playback.playhead_frame = edited_frame;
      playback.playing = false;
      playback.previewing = true;
    }
  } else {
    ImGui::TextDisabled("Movie duration is zero. Add clips and an instance in a migrated or future Movie Editor workflow.");
  }

  if (!render_error.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.25f, 1.0f), "%s",
                       render_error.c_str());
  } else if (render_job.getState() == film::FinalRenderState::Error) {
    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.25f, 1.0f), "%s",
                       render_job.getError().c_str());
  }

  ImGui::Separator();
  ImGui::TextDisabled("MovieTimeline compatibility view (editing arrives in Milestone 3)");
  ImGui::Text("%zu sequences, %zu instances", timeline.sequences.size(),
              timeline.instances.size());
  for (const film::TargetSequence& sequence : timeline.sequences) {
    ImGui::BulletText("%s  (%zu clips, %d frames)", sequence.name.c_str(),
                      sequence.clips.size(), sequence.durationFrames());
  }

  ImGui::End();
  return panel_rect;
}

}  // namespace kage::editor
