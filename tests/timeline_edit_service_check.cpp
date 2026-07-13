#include "film/timeline_edit_service.hpp"

#include <iostream>
#include <limits>

namespace {

int fail(const char* parMessage) {
  std::cerr << parMessage << '\n';
  return 1;
}

}  // namespace

int main() {
  using namespace kage;
  using namespace kage::film;

  MovieTimeline timeline;
  TimelineEditService edits(timeline);
  CapturedEntityBaseState entity_base;
  const auto sequence =
      edits.createSequence("Actor", {TimelineTargetKind::RiggedEntity, {3}}, entity_base);
  if (!sequence.has_value()) {
    return fail("actor sequence creation failed");
  }
  MovementClip first_move;
  first_move.end.translation.x = 4.0f;
  const auto first_clip = edits.appendClipToLane(*sequence, 10, first_move);
  if (!first_clip.has_value()) {
    return fail("movement append failed");
  }
  const auto invalid_move = edits.moveClip(*first_clip, 0, 3601);
  if (invalid_move.has_value() || timeline.findClip(*first_clip)->end_frame != 10) {
    return fail("failed clip edits were not atomic or did not enforce frame 3600");
  }
  MovementClip inserted;
  inserted.end.translation.x = 2.0f;
  if (!edits.rippleInsertClip(*sequence, 0, 5, inserted).has_value() ||
      timeline.findClip(*first_clip)->start_frame != 5) {
    return fail("ripple insertion did not shift only later lane clips");
  }
  const auto placed = edits.placeSequence(*sequence, 0);
  if (!placed.has_value() || edits.duplicateInstance(*placed, 1).has_value()) {
    return fail("non-camera instance conflict rule failed");
  }

  CapturedEntityBaseState camera_base;
  camera_base.camera = CapturedCameraState{};
  const auto camera_sequence =
      edits.createSequence("Camera", {TimelineTargetKind::Camera, {8}}, camera_base);
  if (!camera_sequence.has_value() ||
      !edits.appendClipToLane(*camera_sequence, 10, MovementClip{}).has_value()) {
    return fail("camera setup failed");
  }
  const auto camera_first = edits.placeSequence(*camera_sequence, 0);
  const auto camera_second = edits.duplicateInstance(*camera_first, 5);
  if (!camera_first.has_value() || !camera_second.has_value() ||
      !edits.validateAuthoring().hasWarnings() || !edits.validateForBake().hasErrors()) {
    return fail("camera overlap warning and bake blocker rule failed");
  }
  if (!edits.deleteSequence(*sequence).has_value() ||
      !timeline.instances.empty() && timeline.instances.front().sequence_id == *sequence) {
    return fail("sequence deletion did not remove its instances");
  }

  MovieTimeline stale_id_timeline;
  TargetSequence seeded;
  seeded.id = 50;
  seeded.target = {TimelineTargetKind::RiggedEntity, {20}};
  seeded.captured_base = CapturedEntityBaseState{};
  seeded.clips.push_back({42, 0, 1, MovementClip{}});
  stale_id_timeline.sequences.push_back(seeded);
  stale_id_timeline.next_sequence_id = 1;
  stale_id_timeline.next_clip_id = 1;
  TimelineEditService stale_id_edits(stale_id_timeline);
  const auto stale_clip = stale_id_edits.appendClipToLane(50, 1, MovementClip{});
  const auto duplicate = stale_id_edits.duplicateSequence(50);
  if (!stale_clip.has_value() || *stale_clip != 43 || !duplicate.has_value() ||
      *duplicate != 51 || stale_id_timeline.next_clip_id != 46 ||
      stale_id_timeline.sequences.back().clips[0].id != 44 ||
      stale_id_timeline.sequences.back().clips[1].id != 45) {
    return fail("stale ID counters did not allocate globally unique IDs");
  }

  MovieTimeline boundary_timeline;
  TimelineEditService boundary_edits(boundary_timeline);
  const auto boundary_sequence = boundary_edits.createSequence(
      "Boundary", {TimelineTargetKind::RiggedEntity, {21}}, CapturedEntityBaseState{});
  const auto boundary_clip =
      boundary_edits.appendClipToLane(*boundary_sequence, MAX_FILM_FRAMES, MovementClip{});
  const auto boundary_instance = boundary_edits.placeSequence(*boundary_sequence, 0);
  if (!boundary_sequence.has_value() || !boundary_clip.has_value() ||
      !boundary_instance.has_value() || boundary_timeline.durationFrames() != MAX_FILM_FRAMES ||
      boundary_edits.duplicateInstance(*boundary_instance, 1).has_value()) {
    return fail("exact frame-3600 boundary was not handled correctly");
  }
  MovieTimeline overflow_timeline;
  TimelineEditService overflow_edits(overflow_timeline);
  const auto overflow_sequence = overflow_edits.createSequence(
      "Overflow", {TimelineTargetKind::RiggedEntity, {22}}, CapturedEntityBaseState{});
  if (!overflow_sequence.has_value() ||
      overflow_edits
          .appendClipToLane(*overflow_sequence,
                            std::numeric_limits<FilmFrame>::max(),
                            MovementClip{})
          .has_value() ||
      !overflow_timeline.findSequence(*overflow_sequence)->clips.empty()) {
    return fail("overflowing frame input was not rejected atomically");
  }

  MovieTimeline camera_overlap_timeline;
  TimelineEditService camera_overlap_edits(camera_overlap_timeline);
  CapturedEntityBaseState first_camera_base;
  first_camera_base.camera = CapturedCameraState{};
  first_camera_base.transform.translation.x = 1.0f;
  CapturedEntityBaseState second_camera_base = first_camera_base;
  second_camera_base.transform.translation.x = 2.0f;
  const auto first_camera = camera_overlap_edits.createSequence(
      "Camera A", {TimelineTargetKind::Camera, {30}}, first_camera_base);
  const auto second_camera = camera_overlap_edits.createSequence(
      "Camera B", {TimelineTargetKind::Camera, {31}}, second_camera_base);
  if (!first_camera.has_value() || !second_camera.has_value() ||
      !camera_overlap_edits.appendClipToLane(*first_camera, 10, MovementClip{}).has_value() ||
      !camera_overlap_edits.appendClipToLane(*second_camera, 10, MovementClip{}).has_value() ||
      !camera_overlap_edits.placeSequence(*first_camera, 0).has_value() ||
      !camera_overlap_edits.placeSequence(*second_camera, 0).has_value()) {
    return fail("camera-overlap setup failed");
  }
  const FilmFrameState overlap_frame = evaluateMovieTimeline(camera_overlap_timeline, 5);
  if (!overlap_frame.camera_output.camera.has_value() ||
      overlap_frame.camera_output.camera->source_entity.value != 31) {
    return fail("camera overlap was not resolved by greatest instance ID");
  }
  CapturedEntityBaseState later_camera_base = first_camera_base;
  later_camera_base.transform.translation.x = 3.0f;
  const auto later_camera = camera_overlap_edits.createSequence(
      "Camera C", {TimelineTargetKind::Camera, {32}}, later_camera_base);
  if (!later_camera.has_value() ||
      !camera_overlap_edits
           .appendClipToLane(*later_camera, 10, MovementClip{})
           .has_value() ||
      !camera_overlap_edits.placeSequence(*later_camera, 1).has_value()) {
    return fail("later camera-overlap setup failed");
  }
  const FilmFrameState later_overlap_frame =
      evaluateMovieTimeline(camera_overlap_timeline, 5);
  if (!later_overlap_frame.camera_output.camera.has_value() ||
      later_overlap_frame.camera_output.camera->source_entity.value != 32) {
    return fail("camera overlap was not resolved by greatest start frame");
  }

  MovieTimeline atomic_timeline;
  TimelineEditService atomic_edits(atomic_timeline);
  const auto atomic_sequence = atomic_edits.createSequence(
      "Atomic", {TimelineTargetKind::RiggedEntity, {40}}, CapturedEntityBaseState{});
  const auto atomic_clip = atomic_edits.appendClipToLane(*atomic_sequence, 100, MovementClip{});
  if (!atomic_sequence.has_value() || !atomic_clip.has_value() ||
      !atomic_edits.moveClip(*atomic_clip, 3500, 3600).has_value() ||
      atomic_edits.rippleInsertClip(*atomic_sequence, 3400, 200, MovementClip{}).has_value() ||
      atomic_timeline.findClip(*atomic_clip)->start_frame != 3500) {
    return fail("failed ripple insertion was not atomic");
  }
  MovieTimeline instance_timeline;
  TimelineEditService instance_edits(instance_timeline);
  const auto instance_sequence = instance_edits.createSequence(
      "Instance", {TimelineTargetKind::RiggedEntity, {41}}, CapturedEntityBaseState{});
  const auto instance_clip =
      instance_edits.appendClipToLane(*instance_sequence, 100, MovementClip{});
  const auto atomic_instance = instance_edits.placeSequence(*instance_sequence, 3500);
  if (!instance_sequence.has_value() || !instance_clip.has_value() ||
      !atomic_instance.has_value() ||
      instance_edits.moveInstance(*atomic_instance, 3501).has_value() ||
      instance_timeline.instances.front().start_frame != 3500) {
    return fail("failed instance move was not atomic");
  }

  MovieTimeline target_timeline;
  TimelineEditService target_edits(target_timeline);
  CapturedEntityBaseState strict_camera_base;
  strict_camera_base.camera = CapturedCameraState{};
  const auto strict_camera = target_edits.createSequence(
      "Strict Camera", {TimelineTargetKind::Camera, {50}}, strict_camera_base);
  const auto strict_rig = target_edits.createSequence(
      "Strict Rig", {TimelineTargetKind::RiggedEntity, {52}},
      CapturedEntityBaseState{});
  RigAnimationClip invalid_source_range;
  invalid_source_range.source_out = 1.1f;
  RigAnimationClip invalid_speed;
  invalid_speed.speed = std::numeric_limits<float>::quiet_NaN();
  if (!strict_camera.has_value() ||
      !strict_rig.has_value() ||
      target_edits.appendClipToLane(*strict_camera, 1, RigAnimationClip{}).has_value() ||
      target_edits.appendClipToLane(*strict_rig, 1, invalid_source_range).has_value() ||
      target_edits.appendClipToLane(*strict_rig, 1, invalid_speed).has_value() ||
      !target_timeline.findSequence(*strict_rig)->clips.empty() ||
      target_edits.createSequence("Invalid Sun", {TimelineTargetKind::Sun, {51}},
                                  CapturedSunBaseState{})
          .has_value()) {
    return fail("target classification did not enforce valid authoring lanes");
  }
  return 0;
}
