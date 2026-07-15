#include "editor/movie_inspector.hpp"

#include "editor/movie_editor_controller.hpp"
#include "editor/movie_imgui_scope.hpp"
#include "editor/timeline_view_helpers.hpp"
#include "film/timeline_edit_service.hpp"

#include <imgui.h>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace kage::editor {
namespace {

struct NameEditState final {
  std::array<char, 128> buffer{};
  std::uint64_t id = 0;
  std::string name;
  bool initialized = false;
};

void refreshNameEditState(NameEditState& parState, std::uint64_t parId,
                          std::string_view parName) {
  if (parState.initialized && parState.id == parId &&
      parState.name == parName) {
    return;
  }
  parState.buffer.fill('\0');
  const std::size_t copy_size =
      std::min(parName.size(), parState.buffer.size() - 1);
  std::memcpy(parState.buffer.data(), parName.data(), copy_size);
  parState.id = parId;
  parState.name = parName;
  parState.initialized = true;
}

template <typename Result>
[[nodiscard]] bool acceptMovieEdit(engine::EngineCore& parEngine,
                                   std::string& parError,
                                   const Result& parResult) {
  if (!parResult.has_value()) {
    parError = parResult.error();
    return false;
  }
  parEngine.markProjectDirty();
  parError.clear();
  return true;
}

[[nodiscard]] bool setClipPayload(engine::EngineCore& parEngine,
                                  film::SequenceClipId parClipId,
                                  film::SequenceClipPayload parPayload,
                                  std::string& parError) {
  film::TimelineEditService edits(parEngine.getMovieTimeline());
  const auto result = edits.setClipPayload(parClipId, std::move(parPayload));
  return acceptMovieEdit(parEngine, parError, result);
}

void drawTransformPoint(const char* parLabel, math::Transform& parTransform,
                        bool& parChanged) {
  ImGui::PushID(parLabel);
  ImGui::SeparatorText(parLabel);
  parChanged |= ImGui::DragFloat3("Position", &parTransform.translation.x, 0.05f);
  glm::vec3 rotation = glm::degrees(glm::eulerAngles(parTransform.rotation));
  if (ImGui::DragFloat3("Rotation", &rotation.x, 0.5f)) {
    parTransform.rotation = glm::normalize(glm::quat(glm::radians(rotation)));
    parChanged = true;
  }
  ImGui::PopID();
}

enum class CurvePreset { Linear, EaseIn, EaseOut, EaseInOut, Custom };

[[nodiscard]] bool closeEnough(float parLeft, float parRight) {
  return std::abs(parLeft - parRight) <= 0.0001f;
}

[[nodiscard]] bool closeEnough(const glm::vec4& parLeft,
                               const glm::vec4& parRight) {
  return glm::length(parLeft - parRight) <= 0.0001f;
}

[[nodiscard]] CurvePreset timingPreset(float parControl1,
                                       float parControl2) {
  if (closeEnough(parControl1, 1.0f / 3.0f) &&
      closeEnough(parControl2, 2.0f / 3.0f)) {
    return CurvePreset::Linear;
  }
  if (closeEnough(parControl1, 0.0f) && closeEnough(parControl2, 0.0f)) {
    return CurvePreset::EaseIn;
  }
  if (closeEnough(parControl1, 1.0f) && closeEnough(parControl2, 1.0f)) {
    return CurvePreset::EaseOut;
  }
  if (closeEnough(parControl1, 0.0f) && closeEnough(parControl2, 1.0f)) {
    return CurvePreset::EaseInOut;
  }
  return CurvePreset::Custom;
}

void applyTimingPreset(CurvePreset parPreset, film::MovementCurve& parCurve) {
  switch (parPreset) {
    case CurvePreset::Linear:
      parCurve.timing_control_1 = 1.0f / 3.0f;
      parCurve.timing_control_2 = 2.0f / 3.0f;
      break;
    case CurvePreset::EaseIn:
      parCurve.timing_control_1 = 0.0f;
      parCurve.timing_control_2 = 0.0f;
      break;
    case CurvePreset::EaseOut:
      parCurve.timing_control_1 = 1.0f;
      parCurve.timing_control_2 = 1.0f;
      break;
    case CurvePreset::EaseInOut:
      parCurve.timing_control_1 = 0.0f;
      parCurve.timing_control_2 = 1.0f;
      break;
    case CurvePreset::Custom:
      break;
  }
}

[[nodiscard]] CurvePreset propertyPreset(const film::PropertyClip& parProperty) {
  const glm::vec4 linear_1 = glm::mix(parProperty.start_value,
                                      parProperty.end_value, 1.0f / 3.0f);
  const glm::vec4 linear_2 = glm::mix(parProperty.start_value,
                                      parProperty.end_value, 2.0f / 3.0f);
  if (closeEnough(parProperty.control_1, linear_1) &&
      closeEnough(parProperty.control_2, linear_2)) {
    return CurvePreset::Linear;
  }
  if (closeEnough(parProperty.control_1, parProperty.start_value) &&
      closeEnough(parProperty.control_2, parProperty.start_value)) {
    return CurvePreset::EaseIn;
  }
  if (closeEnough(parProperty.control_1, parProperty.end_value) &&
      closeEnough(parProperty.control_2, parProperty.end_value)) {
    return CurvePreset::EaseOut;
  }
  if (closeEnough(parProperty.control_1, parProperty.start_value) &&
      closeEnough(parProperty.control_2, parProperty.end_value)) {
    return CurvePreset::EaseInOut;
  }
  return CurvePreset::Custom;
}

void applyPropertyPreset(CurvePreset parPreset,
                         film::PropertyClip& parProperty) {
  switch (parPreset) {
    case CurvePreset::Linear:
      parProperty.control_1 = glm::mix(parProperty.start_value,
                                       parProperty.end_value, 1.0f / 3.0f);
      parProperty.control_2 = glm::mix(parProperty.start_value,
                                       parProperty.end_value, 2.0f / 3.0f);
      break;
    case CurvePreset::EaseIn:
      parProperty.control_1 = parProperty.start_value;
      parProperty.control_2 = parProperty.start_value;
      break;
    case CurvePreset::EaseOut:
      parProperty.control_1 = parProperty.end_value;
      parProperty.control_2 = parProperty.end_value;
      break;
    case CurvePreset::EaseInOut:
      parProperty.control_1 = parProperty.start_value;
      parProperty.control_2 = parProperty.end_value;
      break;
    case CurvePreset::Custom:
      break;
  }
}

[[nodiscard]] bool drawCurvePreset(const char* parLabel,
                                   CurvePreset& parPreset) {
  constexpr const char* ITEMS =
      "Linear\0Ease In\0Ease Out\0Ease In-Out\0Custom (loaded)\0";
  int selected = static_cast<int>(parPreset);
  if (!ImGui::Combo(parLabel, &selected, ITEMS)) {
    return false;
  }
  parPreset = static_cast<CurvePreset>(selected);
  return parPreset != CurvePreset::Custom;
}

[[nodiscard]] bool drawSpeedPreset(const char* parLabel,
                                   CurvePreset& parPreset) {
  constexpr std::array<const char*, 4> LABELS = {
      "Linear", "Ease In", "Ease Out", "Ease In-Out"};
  const int selected = static_cast<int>(parPreset);
  const bool has_selected_preset =
      selected >= 0 && selected < static_cast<int>(LABELS.size());
  const char* preview = has_selected_preset
                            ? LABELS[static_cast<std::size_t>(selected)]
                            : "Select preset...";
  bool changed = false;
  if (ImGui::BeginCombo(parLabel, preview)) {
    for (std::size_t index = 0; index < LABELS.size(); ++index) {
      const bool is_selected = selected == static_cast<int>(index);
      if (ImGui::Selectable(LABELS[index], is_selected)) {
        parPreset = static_cast<CurvePreset>(index);
        changed = true;
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  return changed;
}

[[nodiscard]] bool drawMovementInspector(
    engine::EngineCore& parEngine, const film::TargetSequence& parSequence,
    const film::SequenceClip& parClip, std::string& parError) {
  const auto* source = std::get_if<film::MovementClip>(&parClip.payload);
  if (source == nullptr) {
    return false;
  }
  film::MovementClip movement = *source;
  int start_mode = static_cast<int>(movement.start_mode);
  if (ImGui::Combo("Start", &start_mode,
                   "Previous endpoint\0Explicit position\0")) {
    const auto mode = static_cast<film::MovementStartMode>(start_mode);
    std::optional<math::Transform> explicit_start = movement.explicit_start;
    if (mode == film::MovementStartMode::ExplicitPosition &&
        !explicit_start.has_value()) {
      if (const auto* entity = std::get_if<film::CapturedEntityBaseState>(
              &parSequence.captured_base)) {
        explicit_start = entity->transform;
      }
    }
    film::TimelineEditService edits(parEngine.getMovieTimeline());
    const auto result = edits.setMovementStartMode(parClip.id, mode, explicit_start);
    if (acceptMovieEdit(parEngine, parError, result)) {
      return true;
    }
  }

  bool changed = false;
  if (movement.start_mode == film::MovementStartMode::ExplicitPosition &&
      movement.explicit_start.has_value()) {
    drawTransformPoint("Explicit Start Point", *movement.explicit_start, changed);
  }
  drawTransformPoint("End Point", movement.end, changed);

  const auto resolved_movements = film::resolveMovementSegments(parSequence);
  const auto resolved = std::find_if(
      resolved_movements.begin(), resolved_movements.end(),
      [clip_id = parClip.id](const film::ResolvedMovementSegment& item) {
        return item.clip_id == clip_id;
      });
  int path_mode = movement.curve.automatic_position_controls ? 0 : 1;
  if (ImGui::Combo("Path", &path_mode, "Straight\0Custom curve\0")) {
    if (path_mode == 0) {
      movement.curve.automatic_position_controls = true;
    } else if (resolved != resolved_movements.end()) {
      movement.curve.position_control_1 = resolved->movement.control_1;
      movement.curve.position_control_2 = resolved->movement.control_2;
      movement.curve.automatic_position_controls = false;
    }
    changed = true;
  }
  if (!movement.curve.automatic_position_controls &&
      resolved != resolved_movements.end()) {
    const glm::vec3 start = resolved->movement.start.translation;
    glm::vec3 leaving_start = movement.curve.position_control_1 - start;
    glm::vec3 approaching_end =
        movement.curve.position_control_2 - movement.end.translation;
    const float sensitivity = std::max(
        glm::length(movement.end.translation - start) * 0.01f, 0.01f);
    if (ImGui::DragFloat3("Curve leaving start", &leaving_start.x,
                          sensitivity)) {
      movement.curve.position_control_1 = start + leaving_start;
      changed = true;
    }
    if (ImGui::DragFloat3("Curve approaching end", &approaching_end.x,
                          sensitivity)) {
      movement.curve.position_control_2 =
          movement.end.translation + approaching_end;
      changed = true;
    }
  }
  CurvePreset speed_profile = timingPreset(
      movement.curve.timing_control_1, movement.curve.timing_control_2);
  if (drawSpeedPreset("Speed profile", speed_profile)) {
    applyTimingPreset(speed_profile, movement.curve);
    changed = true;
  }
  if (changed) {
    return setClipPayload(parEngine, parClip.id, movement, parError);
  }

  if (resolved == resolved_movements.end() ||
      !resolved->transition_before.has_value()) {
    return false;
  }
  ImGui::SeparatorText("Transition Before");
  film::MovementTransition transition = movement.transition_before;
  if (ImGui::Checkbox("Enabled", &transition.enabled)) {
    if (transition.enabled) {
      transition.curve.automatic_position_controls = true;
      applyTimingPreset(CurvePreset::Linear, transition.curve);
    }
    film::TimelineEditService edits(parEngine.getMovieTimeline());
    const auto result = edits.setMovementTransition(parClip.id, transition);
    if (acceptMovieEdit(parEngine, parError, result)) {
      return true;
    }
  }
  if (!transition.enabled) {
    return false;
  }
  bool transition_changed = false;
  if (resolved->transition_before.has_value()) {
    const film::ResolvedMovementSpline& transition_path =
        resolved->transition_before->spline;
    glm::vec3 leaving_start =
        transition_path.control_1 - transition_path.start.translation;
    glm::vec3 approaching_end =
        transition_path.control_2 - transition_path.end.translation;
    const glm::vec3 transition_delta =
        transition_path.end.translation - transition_path.start.translation;
    const float sensitivity =
        std::max(glm::length(transition_delta) * 0.01f, 0.01f);
    bool transition_curve_initialized =
        !transition.curve.automatic_position_controls;
    const auto initialize_transition_curve = [&]() {
      if (transition_curve_initialized) {
        return true;
      }
      transition.curve.position_control_1 = transition_path.control_1;
      transition.curve.position_control_2 = transition_path.control_2;
      transition.curve.automatic_position_controls = false;
      transition_curve_initialized = true;
      return transition_curve_initialized;
    };

    ImGui::PushID("TransitionCurve");
    if (ImGui::DragFloat3("Curve leaving start", &leaving_start.x,
                          sensitivity) &&
        initialize_transition_curve()) {
      transition.curve.position_control_1 =
          transition_path.start.translation + leaving_start;
      transition_changed = true;
    }
    if (ImGui::DragFloat3("Curve approaching end", &approaching_end.x,
                          sensitivity) &&
        initialize_transition_curve()) {
      transition.curve.position_control_2 =
          transition_path.end.translation + approaching_end;
      transition_changed = true;
    }
    ImGui::PopID();
  }
  CurvePreset transition_profile = timingPreset(
      transition.curve.timing_control_1,
      transition.curve.timing_control_2);
  if (drawSpeedPreset("Transition speed", transition_profile)) {
    applyTimingPreset(transition_profile, transition.curve);
    transition_changed = true;
  }
  if (transition_changed) {
    film::TimelineEditService edits(parEngine.getMovieTimeline());
    const auto result = edits.setMovementTransition(parClip.id, transition);
    if (acceptMovieEdit(parEngine, parError, result)) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] bool drawAnimationInspector(
    engine::EngineCore& parEngine, const film::TargetSequence& parSequence,
    const film::SequenceClip& parClip, std::string& parError) {
  const auto* source = std::get_if<film::RigAnimationClip>(&parClip.payload);
  if (source == nullptr) {
    return false;
  }
  film::RigAnimationClip animation = *source;
  if (const assets::ModelAsset* asset = movieAnimationAsset(parEngine, parSequence);
      asset != nullptr && !asset->animation_clips.empty()) {
    const assets::AnimationClip* selected = &asset->animation_clips.front();
    for (const assets::AnimationClip& candidate : asset->animation_clips) {
      if (candidate.id == animation.clip_id) {
        selected = &candidate;
        break;
      }
    }
    if (ImGui::BeginCombo("Source clip", selected->name.c_str())) {
      for (const assets::AnimationClip& candidate : asset->animation_clips) {
        pushMovieWidgetId(MovieWidgetIdKind::AnimationAssetClip, candidate.id);
        if (ImGui::Selectable(candidate.name.c_str(),
                              candidate.id == animation.clip_id)) {
          animation.clip_id = candidate.id;
          const bool mutated =
              setClipPayload(parEngine, parClip.id, animation, parError);
          ImGui::PopID();
          ImGui::EndCombo();
          return mutated;
        }
        ImGui::PopID();
      }
      ImGui::EndCombo();
    }
  } else {
    ImGui::TextDisabled("No loaded animation clips are available.");
  }
  bool changed = false;
  changed |= ImGui::DragFloat("Source in", &animation.source_in, 0.01f, 0.0f,
                              1.0f);
  changed |= ImGui::DragFloat("Source out", &animation.source_out, 0.01f, 0.0f,
                              1.0f);
  if (changed) {
    animation.source_in = std::clamp(animation.source_in, 0.0f, 0.999f);
    animation.source_out = std::clamp(animation.source_out,
                                      animation.source_in + 0.001f, 1.0f);
  }
  changed |= ImGui::DragFloat("Speed", &animation.speed, 0.01f, 0.0f, 10.0f);
  changed |= ImGui::Checkbox("Loop", &animation.looping);

  if (changed) {
    return setClipPayload(parEngine, parClip.id, animation, parError);
  }
  return false;
}

[[nodiscard]] bool isColorProperty(film::PropertyKind parKind) {
  return parKind == film::PropertyKind::PointLightColor ||
         parKind == film::PropertyKind::SunColor;
}

[[nodiscard]] bool isVectorProperty(film::PropertyKind parKind) {
  return parKind == film::PropertyKind::SunDirection;
}

bool drawPropertyValue(const char* parLabel, glm::vec4& parValue,
                       film::PropertyKind parKind) {
  if (isColorProperty(parKind)) {
    return ImGui::ColorEdit3(parLabel, &parValue.x);
  }
  if (isVectorProperty(parKind)) {
    return ImGui::DragFloat3(parLabel, &parValue.x, 0.01f);
  }
  return ImGui::DragFloat(parLabel, &parValue.x, 0.05f);
}

[[nodiscard]] bool drawPropertyInspector(engine::EngineCore& parEngine,
                                         const film::SequenceClip& parClip,
                                         std::string& parError) {
  const auto* source = std::get_if<film::PropertyClip>(&parClip.payload);
  if (source == nullptr) {
    return false;
  }
  film::PropertyClip property = *source;
  const char* start_label = "Start Value";
  const char* end_label = "End Value";
  switch (property.kind) {
    case film::PropertyKind::CameraFov:
      start_label = "Start FOV";
      end_label = "End FOV";
      break;
    case film::PropertyKind::PointLightIntensity:
    case film::PropertyKind::SunIntensity:
      start_label = "Start Intensity";
      end_label = "End Intensity";
      break;
    case film::PropertyKind::PointLightColor:
    case film::PropertyKind::SunColor:
      start_label = "Start Color";
      end_label = "End Color";
      break;
    case film::PropertyKind::SunDirection:
      start_label = "Start Direction";
      end_label = "End Direction";
      break;
  }
  CurvePreset interpolation = propertyPreset(property);
  bool changed = drawPropertyValue(start_label, property.start_value,
                                   property.kind);
  changed |= drawPropertyValue(end_label, property.end_value, property.kind);
  if (changed && interpolation != CurvePreset::Custom) {
    applyPropertyPreset(interpolation, property);
  }
  if (drawCurvePreset("Interpolation", interpolation)) {
    applyPropertyPreset(interpolation, property);
    changed = true;
  }
  if (changed) {
    return setClipPayload(parEngine, parClip.id, property, parError);
  }
  return false;
}

[[nodiscard]] bool drawClipInspector(engine::EngineCore& parEngine,
                                     const film::TargetSequence& parSequence,
                                     editor::EditorSession& parSession,
                                     std::string& parError) {
  const film::SequenceClip* clip = parEngine.getMovieTimeline().findClip(
      parSession.movie_selection.clip_id);
  const bool belongs_to_sequence = clip != nullptr && std::any_of(
      parSequence.clips.begin(), parSequence.clips.end(),
      [clip](const film::SequenceClip& candidate) { return candidate.id == clip->id; });
  if (!belongs_to_sequence) {
    parSession.movie_selection.clip_id = 0;
    return false;
  }
  const film::SequenceClipId clip_id = clip->id;
  ImGui::SeparatorText("Selected Clip");
  ImGui::Text("Frames [%d, %d)", clip->start_frame, clip->end_frame);
  pushMovieWidgetId(MovieWidgetIdKind::SequenceClip, clip_id);
  bool mutated = false;
  if (std::holds_alternative<film::MovementClip>(clip->payload)) {
    mutated = drawMovementInspector(parEngine, parSequence, *clip, parError);
  } else if (std::holds_alternative<film::RigAnimationClip>(clip->payload)) {
    mutated = drawAnimationInspector(parEngine, parSequence, *clip, parError);
  } else {
    mutated = drawPropertyInspector(parEngine, *clip, parError);
  }
  const float action_width =
      (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) *
      0.5f;
  if (!mutated && ImGui::Button("Duplicate Clip", ImVec2(action_width, 0.0f))) {
    film::TimelineEditService edits(parEngine.getMovieTimeline());
    const auto duplicate = edits.duplicateClip(clip_id);
    if (acceptMovieEdit(parEngine, parError, duplicate)) {
      parSession.movie_selection.clip_id = *duplicate;
      mutated = true;
    }
  }
  ImGui::SameLine();
  if (!mutated && ImGui::Button("Delete Clip", ImVec2(-1.0f, 0.0f))) {
    film::TimelineEditService edits(parEngine.getMovieTimeline());
    const auto result = edits.deleteClip(clip_id);
    if (acceptMovieEdit(parEngine, parError, result)) {
      parSession.movie_selection.clip_id = 0;
      mutated = true;
    }
  }
  ImGui::PopID();
  return mutated;
}

void drawMovieInspectorContext(engine::EngineCore& parEngine,
                               editor::EditorSession& parSession,
                               std::string& parError) {
  film::MovieTimeline& timeline = parEngine.getMovieTimeline();
  const film::SequenceInstance* selected_instance = timeline.findInstance(
      parSession.movie_selection.instance_id);

  ImGui::SeparatorText("Selected Instance");
  if (selected_instance == nullptr) {
    parSession.movie_selection.instance_id = 0;
  } else {
    const film::TargetSequence* sequence =
        timeline.findSequence(selected_instance->sequence_id);
    ImGui::Text("Start frame %d", selected_instance->start_frame);
    if (sequence != nullptr) {
      ImGui::Text("Target sequence: %s", sequence->name.c_str());
      ImGui::TextDisabled("%s", movieTargetLabel(parEngine, sequence->target).c_str());
    } else {
      ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.25f, 1.0f),
                         "Referenced target sequence is missing.");
    }

    if (ImGui::Button("Duplicate Instance", ImVec2(-1.0f, 0.0f))) {
      film::TimelineEditService edits(timeline);
      const auto duplicate = edits.duplicateInstance(selected_instance->id);
      if (acceptMovieEdit(parEngine, parError, duplicate)) {
        selectMovieInstance(parSession, *duplicate);
        return;
      }
    }
    if (ImGui::Button("Delete Instance", ImVec2(-1.0f, 0.0f))) {
      film::TimelineEditService edits(timeline);
      const auto result = edits.deleteInstance(selected_instance->id);
      if (acceptMovieEdit(parEngine, parError, result)) {
        parSession.movie_selection.instance_id = 0;
        return;
      }
    }
  }

  ImGui::SeparatorText("Camera Gap Mode");
  int gap_mode = static_cast<int>(timeline.camera_gap_mode);
  if (ImGui::Combo("Camera gaps", &gap_mode,
                   "Hold last camera state\0Black\0")) {
    film::TimelineEditService edits(timeline);
    const auto result = edits.setCameraGapMode(
        static_cast<film::CameraGapMode>(gap_mode));
    (void)acceptMovieEdit(parEngine, parError, result);
  }

  ImGui::SeparatorText("Bake Movie");
  const film::FinalRenderJob& render_job = parEngine.getFinalRenderJob();
  const film::TimelineValidation validation = parEngine.validateMovieTimeline(true);
  if (render_job.isActive()) {
    ImGui::ProgressBar(render_job.getProgress(), ImVec2(-1.0f, 0.0f),
                       "Baking");
    if (ImGui::Button("Cancel", ImVec2(-1.0f, 0.0f))) {
      parEngine.cancelFilmExport();
    }
  } else {
    {
      MovieDisabledScope disabled(timeline.durationFrames() <= 0 ||
                                  validation.hasErrors());
      if (ImGui::Button("Bake Movie", ImVec2(-1.0f, 0.0f))) {
        if (parEngine.exportFilmSequence(parError)) {
          parError.clear();
        }
      }
    }
  }
  if (validation.hasErrors()) {
    const auto error = std::find_if(
        validation.diagnostics.begin(), validation.diagnostics.end(),
        [](const film::TimelineDiagnostic& diagnostic) {
          return diagnostic.severity == film::TimelineDiagnostic::Severity::Error;
        });
    if (error != validation.diagnostics.end()) {
      ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.28f, 1.0f),
                         "Bake blocked: %s", error->message.c_str());
    }
  }
  if (render_job.getState() == film::FinalRenderState::Error) {
    ImGui::TextWrapped("Bake failed: %s", render_job.getError().c_str());
  }
  if (!parError.empty() &&
      (render_job.getState() != film::FinalRenderState::Error ||
       parError != render_job.getError())) {
    ImGui::TextWrapped("%s", parError.c_str());
  }
}

void drawTargetInspectorContext(engine::EngineCore& parEngine,
                                editor::EditorSession& parSession,
                                std::string& parError) {
  const film::TimelineTarget target = *parSession.movie_selection.target;
  film::MovieTimeline& timeline = parEngine.getMovieTimeline();
  ImGui::Text("%s", movieTargetLabel(parEngine, target).c_str());
  if (ImGui::Button("Deselect", ImVec2(-1.0f, 0.0f))) {
    parEngine.clearFilmPreviewState();
    deselectMovieTarget(parSession);
    return;
  }

  const std::optional<film::CapturedTargetBaseState> current_base =
      captureMovieTargetBase(parEngine, target);
  ImGui::SeparatorText("Sequences");
  for (const film::TargetSequence& sequence : timeline.sequences) {
    if (sequence.target != target) {
      continue;
    }
    const bool selected = parSession.movie_selection.sequence_id == sequence.id;
    pushMovieWidgetId(MovieWidgetIdKind::TargetSequence, sequence.id);
    if (ImGui::Selectable(sequence.name.c_str(), selected)) {
      parEngine.clearFilmPreviewState();
      selectMovieSequence(parSession, sequence);
    }
    ImGui::PopID();
  }

  {
    MovieDisabledScope disabled(!current_base.has_value());
    if (ImGui::Button("New Sequence", ImVec2(-1.0f, 0.0f)) &&
        current_base.has_value()) {
      film::TimelineEditService edits(timeline);
      const auto created = edits.createSequence(
          movieTargetLabel(parEngine, target) + " Sequence", target, *current_base);
      if (acceptMovieEdit(parEngine, parError, created)) {
        parEngine.clearFilmPreviewState();
        const film::TargetSequence* sequence = timeline.findSequence(*created);
        if (sequence != nullptr) {
          selectMovieSequence(parSession, *sequence);
        }
        return;
      }
    }
  }

  film::TargetSequence* selected_sequence =
      timeline.findSequence(parSession.movie_selection.sequence_id);
  const bool has_selected_sequence =
      selected_sequence != nullptr && selected_sequence->target == target;
  {
    MovieDisabledScope disabled(!has_selected_sequence);
    if (ImGui::Button("Duplicate Sequence", ImVec2(-1.0f, 0.0f)) &&
        has_selected_sequence) {
      film::TimelineEditService edits(timeline);
      const auto duplicate = edits.duplicateSequence(selected_sequence->id);
      if (acceptMovieEdit(parEngine, parError, duplicate)) {
        parEngine.clearFilmPreviewState();
        const film::TargetSequence* duplicated = timeline.findSequence(*duplicate);
        if (duplicated != nullptr) {
          selectMovieSequence(parSession, *duplicated);
        }
        return;
      }
    }
  }
  if (selected_sequence == nullptr || selected_sequence->target != target) {
    if (selected_sequence != nullptr) {
      parSession.movie_selection.sequence_id = 0;
      parSession.movie_selection.clip_id = 0;
    }
  } else {
    static NameEditState sequence_name_state;
    refreshNameEditState(sequence_name_state, selected_sequence->id,
                         selected_sequence->name);
    ImGui::TextUnformatted("Selected Sequence");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("Sequence Name", sequence_name_state.buffer.data(),
                         sequence_name_state.buffer.size())) {
      film::TimelineEditService edits(timeline);
      const auto result = edits.renameSequence(selected_sequence->id,
                                               sequence_name_state.buffer.data());
      if (acceptMovieEdit(parEngine, parError, result)) {
        return;
      }
    }
    if (ImGui::Button("Delete Sequence", ImVec2(-1.0f, 0.0f))) {
      const film::TargetSequenceId sequence_id = selected_sequence->id;
      film::TimelineEditService edits(timeline);
      const auto result = edits.deleteSequence(sequence_id);
      if (acceptMovieEdit(parEngine, parError, result)) {
        parEngine.clearFilmPreviewState();
        parSession.movie_selection.sequence_id = 0;
        parSession.movie_selection.clip_id = 0;
        parSession.movie_selection.instance_id = 0;
        return;
      }
    }
    if (drawClipInspector(parEngine, *selected_sequence, parSession, parError)) {
      return;
    }
  }
}

}  // namespace

void drawMovieInspector(engine::EngineCore& parEngine, EditorSession& parSession,
                        std::string& parError) {
  if (parSession.movie_selection.target.has_value()) {
    drawTargetInspectorContext(parEngine, parSession, parError);
  } else {
    drawMovieInspectorContext(parEngine, parSession, parError);
  }
}

}  // namespace kage::editor
