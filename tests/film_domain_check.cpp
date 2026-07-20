#include "animation/animation_system.hpp"
#include "check_helpers.hpp"
#include "film/movie_timeline.hpp"
#include "film/timeline_edit_service.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

using namespace kage;
using namespace kage::film;
using kage::test::close;
using kage::test::fail;

[[nodiscard]] bool close(const glm::vec3& parLeft, const glm::vec3& parRight) {
  return glm::length(parLeft - parRight) < 0.001f;
}

[[nodiscard]] bool hasSeverity(const TimelineValidation& parValidation,
                               TimelineDiagnostic::Severity parSeverity) {
  return std::any_of(parValidation.diagnostics.begin(), parValidation.diagnostics.end(),
                     [parSeverity](const TimelineDiagnostic& diagnostic) {
                       return diagnostic.severity == parSeverity;
                     });
}

[[nodiscard]] CapturedTargetBaseState baseFor(TimelineTargetKind parKind) {
  if (parKind == TimelineTargetKind::Sun) {
    return CapturedSunBaseState{};
  }
  CapturedEntityBaseState base;
  if (parKind == TimelineTargetKind::Camera) {
    base.camera = CapturedCameraState{};
  } else if (parKind == TimelineTargetKind::PointLight) {
    base.point_light = CapturedPointLightState{};
  }
  return base;
}

[[nodiscard]] bool testFrameRangesAndEvaluation() {
  MovieTimeline timeline;
  TimelineEditService edits(timeline);
  CapturedEntityBaseState base;
  base.transform.translation.x = 10.0f;
  base.camera = CapturedCameraState{55.0f, 0.2f, 400.0f};
  const auto sequence = edits.createSequence("Camera", {TimelineTargetKind::Camera, {7}}, base);
  MovementClip movement;
  movement.end.translation.x = 20.0f;
  PropertyClip fov;
  fov.kind = PropertyKind::CameraFov;
  fov.start_value = fov.control_1 = glm::vec4(55.0f);
  fov.end_value = fov.control_2 = glm::vec4(65.0f);
  if (!sequence || !edits.appendClipToLane(*sequence, 10, movement) ||
      !edits.appendClipToLane(*sequence, 10, fov) || !edits.placeSequence(*sequence, 5) ||
      !edits.placeSequence(*sequence, 15) || timeline.durationFrames() != 25) {
    return false;
  }

  const auto local = evaluateTargetSequencePreview(timeline, *sequence, 5);
  const FilmFrameState movie = evaluateMovieTimeline(timeline, 10);
  const FilmFrameState reset = evaluateMovieTimeline(timeline, 15);
  const FilmFrameState held = evaluateMovieTimeline(timeline, 25);
  return local && !local->transforms.empty() &&
         close(local->transforms.front().transform.translation.x, 15.0f) && local->camera &&
         close(local->camera->vertical_fov_degrees, 60.0f) &&
         !evaluateTargetSequencePreview(timeline, 9999, 0) && movie.transforms.size() == 1 &&
         close(movie.transforms.front().transform.translation.x, 15.0f) && movie.camera &&
         close(movie.camera->vertical_fov_degrees, 60.0f) && !reset.transforms.empty() &&
         close(reset.transforms.front().transform.translation.x, 10.0f) && held.camera &&
         close(held.camera->transform.translation.x, 20.0f);
}

