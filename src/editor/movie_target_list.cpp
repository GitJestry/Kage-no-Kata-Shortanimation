#include "editor/movie_target_list.hpp"

#include "editor/movie_editor_controller.hpp"
#include "editor/timeline_view_helpers.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

namespace kage::editor {
namespace {

void drawTargetSelectable(EditorSession& parSession,
                          engine::EngineCore& parEngine,
                          const film::TimelineTarget& parTarget,
                          const char* parLabel) {
  const bool selected = parSession.movie_selection.target == parTarget;
  const std::uint64_t target_identity =
      (static_cast<std::uint64_t>(parTarget.kind) << 32U) |
      static_cast<std::uint64_t>(parTarget.entity.value);
  pushMovieWidgetId(MovieWidgetIdKind::TimelineTarget, target_identity);
  if (ImGui::Selectable(parLabel, selected,
                        ImGuiSelectableFlags_AllowDoubleClick)) {
    resetMoviePreview(parEngine, parSession);
    selectMovieTarget(parSession, parEngine.getMovieTimeline(), parTarget);
    if (parTarget.kind != film::TimelineTargetKind::Sun &&
        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      parEngine.frameEntity(parTarget.entity);
    }
  }
  ImGui::PopID();
}

}  // namespace

void drawMovieTargetList(engine::EngineCore& parEngine, EditorSession& parSession,
                         std::string& parError) {
  if (ImGui::Button(parEngine.isProjectDirty() ? "Save Project *"
                                             : "Save Project",
                    ImVec2(-1.0f, 0.0f))) {
    parEngine.saveProject();
  }
  if (ImGui::Button("New Camera From View", ImVec2(-1.0f, 0.0f))) {
    const film::FilmFrame start =
        clampMovieAuthoringCursor(parSession.authoring_cursor_frame);
    const auto result = parEngine.createFilmCameraFromView(start);
    if (result.has_value()) {
      resetMoviePreview(parEngine, parSession);
      selectCreatedFilmCamera(parSession, result->entity, result->sequence_id,
                              result->instance_id);
      parError.clear();
    } else {
      parError = result.error();
    }
  }
  if (!parError.empty()) {
    ImGui::TextWrapped("%s", parError.c_str());
  }

  const auto drawSection = [&](const char* label,
                               film::TimelineTargetKind kind) {
    ImGui::SeparatorText(label);
    bool any = false;
    for (const scene::EntityRecord& entity : parEngine.getWorld().getEntities()) {
      const std::optional<film::TimelineTarget> target =
          movieTargetForEntity(entity);
      if (!target.has_value() || target->kind != kind) {
        continue;
      }
      any = true;
      drawTargetSelectable(parSession, parEngine, {kind, entity.id},
                           entity.name.name.c_str());
    }
    for (const film::TimelineTarget& target : orphanMovieTargetsForKind(
             parEngine.getWorld(), parEngine.getMovieTimeline(), kind)) {
      any = true;
      const std::string orphan_label =
          movieTargetLabel(parEngine, target) + " (orphaned)";
      drawTargetSelectable(parSession, parEngine, target,
                           orphan_label.c_str());
    }
    if (!any) {
      ImGui::TextDisabled("None");
    }
  };
  drawSection("Cameras", film::TimelineTargetKind::Camera);
  drawSection("Rigged Entities", film::TimelineTargetKind::RiggedEntity);
  drawSection("Point Lights", film::TimelineTargetKind::PointLight);
  ImGui::SeparatorText("Sun");
  drawTargetSelectable(parSession, parEngine,
                       {film::TimelineTargetKind::Sun, {}}, "Sun");
}

}  // namespace kage::editor
