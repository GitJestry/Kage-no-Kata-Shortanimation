#include "editor/movie_editor_controller.hpp"
#include "assets/asset_registry.hpp"
#include "film/movie_timeline_world_validation.hpp"
#include "film/timeline_edit_service.hpp"

#include <algorithm>
#include <iostream>
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
        return diagnostic.severity == parSeverity &&
               diagnostic.message.find(parMessage) != std::string::npos;
      });
}

}  // namespace

int main() {
  using namespace kage;

  scene::EntityRecord camera;
  camera.id = {1};
  camera.camera = scene::CameraComponent{};
  const auto camera_target = editor::movieTargetForEntity(camera);
  if (!camera_target.has_value() ||
      camera_target->kind != film::TimelineTargetKind::Camera) {
    return fail("Movie target filtering rejected a camera");
  }

  scene::EntityRecord point_light;
  point_light.id = {2};
  point_light.light = scene::LightComponent{};
  const auto light_target = editor::movieTargetForEntity(point_light);
  if (!light_target.has_value() ||
      light_target->kind != film::TimelineTargetKind::PointLight) {
    return fail("Movie target filtering rejected a point light");
  }

  scene::EntityRecord rigged;
  rigged.id = {3};
  rigged.rig = scene::RigComponent{};
  const auto rig_target = editor::movieTargetForEntity(rigged);
  if (!rig_target.has_value() ||
      rig_target->kind != film::TimelineTargetKind::RiggedEntity) {
    return fail("Movie target filtering rejected a rigged entity");
  }

  scene::EntityRecord static_mesh;
  static_mesh.id = {4};
  static_mesh.static_mesh = scene::StaticMeshComponent{};
  scene::EntityRecord spot_light;
  spot_light.id = {5};
  spot_light.light = scene::LightComponent{};
  spot_light.light->type = scene::LightType::Spot;
  scene::EntityRecord deleted_camera = camera;
  deleted_camera.alive = false;
  if (editor::movieTargetForEntity(static_mesh).has_value() ||
      editor::movieTargetForEntity(spot_light).has_value() ||
      editor::movieTargetForEntity(deleted_camera).has_value()) {
    return fail("Movie target filtering accepted an invalid target");
  }

  editor::EditorSession session;
  if (editor::clampMovieAuthoringCursor(-1) != 0 ||
      editor::clampMovieAuthoringCursor(film::MAX_FILM_FRAMES) !=
          film::MAX_FILM_FRAMES ||
      editor::clampMovieAuthoringCursor(film::MAX_FILM_FRAMES + 1) !=
          film::MAX_FILM_FRAMES) {
    return fail("Movie authoring cursor boundaries were not kept separate from playback");
  }
  film::FilmPlayback cursor_playback;
  editor::setMovieAuthoringCursor(session, cursor_playback, 30, 30);
  if (session.authoring_cursor_frame != 30 ||
      cursor_playback.playhead_frame != 29.0 || cursor_playback.playing ||
      !cursor_playback.previewing || session.shot_preview ||
      session.shot_preview_sequence_id != 0 ||
      !film::requiresFilmFrameState(true, session.shot_preview, -1.0,
                                    cursor_playback)) {
    return fail("scrubbing did not activate paused Movie preview");
  }
  editor::setMovieAuthoringCursor(session, cursor_playback, 0,
                                  film::MAX_FILM_FRAMES + 1);
  if (session.authoring_cursor_frame != film::MAX_FILM_FRAMES ||
      cursor_playback.playhead_frame != 0.0 || cursor_playback.previewing) {
    return fail("zero-duration playback accepted an authoring cursor frame");
  }

  session.movie_selection.target = *camera_target;
  session.movie_selection.sequence_id = 7;
  session.movie_selection.clip_id = 8;
  session.movie_selection.instance_id = 9;
  film::FilmPlayback deselect_playback;
  deselect_playback.playing = true;
  deselect_playback.previewing = true;
  deselect_playback.playhead_frame = 17.0;
  session.shot_preview = true;
  session.shot_preview_sequence_id = 77;
  editor::resetMoviePreview(session, deselect_playback);
  editor::deselectMovieTarget(session);
  if (session.movie_selection.target.has_value() ||
      session.movie_selection.sequence_id != 0 ||
      session.movie_selection.clip_id != 0 ||
      session.movie_selection.instance_id != 0 ||
      deselect_playback.playing || deselect_playback.previewing ||
      session.shot_preview || session.shot_preview_sequence_id != 0 ||
      deselect_playback.playhead_frame != 17.0) {
    return fail("explicit Movie deselect did not completely reset preview state");
  }
  editor::selectCreatedFilmCamera(session, {6}, 70, 80);
  const film::TimelineTarget created_camera_target{
      film::TimelineTargetKind::Camera, {6}};
  if (session.movie_selection.target != created_camera_target ||
      session.movie_selection.sequence_id != 70 ||
      session.movie_selection.clip_id != 0 ||
      session.movie_selection.instance_id != 80) {
    return fail("new film camera did not become the Movie selection");
  }
  editor::deselectMovieTarget(session);

  film::MovieTimeline navigation_timeline;
  film::TargetSequence navigation_sequence;
  navigation_sequence.id = 17;
  navigation_sequence.name = "Camera Sequence";
  navigation_sequence.target = *camera_target;
  navigation_timeline.sequences.push_back(navigation_sequence);
  navigation_timeline.instances.push_back({19, navigation_sequence.id, 12});
  editor::selectMovieTarget(session, navigation_timeline, *camera_target);
  if (session.movie_selection.target != camera_target ||
      session.movie_selection.sequence_id != navigation_sequence.id ||
      session.movie_selection.clip_id != 0 ||
      session.movie_selection.instance_id != 0) {
    return fail("target selection did not open its Target Sequence Timeline");
  }
  editor::selectMovieInstance(session, 19);
  if (session.movie_selection.target.has_value() ||
      session.movie_selection.sequence_id != 0 ||
      session.movie_selection.instance_id != 19) {
    return fail("movie instance selection did not preserve Movie Inspector context");
  }
  editor::deselectMovieTarget(session);

  film::FilmPlayback playback;
  film::TargetSequence camera_sequence;
  camera_sequence.id = (film::TargetSequenceId{1} << 40U) + 42;
  camera_sequence.target = *camera_target;
  camera_sequence.clips.push_back({1, 0, 90, film::MovementClip{}});
  camera_sequence.clips.push_back({3, 0, 90, film::PropertyClip{}});
  film::TargetSequence rig_sequence;
  rig_sequence.id = 43;
  rig_sequence.target = *rig_target;
  rig_sequence.clips.push_back({2, 0, 30, film::MovementClip{}});

  film::MovieTimeline preview_timeline;
  preview_timeline.sequences.push_back(camera_sequence);
  preview_timeline.sequences.push_back(rig_sequence);
  preview_timeline.instances.push_back({1, rig_sequence.id, 0});

  editor::selectMovieSequence(session, preview_timeline.sequences.front());
  film::MovementClip early_movement;
  early_movement.end.translation = {10.0f, 0.0f, 0.0f};
  film::MovementClip late_movement;
  late_movement.start_mode = film::MovementStartMode::ExplicitPosition;
  late_movement.explicit_start = math::Transform{};
  late_movement.explicit_start->translation = {100.0f, 0.0f, 0.0f};
  late_movement.end.translation = {110.0f, 0.0f, 0.0f};
  late_movement.transition_before.enabled = true;
  film::TargetSequence path_sequence;
  path_sequence.id = 44;
  path_sequence.target = *camera_target;
  path_sequence.clips.push_back({12, 20, 30, late_movement});
  path_sequence.clips.push_back({13, 0, 30, film::PropertyClip{}});
  path_sequence.clips.push_back({11, 0, 10, early_movement});
  const auto early_path = film::resolveMovementPath(path_sequence, 11);
  const auto late_path = film::resolveMovementPath(path_sequence, 12);
  if (!early_path.has_value() || !late_path.has_value() ||
      early_path->movement.start.x != 0.0f ||
      early_path->movement.end.x != 10.0f ||
      early_path->transition_before.has_value() ||
      late_path->movement.start.x != 100.0f ||
      late_path->movement.end.x != 110.0f ||
      std::abs(late_path->movement.control_1.x -
               (100.0f + 10.0f / 3.0f)) > 0.001f ||
      std::abs(late_path->movement.control_2.x -
               (110.0f - 10.0f / 3.0f)) > 0.001f ||
      !late_path->transition_before.has_value() ||
      late_path->transition_before->start.x != 10.0f ||
      late_path->transition_before->end.x != 100.0f ||
      std::abs(late_path->transition_before->control_1.x - 40.0f) > 0.001f ||
      std::abs(late_path->transition_before->control_2.x - 70.0f) > 0.001f ||
      !editor::movementTransitionAvailable(path_sequence,
                                           path_sequence.clips.front()) ||
      editor::movementTransitionAvailable(path_sequence,
                                          path_sequence.clips.back()) ||
      film::resolveMovementPath(path_sequence, 13).has_value() ||
      film::resolveMovementPath(path_sequence, 999999).has_value()) {
    return fail("movement paths were not resolved chronologically");
  }

  session.movie_selection.clip_id = 0;
  playback.previewing = true;
  editor::updateMoviePreviewContext(session, preview_timeline, playback);
  if (!session.shot_preview ||
      session.shot_preview_sequence_id != camera_sequence.id ||
      editor::moviePreviewDuration(session, preview_timeline) != 90) {
    return fail("camera Target Sequence did not become the active preview context");
  }

  editor::selectMovieSequence(session, preview_timeline.sequences.back());
  editor::updateMoviePreviewContext(session, preview_timeline, playback);
  if (session.shot_preview ||
      session.shot_preview_sequence_id != rig_sequence.id ||
      editor::moviePreviewDuration(session, preview_timeline) != 30) {
    return fail("non-camera Target Sequence did not use local time and editor view");
  }

  session.movie_selection.sequence_id = 999999;
  if (editor::moviePreviewDuration(session, preview_timeline) != 0) {
    return fail("invalid Target Sequence preview fell back to master time");
  }
  editor::selectMovieSequence(session, preview_timeline.sequences.front());

  playback.playing = false;
  playback.previewing = false;
  playback.playhead_frame = 900.0;
  if (!editor::toggleMoviePlayback(session, preview_timeline, playback) ||
      !playback.playing || !playback.previewing ||
      playback.playhead_frame != 0.0 || !session.shot_preview ||
      session.shot_preview_sequence_id != camera_sequence.id) {
    return fail("Target Sequence Play retained a stale master playhead");
  }

  playback.playing = true;
  playback.previewing = true;
  playback.playhead_frame = 0.0;
  playback.update(2.0f, editor::moviePreviewDuration(session, preview_timeline));
  if (playback.playhead_frame != 60.0 || !playback.playing) {
    return fail("camera-sequence playback did not loop at the selected duration");
  }
  editor::synchronizeMovieAuthoringCursor(session, playback);
  if (session.authoring_cursor_frame != 60) {
    return fail("visible Movie playhead did not follow playback");
  }

  editor::deselectMovieTarget(session);
  editor::updateMoviePreviewContext(session, preview_timeline, playback);
  if (session.shot_preview || session.shot_preview_sequence_id != 0 ||
      editor::moviePreviewDuration(session, preview_timeline) != 30) {
    return fail("Movie Timeline did not restore master preview context");
  }

  film::MovieTimeline zero_preview_timeline;
  film::TargetSequence zero_camera = camera_sequence;
  zero_camera.id += 1;
  zero_camera.clips.clear();
  zero_preview_timeline.sequences.push_back(zero_camera);
  editor::selectMovieSequence(session, zero_preview_timeline.sequences.front());
  playback.playing = false;
  playback.previewing = true;
  session.shot_preview = true;
  session.shot_preview_sequence_id = zero_camera.id;
  editor::updateMoviePreviewContext(session, zero_preview_timeline, playback);
  if (playback.playing || playback.previewing || session.shot_preview ||
      session.shot_preview_sequence_id != 0 ||
      editor::moviePreviewDuration(session, zero_preview_timeline) != 0) {
    return fail("selecting an empty sequence did not keep the normal viewport");
  }
  playback.playhead_frame = 0.0;
  if (editor::toggleMoviePlayback(session, zero_preview_timeline, playback) ||
      playback.playing || playback.previewing ||
      playback.playhead_frame != 0.0) {
    return fail("zero-duration Target Sequence preview started playback");
  }

  scene::World world;
  const scene::EntityId live_entity = world.createEntityWithId("Live", {70});
  const film::TimelineTarget live_target{
      film::TimelineTargetKind::RiggedEntity, live_entity};
  const film::TimelineTarget orphan_target{
      film::TimelineTargetKind::RiggedEntity, {71}};

  film::MovieTimeline orphan_timeline;
  film::TimelineEditService orphan_edits(orphan_timeline);
  const auto orphan_sequence = orphan_edits.createSequence(
      "Orphan", orphan_target, film::CapturedEntityBaseState{});
  const auto orphan_clip = orphan_sequence.has_value()
                               ? orphan_edits.appendClipToLane(
                                     *orphan_sequence, 10,
                                     film::MovementClip{})
                               : std::expected<film::SequenceClipId,
                                               std::string>{
                                     std::unexpected("setup failed")};
  const auto orphan_instance = orphan_sequence.has_value()
                                   ? orphan_edits.placeSequence(
                                         *orphan_sequence, 0)
                                   : std::expected<film::SequenceInstanceId,
                                                   std::string>{
                                         std::unexpected("setup failed")};
  if (!orphan_sequence.has_value() || !orphan_clip.has_value() ||
      !orphan_instance.has_value()) {
    return fail("orphan sequence test setup failed");
  }
  editor::selectMovieSequence(
      session, *orphan_timeline.findSequence(*orphan_sequence));
  if (session.movie_selection.target != orphan_target ||
      session.movie_selection.sequence_id != *orphan_sequence) {
    return fail("orphan sequence selection did not open its inspector context");
  }
  const film::TimelineValidation orphan_authoring =
      film::validateMovieTimelineWithWorld(orphan_timeline, world);
  const film::TimelineValidation orphan_bake =
      film::validateMovieTimelineWithWorld(orphan_timeline, world, true);
  if (!hasDiagnostic(orphan_authoring,
                     film::TimelineDiagnostic::Severity::Warning,
                     "A sequence targets an entity that no longer exists") ||
      orphan_authoring.hasErrors() ||
      !hasDiagnostic(orphan_bake, film::TimelineDiagnostic::Severity::Error,
                     "An orphaned sequence has instances")) {
    return fail("placed orphan sequence did not block Bake");
  }
  if (!orphan_edits.deleteSequence(*orphan_sequence).has_value() ||
      !orphan_timeline.sequences.empty() ||
      !orphan_timeline.instances.empty()) {
    return fail("orphan deletion did not remove all associated instances");
  }

  film::MovieTimeline incompatible_timeline;
  film::TimelineEditService incompatible_edits(incompatible_timeline);
  const auto incompatible_sequence = incompatible_edits.createSequence(
      "Stale rig target", live_target, film::CapturedEntityBaseState{});
  if (!incompatible_sequence.has_value() ||
      !incompatible_edits.appendClipToLane(*incompatible_sequence, 10,
                                            film::MovementClip{})
           .has_value() ||
      !incompatible_edits.placeSequence(*incompatible_sequence, 0).has_value()) {
    return fail("incompatible target validation setup failed");
  }
  const film::TimelineValidation incompatible_authoring =
      film::validateMovieTimelineWithWorld(incompatible_timeline, world);
  const film::TimelineValidation incompatible_bake =
      film::validateMovieTimelineWithWorld(incompatible_timeline, world, true);
  if (!hasDiagnostic(incompatible_authoring,
                     film::TimelineDiagnostic::Severity::Warning,
                     "no longer compatible") ||
      !hasDiagnostic(incompatible_bake, film::TimelineDiagnostic::Severity::Error,
                     "no longer compatible") ||
      !incompatible_bake.hasErrors()) {
    return fail("incompatible World target was not reported and blocked for Bake");
  }

  scene::World animation_world;
  const scene::EntityId animated_entity =
      animation_world.createEntityWithId("Animated", {90});
  animation_world.setRig(animated_entity, scene::RigComponent{});
  assets::AssetRegistry assets;
  assets::ModelAsset animated_asset;
  animated_asset.animation_clips.push_back({.id = 7001, .name = "ArmAction"});
  const std::size_t asset_index = assets.registerModelAsset(
      "Animated", "assets/models/animated.glb", std::move(animated_asset));
  scene::StaticMeshComponent animated_mesh;
  animated_mesh.asset_library_index = asset_index;
  animation_world.setStaticMesh(animated_entity, animated_mesh);
  const film::TimelineTarget animated_target{
      film::TimelineTargetKind::RiggedEntity, animated_entity};
  film::MovieTimeline animation_timeline;
  film::TimelineEditService animation_edits(animation_timeline);
  const auto animation_sequence = animation_edits.createSequence(
      "Animated sequence", animated_target, film::CapturedEntityBaseState{});
  film::RigAnimationClip missing_animation;
  missing_animation.clip_id = 9001;
  if (!animation_sequence.has_value() ||
      !animation_edits.appendClipToLane(*animation_sequence, 10,
                                         missing_animation)
           .has_value() ||
      !animation_edits.placeSequence(*animation_sequence, 0).has_value()) {
    return fail("animation asset validation setup failed");
  }
  const film::TimelineValidation missing_animation_authoring =
      film::validateMovieTimelineWithWorld(animation_timeline, animation_world,
                                           false, &assets);
  const film::TimelineValidation missing_animation_bake =
      film::validateMovieTimelineWithWorld(animation_timeline, animation_world,
                                           true, &assets);
  if (!hasDiagnostic(missing_animation_authoring,
                     film::TimelineDiagnostic::Severity::Warning,
                     "unavailable animation") ||
      !hasDiagnostic(missing_animation_bake,
                     film::TimelineDiagnostic::Severity::Error,
                     "missing or incompatible animation") ||
      !missing_animation_bake.hasErrors()) {
    return fail("missing animation asset was not reported and blocked for Bake");
  }

  playback.playing = true;
  playback.previewing = true;
  playback.playhead_frame = 23.0;
  session.shot_preview = true;
  session.shot_preview_sequence_id = rig_sequence.id;
  editor::resetMoviePreview(session, playback);
  if (playback.playing || playback.previewing || playback.playhead_frame != 23.0 ||
      session.shot_preview || session.shot_preview_sequence_id != 0 ||
      film::requiresFilmFrameState(true, session.shot_preview, -1.0,
                                   playback)) {
    return fail("Movie preview reset left active playback state behind");
  }
  return 0;
}