[[nodiscard]] bool testMovementEvaluation() {
  MovieTimeline timeline;
  TimelineEditService edits(timeline);
  const auto sequence = edits.createSequence("Rig", {TimelineTargetKind::RiggedEntity, {9}},
                                             CapturedEntityBaseState{});
  MovementClip first;
  first.end.translation.x = 10.0f;
  MovementClip second;
  second.end.translation.x = 110.0f;
  if (!sequence) {
    return false;
  }
  const auto first_id = edits.appendClipToLane(*sequence, 10, first);
  const auto second_id = edits.appendClipToLane(*sequence, 10, second);
  const auto continuous = evaluateTargetSequencePreview(timeline, *sequence, 10);
  math::Transform explicit_start;
  explicit_start.translation.x = 100.0f;
  const auto xAt = [&](FilmFrame frame) {
    return evaluateMovieTimeline(timeline, frame).transforms.front().transform.translation.x;
  };
  if (!first_id || !second_id || !continuous || continuous->transforms.empty() ||
      !close(continuous->transforms.front().transform.translation.x, 10.0f) ||
      !edits.setMovementStartMode(*second_id, MovementStartMode::ExplicitPosition,
                                  explicit_start) ||
      !edits.placeSequence(*sequence, 0) || !close(xAt(10), 100.0f) ||
      !edits.moveClip(*second_id, 20, 30)) {
    return false;
  }
  TargetSequence reordered = *timeline.findSequence(*sequence);
  std::reverse(reordered.clips.begin(), reordered.clips.end());
  const auto disabled = resolveMovementSegments(reordered);
  if (disabled.size() != 2 || disabled[0].clip_id != *first_id || disabled[0].transition_before ||
      !disabled[1].transition_before ||
      !close(disabled[0].movement.start.translation, glm::vec3(0.0f)) ||
      disabled[1].transition_before->enabled ||
      disabled[1].transition_before->predecessor_clip_id != *first_id ||
      !edits.setMovementTransition(*second_id, MovementTransition{true, {}})) {
    return false;
  }
  reordered.clips.front().start_frame = 10;
  const bool contiguous_transition =
      resolveMovementSegments(reordered)[1].transition_before.has_value();
  reordered.clips.front().start_frame = 20;
  std::get<MovementClip>(reordered.clips.front().payload).explicit_start.reset();
  if (contiguous_transition || resolveMovementSegments(reordered)[1].transition_before) {
    return false;
  }

  MovementClip manual = std::get<MovementClip>(timeline.findClip(*second_id)->payload);
  manual.curve.automatic_position_controls = false;
  manual.curve.position_control_1 = {101.0f, 2.0f, 3.0f};
  manual.curve.position_control_2 = {109.0f, 5.0f, 6.0f};
  if (!edits.setClipPayload(*second_id, manual)) {
    return false;
  }
  const auto paths = resolveMovementSegments(*timeline.findSequence(*sequence));
  const ResolvedMovementSegment& path = paths[1];
  return close(xAt(15), 55.0f) && paths.size() == 2 && path.transition_before &&
         path.movement.start_frame == 20 && path.movement.end_frame == 30 &&
         close(path.movement.start.translation, explicit_start.translation) &&
         close(path.movement.control_1, manual.curve.position_control_1) &&
         close(path.movement.control_2, manual.curve.position_control_2) &&
         path.transition_before->spline.start_frame == 10 &&
         path.transition_before->spline.end_frame == 20 &&
         close(path.transition_before->spline.timing_control_1, 1.0f / 3.0f) &&
         close(path.transition_before->spline.start.translation, glm::vec3(10.0f, 0.0f, 0.0f)) &&
         close(path.transition_before->spline.control_1, glm::vec3(40.0f, 0.0f, 0.0f)) &&
         close(path.transition_before->spline.control_2, glm::vec3(70.0f, 0.0f, 0.0f)) &&
         close(path.transition_before->spline.end.translation, explicit_start.translation) &&
         edits.setMovementStartMode(*second_id, MovementStartMode::PreviousEndpoint,
                                    std::nullopt) &&
         close(xAt(15), 10.0f) &&
         edits.setMovementStartMode(*second_id, MovementStartMode::ExplicitPosition,
                                    explicit_start) &&
         edits.deleteClip(*first_id) && close(xAt(15), 0.0f);
}

