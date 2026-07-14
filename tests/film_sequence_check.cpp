#include "animation/animation_system.hpp"
#include "film/movie_timeline.hpp"
#include "film/timeline_edit_service.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {

bool close(float parLeft, float parRight) {
  return std::abs(parLeft - parRight) < 0.001f;
}

bool close(const glm::vec3& parLeft, const glm::vec3& parRight) {
  return close(parLeft.x, parRight.x) && close(parLeft.y, parRight.y) &&
         close(parLeft.z, parRight.z);
}

int fail(const char* parMessage) {
  std::cerr << parMessage << '\n';
  return 1;
}

}  // namespace

int main() {
  using namespace kage;
  using namespace kage::film;

  MovieTimeline empty_timeline;
  FilmPlayback empty_playback;
  empty_playback.playing = true;
  empty_playback.previewing = true;
  empty_playback.update(1.0f, empty_timeline.durationFrames());
  if (empty_playback.playing || empty_playback.previewing ||
      empty_playback.playhead_frame != 0.0) {
    return fail("zero-duration playback did not stop cleanly");
  }
  FilmPlayback bounded_playback;
  bounded_playback.playing = true;
  bounded_playback.previewing = true;
  bounded_playback.update(2.0f, 90);
  if (!bounded_playback.playing || bounded_playback.playhead_frame != 60.0) {
    return fail("playback did not honor its supplied duration");
  }
  FilmPlayback stopped_playback;
  if (!requiresFilmFrameState(true, true, -1.0, stopped_playback) ||
      !requiresFilmFrameState(true, false, 0.0, stopped_playback) ||
      requiresFilmFrameState(true, false, -1.0, stopped_playback)) {
    return fail("camera preview or export did not select the film evaluation path");
  }

  MovieTimeline timeline;
  TimelineEditService edits(timeline);
  CapturedEntityBaseState base;
  base.transform.translation.x = 10.0f;
  base.camera = CapturedCameraState{55.0f, 0.2f, 400.0f};
  const auto sequence = edits.createSequence("Camera", {TimelineTargetKind::Camera, {7}}, base);
  if (!sequence.has_value()) {
    return fail("camera sequence creation failed");
  }

  MovementClip movement;
  movement.end.translation.x = 20.0f;
  const auto movement_id = edits.appendClipToLane(*sequence, 10, movement);
  PropertyClip fov;
  fov.kind = PropertyKind::CameraFov;
  fov.start_value = fov.control_1 = glm::vec4(55.0f);
  fov.end_value = fov.control_2 = glm::vec4(65.0f);
  const auto fov_id = edits.appendClipToLane(*sequence, 10, fov);
  if (!movement_id.has_value() || !fov_id.has_value()) {
    return fail("independent movement and FOV lanes were not accepted");
  }
  const auto first = edits.placeSequence(*sequence, 5);
  const auto second = edits.duplicateInstance(*first);
  if (!first.has_value() || !second.has_value() ||
      timeline.instances.back().start_frame != 15 ||
      timeline.durationFrames() != 25) {
    return fail("sequence reuse did not derive movie duration");
  }

  const FilmFrameState middle = evaluateMovieTimeline(timeline, 10);
  if (middle.transforms.size() != 1 || !close(middle.transforms[0].transform.translation.x, 15.0f) ||
      !middle.camera_output.camera.has_value() ||
      !close(middle.camera_output.camera->vertical_fov_degrees, 60.0f)) {
    return fail("local sequence evaluation failed");
  }
  const std::optional<FilmFrameState> camera_preview =
      evaluateTargetSequencePreview(timeline, *sequence, 5);
  if (!camera_preview.has_value() ||
      !camera_preview->camera_output.camera.has_value() ||
      !close(camera_preview->camera_output.camera->transform.translation.x,
             15.0f) ||
      !close(camera_preview->camera_output.camera->vertical_fov_degrees,
             60.0f) ||
      evaluateTargetSequencePreview(timeline, 9999, 0).has_value()) {
    return fail("camera sequence preview did not evaluate the selected sequence");
  }
  CapturedEntityBaseState actor_base;
  actor_base.transform.translation.x = 2.0f;
  const auto actor_sequence = edits.createSequence(
      "Actor", {TimelineTargetKind::RiggedEntity, {2}}, actor_base);
  MovementClip actor_movement;
  actor_movement.end.translation.x = 12.0f;
  const auto actor_clip = actor_sequence.has_value()
                              ? edits.appendClipToLane(*actor_sequence, 10,
                                                       actor_movement)
                              : std::expected<SequenceClipId, std::string>{
                                    std::unexpected("setup failed")};
  const std::optional<FilmFrameState> actor_preview =
      actor_sequence.has_value()
          ? evaluateTargetSequencePreview(timeline, *actor_sequence, 5)
          : std::nullopt;
  if (!actor_clip.has_value() || !actor_preview.has_value() ||
      actor_preview->transforms.size() != 1 ||
      !close(actor_preview->transforms.front().transform.translation.x, 7.0f) ||
      actor_preview->camera_output.kind != FilmOutputKind::Black) {
    return fail("non-camera sequence preview did not evaluate in local time");
  }
  const FilmFrameState reset = evaluateMovieTimeline(timeline, 15);
  if (reset.transforms.empty() ||
      !close(reset.transforms[0].transform.translation.x, 10.0f)) {
    return fail("instance local time did not reset");
  }
  const FilmFrameState held = evaluateMovieTimeline(timeline, 25);
  if (!held.camera_output.camera.has_value() ||
      !close(held.camera_output.camera->transform.translation.x, 20.0f)) {
    return fail("half-open end or camera hold failed");
  }
  timeline.camera_gap_mode = CameraGapMode::Black;
  if (evaluateMovieTimeline(timeline, 25).camera_output.kind != FilmOutputKind::Black) {
    return fail("black camera gaps were not respected");
  }
  if (evaluateMovieTimeline(timeline, 25).camera_output.kind != FilmOutputKind::Black) {
    return fail("exclusive instance end was not respected");
  }
  if (timeline.sequences[0].captured_base.index() != 0 ||
      !close(std::get<CapturedEntityBaseState>(timeline.sequences[0].captured_base)
                 .transform.translation.x,
             10.0f)) {
    return fail("captured base state was unexpectedly changed");
  }

  MovieTimeline movement_timeline;
  TimelineEditService movement_edits(movement_timeline);
  const auto movement_sequence = movement_edits.createSequence(
      "Rig", {TimelineTargetKind::RiggedEntity, {9}}, CapturedEntityBaseState{});
  MovementClip first_movement;
  first_movement.end.translation.x = 10.0f;
  MovementClip second_movement;
  second_movement.end.translation.x = 110.0f;
  const auto first_movement_id =
      movement_edits.appendClipToLane(*movement_sequence, 10, first_movement);
  const auto second_movement_id =
      movement_edits.appendClipToLane(*movement_sequence, 10, second_movement);
  const std::optional<FilmFrameState> continuity_state =
      evaluateTargetSequencePreview(movement_timeline, *movement_sequence, 10);
  if (!continuity_state.has_value() || continuity_state->transforms.empty() ||
      !close(continuity_state->transforms.front().transform.translation.x, 10.0f)) {
    return fail("previous-endpoint movement continuity failed");
  }
  math::Transform explicit_start;
  explicit_start.translation.x = 100.0f;
  if (!movement_sequence.has_value() || !first_movement_id.has_value() ||
      !second_movement_id.has_value() ||
      !movement_edits.setMovementStartMode(*second_movement_id,
                                            MovementStartMode::ExplicitPosition,
                                            explicit_start)
           .has_value()) {
    return fail("movement setup failed");
  }
  const auto movement_instance = movement_edits.placeSequence(*movement_sequence, 0);
  if (!movement_instance.has_value() ||
      !close(evaluateMovieTimeline(movement_timeline, 10)
                 .transforms.front()
                 .transform.translation.x,
             100.0f)) {
    return fail("explicit movement start was not preserved");
  }
  if (!movement_edits.moveClip(*second_movement_id, 20, 30).has_value() ||
      !movement_edits.setMovementTransition(*second_movement_id,
                                              MovementTransition{true, {}})
           .has_value() ||
      !close(evaluateMovieTimeline(movement_timeline, 15)
                 .transforms.front()
                 .transform.translation.x,
             55.0f)) {
    return fail("movement transition sampling failed");
  }
  const TargetSequence* resolved_sequence =
      movement_timeline.findSequence(*movement_sequence);
  const std::optional<ResolvedMovementPath> first_path =
      resolveMovementPath(*resolved_sequence, *first_movement_id);
  const std::optional<ResolvedMovementPath> second_path =
      resolveMovementPath(*resolved_sequence, *second_movement_id);
  if (!first_path.has_value() || first_path->transition_before.has_value() ||
      !close(first_path->movement.start, glm::vec3(0.0f)) ||
      !close(first_path->movement.control_1,
             glm::vec3(10.0f / 3.0f, 0.0f, 0.0f)) ||
      !close(first_path->movement.end, glm::vec3(10.0f, 0.0f, 0.0f)) ||
      !second_path.has_value() || !second_path->transition_before.has_value() ||
      !close(second_path->movement.start, glm::vec3(100.0f, 0.0f, 0.0f)) ||
      !close(second_path->movement.end, glm::vec3(110.0f, 0.0f, 0.0f)) ||
      !close(second_path->transition_before->start,
             glm::vec3(10.0f, 0.0f, 0.0f)) ||
      !close(second_path->transition_before->control_1,
             glm::vec3(40.0f, 0.0f, 0.0f)) ||
      !close(second_path->transition_before->control_2,
             glm::vec3(70.0f, 0.0f, 0.0f)) ||
      !close(second_path->transition_before->end,
             glm::vec3(100.0f, 0.0f, 0.0f)) ||
      resolveMovementPath(*resolved_sequence, 999999).has_value()) {
    return fail("movement path resolution did not match authored geometry");
  }

  MovementClip manual_movement = std::get<MovementClip>(
      movement_timeline.findClip(*second_movement_id)->payload);
  manual_movement.curve.automatic_position_controls = false;
  manual_movement.curve.position_control_1 = {101.0f, 2.0f, 3.0f};
  manual_movement.curve.position_control_2 = {109.0f, 5.0f, 6.0f};
  if (!movement_edits
           .setClipPayload(*second_movement_id, manual_movement)
           .has_value()) {
    return fail("manual movement path setup failed");
  }
  const std::optional<ResolvedMovementPath> manual_path = resolveMovementPath(
      *movement_timeline.findSequence(*movement_sequence), *second_movement_id);
  if (!manual_path.has_value() ||
      !close(manual_path->movement.control_1,
             manual_movement.curve.position_control_1) ||
      !close(manual_path->movement.control_2,
             manual_movement.curve.position_control_2)) {
    return fail("manual movement controls were not preserved in the path");
  }

  TargetSequence adjacent_sequence =
      *movement_timeline.findSequence(*movement_sequence);
  for (SequenceClip& clip : adjacent_sequence.clips) {
    if (clip.id == *second_movement_id) {
      clip.start_frame = 10;
      clip.end_frame = 20;
    }
  }
  const std::optional<ResolvedMovementPath> adjacent_path =
      resolveMovementPath(adjacent_sequence, *second_movement_id);
  if (!adjacent_path.has_value() || adjacent_path->transition_before.has_value()) {
    return fail("movement path exposed a transition without an in-between gap");
  }

  TargetSequence disabled_transition_sequence =
      *movement_timeline.findSequence(*movement_sequence);
  TargetSequence inherited_start_sequence = disabled_transition_sequence;
  TargetSequence no_predecessor_sequence = disabled_transition_sequence;
  for (SequenceClip& clip : disabled_transition_sequence.clips) {
    if (clip.id == *second_movement_id) {
      std::get<MovementClip>(clip.payload).transition_before.enabled = false;
    }
  }
  for (SequenceClip& clip : inherited_start_sequence.clips) {
    if (clip.id == *second_movement_id) {
      MovementClip& movement = std::get<MovementClip>(clip.payload);
      movement.start_mode = MovementStartMode::PreviousEndpoint;
      movement.explicit_start.reset();
    }
  }
  std::erase_if(no_predecessor_sequence.clips,
                [first_id = *first_movement_id](const SequenceClip& clip) {
                  return clip.id == first_id;
                });
  const auto hasTransition = [second_id = *second_movement_id](
                                 const TargetSequence& sequence) {
    const std::optional<ResolvedMovementPath> path =
        resolveMovementPath(sequence, second_id);
    return path.has_value() && path->transition_before.has_value();
  };
  if (hasTransition(disabled_transition_sequence) ||
      hasTransition(inherited_start_sequence) ||
      hasTransition(no_predecessor_sequence)) {
    return fail("movement path exposed an ineligible transition");
  }

  MovieTimeline animation_timeline;
  TimelineEditService animation_edits(animation_timeline);
  const auto animation_sequence = animation_edits.createSequence(
      "Animated", {TimelineTargetKind::RiggedEntity, {10}}, CapturedEntityBaseState{});
  RigAnimationClip animation;
  animation.clip_id = 5;
  animation.source_in = 0.2f;
  animation.source_out = 0.4f;
  animation.speed = 0.5f;
  const auto animation_id = animation_edits.appendClipToLane(*animation_sequence, 10, animation);
  if (!animation_sequence.has_value() || !animation_id.has_value() ||
      !animation_edits.placeSequence(*animation_sequence, 0).has_value()) {
    return fail("animation setup failed");
  }
  const FilmFrameState animation_mid = evaluateMovieTimeline(animation_timeline, 6);
  const FilmFrameState animation_hold = evaluateMovieTimeline(animation_timeline, 10);
  const FilmFrameState animation_late_hold =
      evaluateMovieTimeline(animation_timeline, 30);
  if (animation_mid.rig_animations.size() != 1 ||
      animation_mid.rig_animations.front().final_pose ||
      !close(animation_mid.rig_animations.front().local_time_seconds,
             6.0f / 30.0f * 0.5f) ||
      animation_hold.rig_animations.size() != 1 ||
      !animation_hold.rig_animations.front().final_pose ||
      !close(animation_hold.rig_animations.front().local_time_seconds,
             10.0f / 30.0f * 0.5f) ||
      animation_late_hold.rig_animations.size() != 1 ||
      !close(animation_late_hold.rig_animations.front().local_time_seconds,
             animation_hold.rig_animations.front().local_time_seconds)) {
    return fail("animation trim, speed, or final-pose hold failed");
  }
  MovieTimeline loop_timeline;
  TimelineEditService loop_edits(loop_timeline);
  const auto loop_sequence = loop_edits.createSequence(
      "Loop", {TimelineTargetKind::RiggedEntity, {11}}, CapturedEntityBaseState{});
  animation.looping = true;
  animation.speed = 1.0f;
  const auto loop_id = loop_edits.appendClipToLane(*loop_sequence, 20, animation);
  if (!loop_sequence.has_value() || !loop_id.has_value() ||
      !loop_edits.placeSequence(*loop_sequence, 0).has_value() ||
      !close(evaluateMovieTimeline(loop_timeline, 9)
                 .rig_animations.front()
                 .local_time_seconds,
             0.3f)) {
    return fail("looping animation sampling failed");
  }
  const FilmFrameState loop_hold = evaluateMovieTimeline(loop_timeline, 20);
  const FilmFrameState loop_late_hold = evaluateMovieTimeline(loop_timeline, 50);
  if (loop_hold.rig_animations.size() != 1 ||
      loop_late_hold.rig_animations.size() != 1 ||
      !close(loop_hold.rig_animations.front().local_time_seconds,
             20.0f / 30.0f) ||
      !close(loop_late_hold.rig_animations.front().local_time_seconds,
             loop_hold.rig_animations.front().local_time_seconds)) {
    return fail("looping animation final-pose hold failed");
  }

  assets::ModelAsset animation_asset;
  animation_asset.animation_clips = {{10, "First"}, {20, "Second"}};
  const RigAnimationPlayback stable_animation{.clip_id = 20};
  const RigAnimationPlayback missing_animation{.clip_id = 999};
  if (animation::resolveAnimationClipIndex(animation_asset, stable_animation) != 1 ||
      animation::resolveAnimationClipIndex(animation_asset, missing_animation)
          .has_value()) {
    return fail("animation playback did not resolve only stable IDs");
  }
  MovieTimeline bind_pose_timeline;
  TargetSequence bind_pose_sequence;
  bind_pose_sequence.id = 1;
  bind_pose_sequence.target = {TimelineTargetKind::RiggedEntity, {13}};
  bind_pose_sequence.clips.push_back({1, 10, 20, RigAnimationClip{.clip_id = 20}});
  bind_pose_timeline.sequences.push_back(std::move(bind_pose_sequence));
  bind_pose_timeline.instances.push_back({1, 1, 0});
  if (!evaluateMovieTimeline(bind_pose_timeline, 0).rig_animations.empty()) {
    return fail("rig animation override was active before its clip");
  }

  MovieTimeline property_timeline;
  TimelineEditService property_edits(property_timeline);
  CapturedEntityBaseState light_base;
  light_base.point_light = CapturedPointLightState{};
  const auto light_sequence = property_edits.createSequence(
      "Light", {TimelineTargetKind::PointLight, {12}}, light_base);
  PropertyClip intensity;
  intensity.kind = PropertyKind::PointLightIntensity;
  intensity.start_value = intensity.control_1 = glm::vec4(1.0f);
  intensity.end_value = intensity.control_2 = glm::vec4(7.0f);
  if (!light_sequence.has_value() ||
      !property_edits.appendClipToLane(*light_sequence, 10, intensity).has_value() ||
      !property_edits.placeSequence(*light_sequence, 0).has_value()) {
    return fail("point-light property setup failed");
  }
  const FilmFrameState light_hold = evaluateMovieTimeline(property_timeline, 10);
  const auto held_intensity = std::find_if(
      light_hold.properties.begin(), light_hold.properties.end(),
      [](const PropertyOverride& property) {
        return property.kind == FilmPropertyKind::LightIntensity;
      });
  if (held_intensity == light_hold.properties.end() ||
      !close(held_intensity->value.x, 7.0f)) {
    return fail("point-light property hold failed");
  }
  MovieTimeline sun_timeline;
  TimelineEditService sun_edits(sun_timeline);
  const auto sun_sequence = sun_edits.createSequence(
      "Sun", {TimelineTargetKind::Sun, {}}, CapturedSunBaseState{});
  PropertyClip sun_intensity;
  sun_intensity.kind = PropertyKind::SunIntensity;
  sun_intensity.start_value = sun_intensity.control_1 = glm::vec4(1.0f);
  sun_intensity.end_value = sun_intensity.control_2 = glm::vec4(3.0f);
  if (!sun_sequence.has_value() ||
      !sun_edits.appendClipToLane(*sun_sequence, 10, sun_intensity).has_value() ||
      !sun_edits.placeSequence(*sun_sequence, 0).has_value() ||
      !evaluateMovieTimeline(sun_timeline, 10).sun.has_value() ||
      !close(evaluateMovieTimeline(sun_timeline, 10).sun->intensity, 3.0f)) {
    return fail("sun property hold failed");
  }

  return 0;
}
