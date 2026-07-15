#include "editor/movie_inspector.hpp"

#include "editor/movie_editor_controller.hpp"
#include "editor/movie_imgui_scope.hpp"
#include "editor/text_buffer.hpp"
#include "editor/timeline_view_helpers.hpp"
#include "film/timeline_edit_service.hpp"

#include <imgui.h>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <cmath>
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
  copyTextToBuffer(parName, parState.buffer);
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

template <typename Edit>
[[nodiscard]] bool applyMovieEdit(engine::EngineCore& parEngine,
                                  std::string& parError, Edit&& parEdit) {
  film::TimelineEditService edits(parEngine.getMovieTimeline());
  return acceptMovieEdit(parEngine, parError, parEdit(edits));
}

[[nodiscard]] bool setClipPayload(engine::EngineCore& parEngine,
                                  film::SequenceClipId parClipId,
                                  film::SequenceClipPayload parPayload,
                                  std::string& parError) {
  return applyMovieEdit(parEngine, parError, [&](auto& edits) {
    return edits.setClipPayload(parClipId, std::move(parPayload));
  });
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

struct CurvePresetDefinition final {
  const char* label;
  float control_1;
  float control_2;
};

constexpr std::array<CurvePresetDefinition, 4> CURVE_PRESETS{{
    {"Linear", 1.0f / 3.0f, 2.0f / 3.0f},
    {"Ease In", 0.0f, 0.0f},
    {"Ease Out", 1.0f, 1.0f},
    {"Ease In-Out", 0.0f, 1.0f},
}};
constexpr float CURVE_COMPARE_EPSILON = 0.0001f;

[[nodiscard]] bool closeEnough(float parLeft, float parRight) {
  return std::abs(parLeft - parRight) <= CURVE_COMPARE_EPSILON;
}

[[nodiscard]] bool closeEnough(const glm::vec4& parLeft,
                               const glm::vec4& parRight) {
  return glm::length(parLeft - parRight) <= CURVE_COMPARE_EPSILON;
}

[[nodiscard]] CurvePreset timingPreset(float parControl1,
                                       float parControl2) {
  for (std::size_t index = 0; index < CURVE_PRESETS.size(); ++index) {
    const CurvePresetDefinition& preset = CURVE_PRESETS[index];
    if (closeEnough(parControl1, preset.control_1) &&
        closeEnough(parControl2, preset.control_2)) {
      return static_cast<CurvePreset>(index);
    }
  }
  return CurvePreset::Custom;
}

void applyTimingPreset(CurvePreset parPreset, film::MovementCurve& parCurve) {
  const std::size_t index = static_cast<std::size_t>(parPreset);
  if (index < CURVE_PRESETS.size()) {
    parCurve.timing_control_1 = CURVE_PRESETS[index].control_1;
    parCurve.timing_control_2 = CURVE_PRESETS[index].control_2;
  }
}

[[nodiscard]] bool drawCurveHandles(const char* parId,
                                    film::MovementCurve& parCurve,
                                    const film::ResolvedMovementSpline& parPath,
                                    bool parInitializeOnEdit) {
  if (parCurve.automatic_position_controls && !parInitializeOnEdit) {
    return false;
  }
  glm::vec3 leaving = parPath.control_1 - parPath.start.translation;
  glm::vec3 approaching = parPath.control_2 - parPath.end.translation;
  const float sensitivity = std::max(
      glm::length(parPath.end.translation - parPath.start.translation) * 0.01f,
      0.01f);
  bool changed = false;
  const auto commit = [&] {
    if (parCurve.automatic_position_controls) {
      parCurve.position_control_1 = parPath.control_1;
      parCurve.position_control_2 = parPath.control_2;
      parCurve.automatic_position_controls = false;
    }
  };
  ImGui::PushID(parId);
  if (ImGui::DragFloat3("Curve leaving start", &leaving.x, sensitivity)) {
    commit();
    parCurve.position_control_1 = parPath.start.translation + leaving;
    changed = true;
  }
  if (ImGui::DragFloat3("Curve approaching end", &approaching.x, sensitivity)) {
    commit();
    parCurve.position_control_2 = parPath.end.translation + approaching;
    changed = true;
  }
  ImGui::PopID();
  return changed;
}

[[nodiscard]] CurvePreset propertyPreset(const film::PropertyClip& parProperty) {
  for (std::size_t index = 0; index < CURVE_PRESETS.size(); ++index) {
    const CurvePresetDefinition& preset = CURVE_PRESETS[index];
    if (closeEnough(parProperty.control_1,
                    glm::mix(parProperty.start_value, parProperty.end_value,
                             preset.control_1)) &&
        closeEnough(parProperty.control_2,
                    glm::mix(parProperty.start_value, parProperty.end_value,
                             preset.control_2))) {
      return static_cast<CurvePreset>(index);
    }
  }
  return CurvePreset::Custom;
}

void applyPropertyPreset(CurvePreset parPreset,
                         film::PropertyClip& parProperty) {
  const std::size_t index = static_cast<std::size_t>(parPreset);
  if (index < CURVE_PRESETS.size()) {
    const CurvePresetDefinition& preset = CURVE_PRESETS[index];
    parProperty.control_1 = glm::mix(
        parProperty.start_value, parProperty.end_value, preset.control_1);
    parProperty.control_2 = glm::mix(
        parProperty.start_value, parProperty.end_value, preset.control_2);
  }
}

[[nodiscard]] bool drawPreset(const char* parLabel, CurvePreset& parPreset,
                              bool parShowLoadedCustom) {
  const int selected = static_cast<int>(parPreset);
  const bool has_preset = selected >= 0 &&
                          selected < static_cast<int>(CURVE_PRESETS.size());
  const char* preview = has_preset
                            ? CURVE_PRESETS[static_cast<std::size_t>(selected)].label
                            : parShowLoadedCustom ? "Custom (loaded)"
                                                  : "Select preset...";
  bool changed = false;
  if (ImGui::BeginCombo(parLabel, preview)) {
    for (std::size_t index = 0; index < CURVE_PRESETS.size(); ++index) {
      const bool is_selected = selected == static_cast<int>(index);
      if (ImGui::Selectable(CURVE_PRESETS[index].label, is_selected)) {
        parPreset = static_cast<CurvePreset>(index);
        changed = true;
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    if (parShowLoadedCustom && !has_preset) {
      ImGui::Separator();
      ImGui::Selectable("Custom (loaded)", true,
                        ImGuiSelectableFlags_Disabled);
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
    if (applyMovieEdit(parEngine, parError, [&](auto& edits) {
          return edits.setMovementStartMode(parClip.id, mode, explicit_start);
        })) {
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
    changed |= drawCurveHandles("MovementCurve", movement.curve,
                                resolved->movement, false);
  }
  CurvePreset speed_profile = timingPreset(
      movement.curve.timing_control_1, movement.curve.timing_control_2);
  if (drawPreset("Speed profile", speed_profile, false)) {
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
    if (applyMovieEdit(parEngine, parError, [&](auto& edits) {
          return edits.setMovementTransition(parClip.id, transition);
        })) {
      return true;
    }
  }
  if (!transition.enabled) {
    return false;
  }
  bool transition_changed = false;
  if (resolved->transition_before.has_value()) {
    transition_changed |= drawCurveHandles(
        "TransitionCurve", transition.curve,
        resolved->transition_before->spline, true);
  }
  CurvePreset transition_profile = timingPreset(
      transition.curve.timing_control_1,
      transition.curve.timing_control_2);
  if (drawPreset("Transition speed", transition_profile, false)) {
    applyTimingPreset(transition_profile, transition.curve);
    transition_changed = true;
  }
  if (transition_changed) {
    if (applyMovieEdit(parEngine, parError, [&](auto& edits) {
          return edits.setMovementTransition(parClip.id, transition);
        })) {
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

enum class PropertyEditor { Scalar, Color, Vector };

struct PropertyDescriptor final {
  film::PropertyKind kind;
  const char* start_label;
  const char* end_label;
  PropertyEditor editor = PropertyEditor::Scalar;
};

constexpr std::array PROPERTY_DESCRIPTORS{
    PropertyDescriptor{film::PropertyKind::CameraFov, "Start FOV", "End FOV"},
    PropertyDescriptor{film::PropertyKind::PointLightIntensity,
                       "Start Intensity", "End Intensity"},
    PropertyDescriptor{film::PropertyKind::PointLightColor, "Start Color",
                       "End Color", PropertyEditor::Color},
    PropertyDescriptor{film::PropertyKind::SunDirection, "Start Direction",
                       "End Direction", PropertyEditor::Vector},
    PropertyDescriptor{film::PropertyKind::SunIntensity, "Start Intensity",
                       "End Intensity"},
    PropertyDescriptor{film::PropertyKind::SunColor, "Start Color",
                       "End Color", PropertyEditor::Color},
};

[[nodiscard]] const PropertyDescriptor& propertyDescriptor(
    film::PropertyKind parKind) {
  const auto found = std::find_if(
      PROPERTY_DESCRIPTORS.begin(), PROPERTY_DESCRIPTORS.end(),
      [parKind](const auto& value) { return value.kind == parKind; });
  static constexpr PropertyDescriptor FALLBACK{
      film::PropertyKind::CameraFov, "Start Value", "End Value"};
  return found != PROPERTY_DESCRIPTORS.end() ? *found : FALLBACK;
}

bool drawPropertyValue(const char* parLabel, glm::vec4& parValue,
                       film::PropertyKind parKind) {
  switch (propertyDescriptor(parKind).editor) {
    case PropertyEditor::Color:
      return ImGui::ColorEdit3(parLabel, &parValue.x);
    case PropertyEditor::Vector:
      return ImGui::DragFloat3(parLabel, &parValue.x, 0.01f);
    case PropertyEditor::Scalar:
      return ImGui::DragFloat(parLabel, &parValue.x, 0.05f);
  }
  return false;
}

[[nodiscard]] bool drawPropertyInspector(engine::EngineCore& parEngine,
                                         const film::SequenceClip& parClip,
                                         std::string& parError) {
  const auto* source = std::get_if<film::PropertyClip>(&parClip.payload);
  if (source == nullptr) {
    return false;
  }
  film::PropertyClip property = *source;
  const PropertyDescriptor& descriptor = propertyDescriptor(property.kind);
  CurvePreset interpolation = propertyPreset(property);
  bool changed = drawPropertyValue(descriptor.start_label,
                                   property.start_value, property.kind);
  changed |= drawPropertyValue(descriptor.end_label, property.end_value,
                               property.kind);
  if (changed && interpolation != CurvePreset::Custom) {
    applyPropertyPreset(interpolation, property);
  }
  if (drawPreset("Interpolation", interpolation, true)) {
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
    if (applyMovieEdit(parEngine, parError, [&](auto& edits) {
          return edits.deleteClip(clip_id);
        })) {
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
      if (applyMovieEdit(parEngine, parError, [&](auto& edits) {
            return edits.deleteInstance(selected_instance->id);
          })) {
        parSession.movie_selection.instance_id = 0;
        return;
      }
    }
  }

  ImGui::SeparatorText("Camera Gap Mode");
  int gap_mode = static_cast<int>(timeline.camera_gap_mode);
  if (ImGui::Combo("Camera gaps", &gap_mode,
                   "Hold last camera state\0Black\0")) {
    (void)applyMovieEdit(parEngine, parError, [&](auto& edits) {
      return edits.setCameraGapMode(static_cast<film::CameraGapMode>(gap_mode));
    });
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
    if (const film::TimelineDiagnostic* error = validation.firstError()) {
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
      if (applyMovieEdit(parEngine, parError, [&](auto& edits) {
            return edits.renameSequence(selected_sequence->id,
                                        sequence_name_state.buffer.data());
          })) {
        return;
      }
    }
    if (ImGui::Button("Delete Sequence", ImVec2(-1.0f, 0.0f))) {
      const film::TargetSequenceId sequence_id = selected_sequence->id;
      if (applyMovieEdit(parEngine, parError, [&](auto& edits) {
            return edits.deleteSequence(sequence_id);
          })) {
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