[[nodiscard]] bool testAnimationEvaluation() {
  assets::ModelAsset asset;
  asset.animation_clips = {
      {.id = 10, .name = "First", .duration_seconds = 0.0f, .samplers = {}, .channels = {}},
      {.id = 20, .name = "Second", .duration_seconds = 0.0f, .samplers = {}, .channels = {}},
  };
  if (animation::resolveAnimationClipIndex(asset, {.clip_id = 20}) != 1 ||
      animation::resolveAnimationClipIndex(asset, {.clip_id = 999})) {
    return false;
  }

  MovieTimeline bind_pose_timeline;
  TargetSequence bind_pose;
  bind_pose.id = 1;
  bind_pose.target = {TimelineTargetKind::RiggedEntity, {13}};
  bind_pose.clips.push_back({1, 10, 20, RigAnimationClip{.clip_id = 20}});
  bind_pose_timeline.sequences.push_back(bind_pose);
  bind_pose_timeline.instances.push_back({1, 1, 0});
  if (!evaluateMovieTimeline(bind_pose_timeline, 0).rig_animations.empty()) {
    return false;
  }

  struct LoopCase final {
    std::string_view name;
    bool looping;
    FilmFrame frame;
    float expected_seconds;
    bool final_pose;
  };
  constexpr std::array cases{
      LoopCase{"trimmed non-looping clip", false, 6, 0.1f, false},
      LoopCase{"trimmed non-looping hold", false, 12, 10.0f / 30.0f * 0.5f, true},
      LoopCase{"looping clip", true, 9, 0.3f, false},
      LoopCase{"looping final hold", true, 25, 20.0f / 30.0f, true},
  };
  for (const LoopCase& item : cases) {
    MovieTimeline timeline;
    TimelineEditService edits(timeline);
    const auto sequence = edits.createSequence(
        "Animation", {TimelineTargetKind::RiggedEntity, {14}}, CapturedEntityBaseState{});
    RigAnimationClip animation;
    animation.clip_id = 20;
    animation.source_in = 0.2f;
    animation.source_out = 0.4f;
    animation.speed = item.looping ? 1.0f : 0.5f;
    animation.looping = item.looping;
    const FilmFrame duration = item.looping ? 20 : 10;
    if (!sequence || !edits.appendClipToLane(*sequence, duration, animation) ||
        !edits.placeSequence(*sequence, 0)) {
      return false;
    }
    const FilmFrameState state = evaluateMovieTimeline(timeline, item.frame);
    if (state.rig_animations.size() != 1 ||
        state.rig_animations.front().animation.looping != item.looping ||
        state.rig_animations.front().final_pose != item.final_pose ||
        !close(state.rig_animations.front().local_time_seconds, item.expected_seconds)) {
      std::cerr << "animation looping mode failed: " << item.name << '\n';
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool testPropertyAndCameraGapEvaluation() {
  MovieTimeline timeline;
  TimelineEditService edits(timeline);
  CapturedEntityBaseState camera_base;
  camera_base.camera = CapturedCameraState{40.0f, 0.1f, 300.0f};
  const auto first_camera =
      edits.createSequence("First", {TimelineTargetKind::Camera, {21}}, camera_base);
  CapturedEntityBaseState second_camera_base = camera_base;
  second_camera_base.camera->vertical_fov_degrees = 70.0f;
  const auto second_camera =
      edits.createSequence("Second", {TimelineTargetKind::Camera, {22}}, second_camera_base);
  CapturedEntityBaseState light_base;
  light_base.point_light = CapturedPointLightState{false, {0.2f, 0.4f, 0.8f}, 2.0f, 42.0f, false};
  const auto light =
      edits.createSequence("Light", {TimelineTargetKind::PointLight, {23}}, light_base);
  const auto sun =
      edits.createSequence("Sun", {TimelineTargetKind::Sun, {}}, CapturedSunBaseState{});
  MovementClip still;
  PropertyClip fov;
  fov.kind = PropertyKind::CameraFov;
  fov.start_value = fov.control_1 = glm::vec4(40.0f);
  fov.end_value = fov.control_2 = glm::vec4(60.0f);
  PropertyClip intensity;
  intensity.kind = PropertyKind::PointLightIntensity;
  intensity.start_value = intensity.control_1 = glm::vec4(2.0f);
  intensity.end_value = intensity.control_2 = glm::vec4(6.0f);
  PropertyClip color;
  color.kind = PropertyKind::PointLightColor;
  color.start_value = color.control_1 = glm::vec4(0.2f, 0.4f, 0.8f, 1.0f);
  color.end_value = color.control_2 = glm::vec4(0.8f, 0.6f, 0.2f, 1.0f);
  PropertyClip sun_intensity;
  sun_intensity.kind = PropertyKind::SunIntensity;
  sun_intensity.start_value = sun_intensity.control_1 = glm::vec4(1.0f);
  sun_intensity.end_value = sun_intensity.control_2 = glm::vec4(3.0f);
  if (!first_camera || !second_camera || !light || !sun ||
      !edits.appendClipToLane(*first_camera, 10, still) ||
      !edits.appendClipToLane(*first_camera, 10, fov) ||
      !edits.appendClipToLane(*second_camera, 10, still) ||
      !edits.appendClipToLane(*light, 10, intensity) ||
      !edits.appendClipToLane(*light, 10, color) ||
      !edits.appendClipToLane(*sun, 10, sun_intensity) || !edits.placeSequence(*first_camera, 0) ||
      !edits.placeSequence(*second_camera, 20) || !edits.placeSequence(*light, 0) ||
      !edits.placeSequence(*sun, 0)) {
    return false;
  }
  const FilmFrameState active = evaluateMovieTimeline(timeline, 5);
  const FilmFrameState held_gap = evaluateMovieTimeline(timeline, 15);
  if (!active.camera || !close(active.camera->vertical_fov_degrees, 50.0f) || !active.sun ||
      !close(active.sun->intensity, 2.0f) || !held_gap.camera ||
      held_gap.camera->source_entity != scene::EntityId{21} ||
      !edits.setCameraGapMode(CameraGapMode::Black)) {
    return false;
  }
  const FilmFrameState black_gap = evaluateMovieTimeline(timeline, 15);
  const auto evaluated_light = std::find_if(active.point_lights.begin(), active.point_lights.end(),
                                            [](const EvaluatedPointLightState& item) {
                                              return item.source_entity == scene::EntityId{23};
                                            });
  return evaluated_light != active.point_lights.end() && !evaluated_light->enabled &&
         close(evaluated_light->color, glm::vec3(0.5f)) &&
         close(evaluated_light->intensity, 4.0f) && close(evaluated_light->range, 42.0f) &&
         !evaluated_light->casts_shadows && !black_gap.camera;
}

[[nodiscard]] bool testSelectedCameraUsesOneEvaluationPath() {
  MovieTimeline timeline;
  timeline.camera_gap_mode = CameraGapMode::Black;
  TimelineEditService edits(timeline);
  const scene::EntityId camera_entity{31};

  CapturedEntityBaseState long_base;
  long_base.transform.translation.x = 10.0f;
  long_base.camera = CapturedCameraState{40.0f, 0.1f, 300.0f};
  CapturedEntityBaseState short_base;
  short_base.transform.translation.x = 100.0f;
  short_base.camera = CapturedCameraState{70.0f, 0.1f, 300.0f};
  const auto long_sequence =
      edits.createSequence("Long", {TimelineTargetKind::Camera, camera_entity}, long_base);
  const auto short_sequence =
      edits.createSequence("Short", {TimelineTargetKind::Camera, camera_entity}, short_base);
  MovementClip long_movement;
  long_movement.end.translation.x = 50.0f;
  MovementClip short_movement;
  short_movement.end.translation.x = 200.0f;
  if (!long_sequence || !short_sequence ||
      !edits.appendClipToLane(*long_sequence, 40, long_movement) ||
      !edits.appendClipToLane(*short_sequence, 10, short_movement) ||
      !edits.placeSequence(*long_sequence, 0) || !edits.placeSequence(*short_sequence, 5)) {
    return false;
  }

  const FilmFrameState state = evaluateMovieTimeline(timeline, 25);
  const auto transform = std::find_if(
      state.transforms.begin(), state.transforms.end(),
      [camera_entity](const TransformOverride& item) { return item.entity == camera_entity; });
  return state.camera && transform != state.transforms.end() &&
         close(state.camera->vertical_fov_degrees, 40.0f) &&
         close(state.camera->transform.translation, transform->transform.translation) &&
         transform->transform.translation.x < short_base.transform.translation.x;
}

[[nodiscard]] bool testTimelineEdits() {
  struct PayloadCase final {
    std::string_view name;
    TimelineTargetKind target;
    SequenceClipPayload payload;
    bool accepted;
  };
  const std::array payload_cases{
      PayloadCase{"rig movement", TimelineTargetKind::RiggedEntity, MovementClip{}, true},
      PayloadCase{"rig animation", TimelineTargetKind::RiggedEntity, RigAnimationClip{.clip_id = 1},
                  true},
      PayloadCase{"rig camera property", TimelineTargetKind::RiggedEntity,
                  PropertyClip{.kind = PropertyKind::CameraFov}, false},
      PayloadCase{"camera FOV", TimelineTargetKind::Camera,
                  PropertyClip{.kind = PropertyKind::CameraFov}, true},
      PayloadCase{"camera animation", TimelineTargetKind::Camera, RigAnimationClip{.clip_id = 1},
                  false},
      PayloadCase{"point-light intensity", TimelineTargetKind::PointLight,
                  PropertyClip{.kind = PropertyKind::PointLightIntensity}, true},
      PayloadCase{"point-light FOV", TimelineTargetKind::PointLight,
                  PropertyClip{.kind = PropertyKind::CameraFov}, false},
      PayloadCase{"sun intensity", TimelineTargetKind::Sun,
                  PropertyClip{.kind = PropertyKind::SunIntensity}, true},
      PayloadCase{"sun movement", TimelineTargetKind::Sun, MovementClip{}, false},
  };
  for (const PayloadCase& item : payload_cases) {
    MovieTimeline timeline;
    TimelineEditService edits(timeline);
    const auto sequence = edits.createSequence("Payload",
                                               {item.target, item.target == TimelineTargetKind::Sun
                                                                 ? scene::EntityId{}
                                                                 : scene::EntityId{41}},
                                               baseFor(item.target));
    const auto clip =
        sequence ? edits.appendClipToLane(*sequence, 1, item.payload)
                 : std::expected<SequenceClipId, std::string>{std::unexpected("setup failed")};
    if (!sequence || clip.has_value() != item.accepted ||
        timeline.sequences.front().clips.size() != static_cast<std::size_t>(item.accepted)) {
      std::cerr << "payload compatibility failed: " << item.name << '\n';
      return false;
    }
  }

  MovieTimeline timeline;
  TimelineEditService edits(timeline);
  const auto sequence = edits.createSequence("Actor", {TimelineTargetKind::RiggedEntity, {42}},
                                             CapturedEntityBaseState{});
  const auto clip =
      sequence ? edits.appendClipToLane(*sequence, 10, MovementClip{})
               : std::expected<SequenceClipId, std::string>{std::unexpected("setup failed")};
  const auto instance =
      clip ? edits.placeSequence(*sequence, 0)
           : std::expected<SequenceInstanceId, std::string>{std::unexpected("setup failed")};
  if (!sequence || !clip || !instance) {
    return false;
  }
  struct InvalidFrameOperation final {
    std::string_view name;
    bool instance_operation;
    FilmFrame first;
    FilmFrame second;
  };
  constexpr std::array invalid_operations{
      InvalidFrameOperation{"negative clip start", false, -1, 10},
      InvalidFrameOperation{"empty clip range", false, 0, 0},
      InvalidFrameOperation{"clip beyond max frame", false, 0, MAX_FILM_FRAMES + 1},
      InvalidFrameOperation{"negative instance start", true, -1, 0},
      InvalidFrameOperation{"instance beyond max frame", true, MAX_FILM_FRAMES, 0},
  };
  for (const InvalidFrameOperation& item : invalid_operations) {
    const bool accepted = item.instance_operation
                              ? edits.moveInstance(*instance, item.first).has_value()
                              : edits.moveClip(*clip, item.first, item.second).has_value();
    if (accepted || timeline.findClip(*clip)->start_frame != 0 ||
        timeline.findClip(*clip)->end_frame != 10 ||
        timeline.findInstance(*instance)->start_frame != 0) {
      std::cerr << "invalid frame operation was not atomic: " << item.name << '\n';
      return false;
    }
  }
  if (edits.placeSequence(*sequence, 1)) {
    return false;
  }

  MovieTimeline camera_timeline;
  TimelineEditService camera_edits(camera_timeline);
  CapturedEntityBaseState camera_base;
  camera_base.camera = CapturedCameraState{};
  const auto camera_a =
      camera_edits.createSequence("Camera A", {TimelineTargetKind::Camera, {51}}, camera_base);
  const auto camera_b =
      camera_edits.createSequence("Camera B", {TimelineTargetKind::Camera, {52}}, camera_base);
  if (!camera_a || !camera_b || !camera_edits.appendClipToLane(*camera_a, 10, MovementClip{}) ||
      !camera_edits.appendClipToLane(*camera_b, 10, MovementClip{}) ||
      !camera_edits.placeSequence(*camera_a, 0) || !camera_edits.placeSequence(*camera_b, 5) ||
      !hasSeverity(validateMovieTimeline(camera_timeline), TimelineDiagnostic::Severity::Warning) ||
      !hasSeverity(validateMovieTimeline(camera_timeline, true),
                   TimelineDiagnostic::Severity::Error)) {
    return false;
  }

  struct DuplicateCase final {
    std::string_view name;
    TimelineTargetKind target;
    SequenceClipPayload payload;
  };
  const std::array duplicate_cases{
      DuplicateCase{"movement", TimelineTargetKind::RiggedEntity, MovementClip{}},
      DuplicateCase{"rig animation", TimelineTargetKind::RiggedEntity,
                    RigAnimationClip{.clip_id = 2}},
      DuplicateCase{"camera property", TimelineTargetKind::Camera,
                    PropertyClip{.kind = PropertyKind::CameraFov}},
  };
  for (const DuplicateCase& item : duplicate_cases) {
    MovieTimeline full_lane;
    TimelineEditService full_lane_edits(full_lane);
    const auto source =
        full_lane_edits.createSequence("Full lane", {item.target, {61}}, baseFor(item.target));
    const auto source_clip =
        source ? full_lane_edits.appendClipToLane(*source, MAX_FILM_FRAMES, item.payload)
               : std::expected<SequenceClipId, std::string>{std::unexpected("setup failed")};
    const SequenceClipId next_id = full_lane.next_clip_id;
    if (!source || !source_clip || full_lane_edits.duplicateClip(*source_clip) ||
        full_lane.sequences.front().clips.size() != 1 || full_lane.next_clip_id != next_id) {
      std::cerr << "duplicate payload was not atomic: " << item.name << '\n';
      return false;
    }
  }
  return true;
}

} // namespace

int main() {
  if (!testFrameRangesAndEvaluation()) {
    return fail("frame ranges, duration, or local/instance evaluation failed");
  }
  if (!testMovementEvaluation()) {
    return fail("movement continuity, explicit starts, curves, or transitions failed");
  }
  if (!testAnimationEvaluation()) {
    return fail("animation IDs, bind pose, trim, speed, looping, or final hold failed");
  }
  if (!testPropertyAndCameraGapEvaluation()) {
    return fail("camera, light, sun, or camera-gap evaluation failed");
  }
  if (!testSelectedCameraUsesOneEvaluationPath()) {
    return fail("selected camera and transform used different evaluation paths");
  }
  if (!testTimelineEdits()) {
    return fail("TimelineEditService compatibility, overlap, duplication, or atomicity failed");
  }
  return 0;
}
