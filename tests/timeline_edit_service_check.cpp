#include "film/timeline_edit_service.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

int fail(const char* parMessage) {
  std::cerr << parMessage << '\n';
  return 1;
}

[[nodiscard]] bool hasDiagnostic(
    const kage::film::TimelineValidation& parValidation,
    kage::film::TimelineDiagnostic::Severity parSeverity,
    std::string_view parMessage) {
  return std::any_of(
      parValidation.diagnostics.begin(), parValidation.diagnostics.end(),
      [parSeverity, parMessage](const kage::film::TimelineDiagnostic& diagnostic) {
        return diagnostic.severity == parSeverity && diagnostic.message == parMessage;
      });
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
  const auto placed = edits.placeSequence(*sequence, 0);
  if (!placed.has_value() || edits.placeSequence(*sequence, 1).has_value()) {
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
  const auto camera_second = edits.placeSequence(*camera_sequence, 5);
  const TimelineValidation camera_authoring = validateMovieTimeline(timeline);
  const TimelineValidation camera_bake = validateMovieTimeline(timeline, true);
  if (!camera_first.has_value() || !camera_second.has_value() ||
      !hasDiagnostic(camera_authoring, TimelineDiagnostic::Severity::Warning,
                     "Camera instances overlap") ||
      !hasDiagnostic(camera_bake, TimelineDiagnostic::Severity::Error,
                     "Camera instances overlap")) {
    return fail("camera overlap warning and bake blocker rule failed");
  }
  MovieTimeline cross_camera_timeline;
  TimelineEditService cross_camera_edits(cross_camera_timeline);
  CapturedEntityBaseState other_camera_base;
  other_camera_base.camera = CapturedCameraState{};
  const auto cross_camera_first = cross_camera_edits.createSequence(
      "Camera A", {TimelineTargetKind::Camera, {81}}, camera_base);
  const auto cross_camera_second = cross_camera_edits.createSequence(
      "Camera B", {TimelineTargetKind::Camera, {82}}, other_camera_base);
  if (!cross_camera_first.has_value() || !cross_camera_second.has_value() ||
      !cross_camera_edits
           .appendClipToLane(*cross_camera_first, 10, MovementClip{})
           .has_value() ||
      !cross_camera_edits
           .appendClipToLane(*cross_camera_second, 10, MovementClip{})
           .has_value() ||
      !cross_camera_edits.placeSequence(*cross_camera_first, 0).has_value() ||
      !cross_camera_edits.placeSequence(*cross_camera_second, 5).has_value()) {
    return fail("overlap between different cameras did not block Bake");
  }
  const TimelineValidation cross_camera_authoring =
      validateMovieTimeline(cross_camera_timeline);
  const TimelineValidation cross_camera_bake =
      validateMovieTimeline(cross_camera_timeline, true);
  if (!hasDiagnostic(cross_camera_authoring,
                     TimelineDiagnostic::Severity::Warning,
                     "Camera instances overlap") ||
      !hasDiagnostic(cross_camera_bake, TimelineDiagnostic::Severity::Error,
                     "Camera instances overlap")) {
    return fail("overlap between different cameras did not block Bake");
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
  const auto stale_instance = stale_id_edits.placeSequence(50, 5);
  const auto duplicate = stale_id_edits.duplicateSequence(50);
  if (!stale_clip.has_value() || *stale_clip != 43 ||
      !stale_instance.has_value() || !duplicate.has_value() ||
      *duplicate != 51 || stale_id_timeline.next_clip_id != 46 ||
      stale_id_timeline.sequences.back().clips[0].id != 44 ||
      stale_id_timeline.sequences.back().clips[1].id != 45) {
    return fail("stale ID counters did not allocate globally unique IDs");
  }
  if (stale_id_timeline.sequences.size() != 2 ||
      stale_id_timeline.sequences.front().id != 50 ||
      stale_id_timeline.sequences.front().clips[0].id != 42 ||
      stale_id_timeline.sequences.front().clips[1].id != 43 ||
      stale_id_timeline.sequences.back().target !=
          stale_id_timeline.sequences.front().target ||
      stale_id_timeline.instances.size() != 1 ||
      stale_id_timeline.instances.front().id != *stale_instance ||
      stale_id_timeline.instances.front().sequence_id != 50 ||
      stale_id_timeline.instances.front().start_frame != 5) {
    return fail("sequence duplication mutated its source or existing instances");
  }
  MovementClip changed_duplicate_clip;
  changed_duplicate_clip.end.translation.x = 9.0f;
  if (!stale_id_edits.setClipPayload(44, changed_duplicate_clip).has_value() ||
      !std::holds_alternative<MovementClip>(
          stale_id_timeline.findClip(42)->payload) ||
      std::get<MovementClip>(stale_id_timeline.findClip(42)->payload)
              .end.translation.x != 0.0f ||
      std::get<MovementClip>(stale_id_timeline.findClip(44)->payload)
              .end.translation.x != 9.0f) {
    return fail("sequence duplication did not create independent clip data");
  }

  {
    MovieTimeline instance_duplicate_timeline;
    TimelineEditService instance_duplicate_edits(instance_duplicate_timeline);
    CapturedEntityBaseState instance_camera_base;
    instance_camera_base.camera = CapturedCameraState{};
    const auto instance_sequence = instance_duplicate_edits.createSequence(
        "Instance source", {TimelineTargetKind::Camera, {23}},
        instance_camera_base);
    const auto instance_clip = instance_sequence.has_value()
                                   ? instance_duplicate_edits.appendClipToLane(
                                         *instance_sequence, 10, MovementClip{})
                                   : std::expected<SequenceClipId, std::string>{
                                         std::unexpected("setup failed")};
    const auto instance_source = instance_sequence.has_value()
                                     ? instance_duplicate_edits.placeSequence(
                                           *instance_sequence, 3)
                                     : std::expected<SequenceInstanceId,
                                                     std::string>{
                                           std::unexpected("setup failed")};
    const auto instance_copy = instance_source.has_value()
                                   ? instance_duplicate_edits.duplicateInstance(
                                         *instance_source)
                                   : std::expected<SequenceInstanceId,
                                                   std::string>{
                                         std::unexpected("setup failed")};
    if (!instance_sequence.has_value() || !instance_clip.has_value() ||
        !instance_source.has_value() || !instance_copy.has_value() ||
        instance_duplicate_timeline.instances.size() != 2 ||
        *instance_copy == *instance_source ||
        instance_duplicate_timeline.next_instance_id != 3 ||
        instance_duplicate_timeline.instances[0].id != *instance_source ||
        instance_duplicate_timeline.instances[0].sequence_id != *instance_sequence ||
        instance_duplicate_timeline.instances[0].start_frame != 3 ||
        instance_duplicate_timeline.instances[1].id != *instance_copy ||
        instance_duplicate_timeline.instances[1].sequence_id != *instance_sequence ||
        instance_duplicate_timeline.instances[1].start_frame != 13) {
      return fail("instance duplication did not create one independent placement");
    }
  }

  {
    MovieTimeline duplicate_placement_timeline;
    TimelineEditService duplicate_placement_edits(duplicate_placement_timeline);
    const auto duplicate_sequence = duplicate_placement_edits.createSequence(
        "Placement", {TimelineTargetKind::RiggedEntity, {24}},
        CapturedEntityBaseState{});
    const auto duplicate_clip = duplicate_sequence.has_value()
                                    ? duplicate_placement_edits.appendClipToLane(
                                          *duplicate_sequence, 10,
                                          MovementClip{})
                                    : std::expected<SequenceClipId, std::string>{
                                          std::unexpected("setup failed")};
    const auto source = duplicate_sequence.has_value()
                            ? duplicate_placement_edits.placeSequence(
                                  *duplicate_sequence, 0)
                            : std::expected<SequenceInstanceId, std::string>{
                                  std::unexpected("setup failed")};
    const auto blocking = duplicate_sequence.has_value()
                              ? duplicate_placement_edits.placeSequence(
                                    *duplicate_sequence, 15)
                              : std::expected<SequenceInstanceId, std::string>{
                                    std::unexpected("setup failed")};
    const auto duplicate = source.has_value()
                               ? duplicate_placement_edits.duplicateInstance(*source)
                               : std::expected<SequenceInstanceId, std::string>{
                                     std::unexpected("setup failed")};
    if (!duplicate_sequence.has_value() || !duplicate_clip.has_value() ||
        !source.has_value() || !blocking.has_value() || !duplicate.has_value() ||
        duplicate_placement_timeline.instances.back().start_frame != 25) {
      return fail("instance duplication did not use the first valid frame after the source");
    }
  }

  {
    MovieTimeline clip_duplicate_timeline;
    TimelineEditService clip_duplicate_edits(clip_duplicate_timeline);
    const auto rig_sequence = clip_duplicate_edits.createSequence(
        "Clip copy", {TimelineTargetKind::RiggedEntity, {25}},
        CapturedEntityBaseState{});
    MovementClip source_movement;
    source_movement.end.translation.x = 4.0f;
    const auto movement_source = rig_sequence.has_value()
                                     ? clip_duplicate_edits.appendClipToLane(
                                           *rig_sequence, 10, source_movement)
                                     : std::expected<SequenceClipId, std::string>{
                                           std::unexpected("setup failed")};
    const auto movement_blocker = rig_sequence.has_value()
                                      ? clip_duplicate_edits.appendClipToLane(
                                            *rig_sequence, 10, MovementClip{})
                                      : std::expected<SequenceClipId, std::string>{
                                            std::unexpected("setup failed")};
    const auto moved_blocker = movement_blocker.has_value()
                                   ? clip_duplicate_edits.moveClip(
                                         *movement_blocker, 15, 25)
                                   : std::expected<void, std::string>{
                                         std::unexpected("setup failed")};
    const auto movement_copy = movement_source.has_value()
                                   ? clip_duplicate_edits.duplicateClip(*movement_source)
                                   : std::expected<SequenceClipId, std::string>{
                                         std::unexpected("setup failed")};
    if (!rig_sequence.has_value() || !movement_source.has_value() ||
        !movement_blocker.has_value() || !moved_blocker.has_value() ||
        !movement_copy.has_value() || *movement_copy == *movement_source ||
        clip_duplicate_timeline.findClip(*movement_source)->start_frame != 0 ||
        clip_duplicate_timeline.findClip(*movement_source)->end_frame != 10 ||
        clip_duplicate_timeline.findClip(*movement_blocker)->start_frame != 15 ||
        clip_duplicate_timeline.findClip(*movement_blocker)->end_frame != 25 ||
        clip_duplicate_timeline.findClip(*movement_copy)->start_frame != 25 ||
        clip_duplicate_timeline.findClip(*movement_copy)->end_frame != 35 ||
        !std::holds_alternative<MovementClip>(
            clip_duplicate_timeline.findClip(*movement_copy)->payload)) {
      return fail("clip duplication did not use the first free range in its lane");
    }
    MovementClip changed_movement =
        std::get<MovementClip>(clip_duplicate_timeline.findClip(*movement_copy)->payload);
    changed_movement.end.translation.x = 9.0f;
    if (!clip_duplicate_edits.setClipPayload(*movement_copy, changed_movement).has_value() ||
        std::get<MovementClip>(
            clip_duplicate_timeline.findClip(*movement_source)->payload)
                .end.translation.x != 4.0f ||
        std::get<MovementClip>(
            clip_duplicate_timeline.findClip(*movement_copy)->payload)
                .end.translation.x != 9.0f) {
      return fail("clip duplication did not create independent movement data");
    }

    RigAnimationClip source_animation;
    source_animation.clip_id = 19;
    source_animation.source_in = 0.2f;
    source_animation.source_out = 0.8f;
    source_animation.speed = 1.5f;
    const auto animation_source = rig_sequence.has_value()
                                      ? clip_duplicate_edits.appendClipToLane(
                                            *rig_sequence, 7, source_animation)
                                      : std::expected<SequenceClipId, std::string>{
                                            std::unexpected("setup failed")};
    const auto animation_copy = animation_source.has_value()
                                    ? clip_duplicate_edits.duplicateClip(*animation_source)
                                    : std::expected<SequenceClipId, std::string>{
                                          std::unexpected("setup failed")};
    const auto* copied_animation = animation_copy.has_value()
                                       ? std::get_if<RigAnimationClip>(
                                             &clip_duplicate_timeline.findClip(
                                                  *animation_copy)->payload)
                                       : nullptr;
    if (!animation_source.has_value() || !animation_copy.has_value() ||
        copied_animation == nullptr || copied_animation->clip_id != 19 ||
        copied_animation->source_in != 0.2f || copied_animation->source_out != 0.8f ||
        copied_animation->speed != 1.5f ||
        clip_duplicate_timeline.findClip(*animation_copy)->start_frame != 7 ||
        clip_duplicate_timeline.findClip(*animation_copy)->end_frame != 14) {
      return fail("clip duplication did not preserve animation data");
    }

    CapturedEntityBaseState camera_base;
    camera_base.camera = CapturedCameraState{};
    const auto camera_sequence = clip_duplicate_edits.createSequence(
        "Property copy", {TimelineTargetKind::Camera, {26}}, camera_base);
    PropertyClip source_property;
    source_property.kind = PropertyKind::CameraFov;
    source_property.end_value.x = 70.0f;
    const auto property_source = camera_sequence.has_value()
                                     ? clip_duplicate_edits.appendClipToLane(
                                           *camera_sequence, 5, source_property)
                                     : std::expected<SequenceClipId, std::string>{
                                           std::unexpected("setup failed")};
    const auto property_copy = property_source.has_value()
                                   ? clip_duplicate_edits.duplicateClip(*property_source)
                                   : std::expected<SequenceClipId, std::string>{
                                         std::unexpected("setup failed")};
    const auto* copied_property = property_copy.has_value()
                                      ? std::get_if<PropertyClip>(
                                            &clip_duplicate_timeline.findClip(
                                                 *property_copy)->payload)
                                      : nullptr;
    if (!camera_sequence.has_value() || !property_source.has_value() ||
        !property_copy.has_value() || copied_property == nullptr ||
        copied_property->kind != PropertyKind::CameraFov ||
        copied_property->end_value.x != 70.0f ||
        clip_duplicate_timeline.findClip(*property_copy)->start_frame != 5 ||
        clip_duplicate_timeline.findClip(*property_copy)->end_frame != 10 ||
        !clip_duplicate_edits.deleteClip(*property_copy).has_value() ||
        clip_duplicate_timeline.findClip(*property_copy) != nullptr ||
        clip_duplicate_timeline.findClip(*property_source) == nullptr) {
      return fail("property clip duplication or deletion failed");
    }

    MovieTimeline full_lane_timeline;
    TimelineEditService full_lane_edits(full_lane_timeline);
    const auto full_lane_sequence = full_lane_edits.createSequence(
        "Full lane", {TimelineTargetKind::RiggedEntity, {27}},
        CapturedEntityBaseState{});
    const auto full_lane_clip = full_lane_sequence.has_value()
                                    ? full_lane_edits.appendClipToLane(
                                          *full_lane_sequence, MAX_FILM_FRAMES,
                                          MovementClip{})
                                    : std::expected<SequenceClipId, std::string>{
                                          std::unexpected("setup failed")};
    const SequenceClipId next_clip_id_before_failure = full_lane_timeline.next_clip_id;
    if (!full_lane_sequence.has_value() || !full_lane_clip.has_value() ||
        full_lane_edits.duplicateClip(*full_lane_clip).has_value() ||
        full_lane_timeline.sequences.front().clips.size() != 1 ||
        full_lane_timeline.next_clip_id != next_clip_id_before_failure) {
      return fail("failed clip duplication was not atomic");
    }
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
      boundary_edits.duplicateInstance(*boundary_instance).has_value()) {
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
  const auto editable_movement =
      strict_rig.has_value()
          ? target_edits.appendClipToLane(*strict_rig, 1, MovementClip{})
          : std::expected<SequenceClipId, std::string>{
                std::unexpected("strict rig setup failed")};
  MovementClip edited_movement;
  edited_movement.end.translation.x = 7.0f;
  PropertyClip incompatible_property;
  incompatible_property.kind = PropertyKind::CameraFov;
  if (!strict_camera.has_value() ||
      !strict_rig.has_value() ||
      !target_edits.setCameraGapMode(CameraGapMode::Black).has_value() ||
      target_timeline.camera_gap_mode != CameraGapMode::Black ||
      !editable_movement.has_value() ||
      !target_edits.setClipPayload(*editable_movement, edited_movement).has_value() ||
      target_edits.setClipPayload(*editable_movement, incompatible_property).has_value() ||
      !std::holds_alternative<MovementClip>(
          target_timeline.findClip(*editable_movement)->payload) ||
      std::get<MovementClip>(target_timeline.findClip(*editable_movement)->payload)
              .end.translation.x != 7.0f ||
      target_edits.appendClipToLane(*strict_camera, 1, RigAnimationClip{}).has_value() ||
      target_edits.appendClipToLane(*strict_rig, 1, invalid_source_range).has_value() ||
      target_edits.appendClipToLane(*strict_rig, 1, invalid_speed).has_value() ||
      target_timeline.findSequence(*strict_rig)->clips.size() != 1 ||
      target_edits.createSequence("Invalid Sun", {TimelineTargetKind::Sun, {51}},
                                  CapturedSunBaseState{})
          .has_value()) {
    return fail("clip payload edits were not atomic or did not enforce authoring lanes");
  }

  return 0;
}
