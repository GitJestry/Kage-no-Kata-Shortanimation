#include "assets/asset_registry.hpp"
#include "editor/movie_editor_controller.hpp"
#include "engine/film_camera_creation.hpp"
#include "engine/film_viewport.hpp"
#include "film/movie_timeline_world_validation.hpp"
#include "film/timeline_edit_service.hpp"
#include "math/screen_projection.hpp"
#include "render/viewport_picking.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <string_view>

namespace {

using namespace kage;
using namespace kage::film;

[[nodiscard]] bool close(float parLeft, float parRight) {
  return std::abs(parLeft - parRight) < 0.001f;
}

[[nodiscard]] scene::EntityRecord entity(std::uint32_t parId) {
  scene::EntityRecord result;
  result.id = {parId};
  return result;
}

[[nodiscard]] bool placeClip(MovieTimeline& parTimeline, std::string_view parName,
                             TimelineTarget parTarget,
                             CapturedTargetBaseState parBase,
                             SequenceClipPayload parPayload) {
  TimelineEditService edits(parTimeline);
  const auto sequence = edits.createSequence(std::string(parName), parTarget,
                                             std::move(parBase));
  return sequence && edits.appendClipToLane(*sequence, 10, std::move(parPayload)) &&
         edits.placeSequence(*sequence, 0);
}

[[nodiscard]] bool orphanBakeFails() {
  MovieTimeline timeline;
  return placeClip(timeline, "Orphan", {TimelineTargetKind::RiggedEntity, {71}},
                   CapturedEntityBaseState{}, MovementClip{}) &&
         validateMovieTimelineWithWorld(timeline, scene::World{}, true).hasErrors();
}

[[nodiscard]] bool incompatibleBakeFails() {
  scene::World world;
  const scene::EntityId entity = world.createEntityWithId("Live", {72});
  MovieTimeline timeline;
  return placeClip(timeline, "Incompatible", {TimelineTargetKind::RiggedEntity, entity},
                   CapturedEntityBaseState{}, MovementClip{}) &&
         validateMovieTimelineWithWorld(timeline, world, true).hasErrors();
}

[[nodiscard]] bool missingAnimationBakeFails() {
  scene::World world;
  const scene::EntityId entity = world.createEntityWithId("Animated", {73});
  world.setRig(entity, scene::RigComponent{});
  assets::AssetRegistry assets;
  assets::ModelAsset asset;
  asset.animation_clips.push_back({.id = 7001, .name = "ArmAction"});
  const std::size_t index = assets.registerModelAsset(
      "Animated", "assets/models/animated.glb", std::move(asset));
  scene::StaticMeshComponent mesh;
  mesh.asset_library_index = index;
  world.setStaticMesh(entity, mesh);
  MovieTimeline timeline;
  return placeClip(timeline, "Missing animation",
                   {TimelineTargetKind::RiggedEntity, entity},
                   CapturedEntityBaseState{}, RigAnimationClip{.clip_id = 9001}) &&
         validateMovieTimelineWithWorld(timeline, world, true, &assets).hasErrors();
}

int fail(std::string_view parMessage) {
  std::cerr << parMessage << '\n';
  return 1;
}

[[nodiscard]] bool testTargetClassification() {
  scene::EntityRecord camera = entity(1);
  camera.camera = scene::CameraComponent{};
  scene::EntityRecord point_light = entity(2);
  point_light.light = scene::LightComponent{};
  scene::EntityRecord rigged = entity(3);
  rigged.rig = scene::RigComponent{};
  scene::EntityRecord static_mesh = entity(4);
  static_mesh.static_mesh = scene::StaticMeshComponent{};
  scene::EntityRecord spot_light = entity(5);
  spot_light.light = scene::LightComponent{.type = scene::LightType::Spot};
  scene::EntityRecord deleted_camera = camera;
  deleted_camera.alive = false;
  struct TargetCase final {
    std::string_view name;
    scene::EntityRecord entity;
    std::optional<TimelineTargetKind> expected;
  };
  const std::array cases{
      TargetCase{"camera", camera, TimelineTargetKind::Camera},
      TargetCase{"point light", point_light, TimelineTargetKind::PointLight},
      TargetCase{"rig", rigged, TimelineTargetKind::RiggedEntity},
      TargetCase{"static mesh", static_mesh, std::nullopt},
      TargetCase{"spot light", spot_light, std::nullopt},
      TargetCase{"deleted camera", deleted_camera, std::nullopt},
  };
  for (const TargetCase& item : cases) {
    const auto target = editor::movieTargetForEntity(item.entity);
    if (target.has_value() != item.expected.has_value() ||
        (target && target->kind != *item.expected)) {
      std::cerr << "Movie target classification failed: " << item.name << '\n';
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool testSelectionAndPreviewState() {
  const TimelineTarget camera_target{TimelineTargetKind::Camera, {2}};
  const TimelineTarget rig_target{TimelineTargetKind::RiggedEntity, {3}};
  TargetSequence camera_sequence;
  camera_sequence.id = 17;
  camera_sequence.target = camera_target;
  camera_sequence.clips.push_back({11, 0, 90, MovementClip{}});
  TargetSequence rig_sequence;
  rig_sequence.id = 43;
  rig_sequence.target = rig_target;
  rig_sequence.clips.push_back({12, 0, 30, MovementClip{}});
  MovieTimeline timeline;
  timeline.sequences = {camera_sequence, rig_sequence};
  timeline.instances.push_back({19, rig_sequence.id, 0});

  editor::EditorSession session;
  editor::selectMovieTarget(session, timeline, camera_target);
  if (session.movie_selection.target != camera_target ||
      session.movie_selection.sequence_id != camera_sequence.id ||
      session.movie_selection.clip_id != 0 || session.movie_selection.instance_id != 0) {
    return false;
  }
  session.movie_selection.clip_id = camera_sequence.clips.front().id;
  editor::selectMovieSequence(session, rig_sequence);
  if (session.movie_selection.target != rig_target ||
      session.movie_selection.sequence_id != rig_sequence.id ||
      session.movie_selection.clip_id != 0 || session.movie_selection.instance_id != 0) {
    return false;
  }
  editor::selectMovieInstance(session, 19);
  if (session.movie_selection.target || session.movie_selection.sequence_id != 0 ||
      session.movie_selection.clip_id != 0 || session.movie_selection.instance_id != 19) {
    return false;
  }
  editor::deselectMovieTarget(session);
  if (session.movie_selection.target || session.movie_selection.sequence_id != 0 ||
      session.movie_selection.clip_id != 0 || session.movie_selection.instance_id != 0) {
    return false;
  }

  FilmPlayback playback;
  editor::setMovieAuthoringCursor(session, playback, 30, 30);
  if (session.authoring_cursor_frame != 30 || playback.playhead_frame != 29.0 ||
      playback.playing || !playback.previewing) {
    return false;
  }
  editor::setMovieAuthoringCursor(session, playback, 0, MAX_FILM_FRAMES + 1);
  if (session.authoring_cursor_frame != MAX_FILM_FRAMES ||
      playback.playhead_frame != 0.0 || playback.previewing) {
    return false;
  }

  editor::selectMovieSequence(session, camera_sequence);
  playback.previewing = true;
  editor::updateMoviePreviewContext(session, timeline, playback);
  if (!session.shot_preview || session.shot_preview_sequence_id != camera_sequence.id ||
      editor::moviePreviewDuration(session, timeline) != 90) {
    return false;
  }
  editor::selectMovieSequence(session, rig_sequence);
  editor::updateMoviePreviewContext(session, timeline, playback);
  if (session.shot_preview || session.shot_preview_sequence_id != rig_sequence.id ||
      editor::moviePreviewDuration(session, timeline) != 30) {
    return false;
  }
  playback.playhead_frame = 900.0;
  playback.previewing = false;
  if (!editor::toggleMoviePlayback(session, timeline, playback) || !playback.playing ||
      !playback.previewing || playback.playhead_frame != 0.0) {
    return false;
  }
  playback.update(0.5f, editor::moviePreviewDuration(session, timeline));
  editor::synchronizeMovieAuthoringCursor(session, playback);
  if (!playback.playing || playback.playhead_frame != 15.0 ||
      session.authoring_cursor_frame != 15) {
    return false;
  }
  editor::deselectMovieTarget(session);
  editor::updateMoviePreviewContext(session, timeline, playback);
  if (session.shot_preview || session.shot_preview_sequence_id != 0 ||
      editor::moviePreviewDuration(session, timeline) != 30) {
    return false;
  }
  playback.playhead_frame = 23.0;
  playback.playing = true;
  playback.previewing = true;
  session.shot_preview = true;
  session.shot_preview_sequence_id = rig_sequence.id;
  editor::resetMoviePreview(session, playback);
  if (playback.playing || playback.previewing || playback.playhead_frame != 23.0 ||
      session.shot_preview || session.shot_preview_sequence_id != 0) {
    return false;
  }

  MovieTimeline empty_timeline;
  if (editor::toggleMoviePlayback(session, empty_timeline, playback) ||
      playback.playing || playback.previewing || playback.playhead_frame != 0.0) {
    return false;
  }
  return true;
}

[[nodiscard]] bool testFilmCameraCreation() {
  scene::SceneManager scenes;
  const std::size_t index = scenes.createScene("Movie test");
  scene::SceneManager::SceneRecord* scene = scenes.getScene(index);
  math::Transform transform;
  transform.translation = {3.0f, 4.0f, 5.0f};
  const CapturedCameraState camera{62.0f, 0.1f, 500.0f};
  const auto created = engine::createFilmCameraAtomically(*scene, transform, camera, 0);
  if (!created || !scene->world.findEntity(created->entity) ||
      !scene->world.findEntity(created->entity)->camera) {
    return false;
  }
  const TargetSequence* sequence =
      scene->movie_timeline.findSequence(created->sequence_id);
  const SequenceInstance* instance =
      scene->movie_timeline.findInstance(created->instance_id);
  editor::EditorSession selection;
  editor::selectCreatedFilmCamera(selection, created->entity, created->sequence_id,
                                  created->instance_id);
  return sequence != nullptr && instance != nullptr &&
         sequence->target == TimelineTarget{TimelineTargetKind::Camera, created->entity} &&
         sequence->clips.size() == 2 && instance->sequence_id == sequence->id &&
         instance->start_frame == 0 &&
         selection.movie_selection.target == sequence->target &&
         selection.movie_selection.sequence_id == sequence->id &&
         selection.movie_selection.clip_id == 0 &&
         selection.movie_selection.instance_id == instance->id;
}

[[nodiscard]] bool testViewportAndFrameConsistency() {
  scene::World world;
  const scene::EntityId first_camera = world.createEntity("First Camera");
  const scene::EntityId second_camera = world.createEntity("Second Camera");
  world.setCamera(first_camera, scene::CameraComponent{});
  world.setCamera(second_camera, scene::CameraComponent{});
  MovieTimeline timeline;
  TimelineEditService edits(timeline);
  CapturedEntityBaseState first_base;
  first_base.transform.translation = {0.0f, 0.0f, 10.0f};
  first_base.camera = CapturedCameraState{40.0f, 0.1f, 300.0f};
  CapturedEntityBaseState second_base;
  second_base.transform.translation = {3.0f, 1.0f, 8.0f};
  second_base.camera = CapturedCameraState{70.0f, 0.2f, 600.0f};
  const auto first = edits.createSequence(
      "First", {TimelineTargetKind::Camera, first_camera}, first_base);
  const auto second = edits.createSequence(
      "Second", {TimelineTargetKind::Camera, second_camera}, second_base);
  if (!first || !second || !edits.appendClipToLane(*first, 10, MovementClip{}) ||
      !edits.appendClipToLane(*second, 10, MovementClip{}) ||
      !edits.placeSequence(*first, 0) || !edits.placeSequence(*second, 10)) {
    return false;
  }
  FilmPlayback playback;
  playback.previewing = true;
  const FilmFrameState preview = evaluateMovieTimeline(timeline, 10);
  const FilmFrameState exported = evaluateMovieTimeline(timeline, 10);
  if (!requiresFilmFrameState(true, false, -1.0, playback) ||
      !requiresFilmFrameState(true, false, 10.0, FilmPlayback{}) ||
      !preview.camera_output.camera || !exported.camera_output.camera ||
      preview.camera_output.camera->source_entity != second_camera ||
      exported.camera_output.camera->source_entity != second_camera ||
      !close(preview.camera_output.camera->vertical_fov_degrees,
             exported.camera_output.camera->vertical_fov_degrees)) {
    return false;
  }
  camera::Camera editor_camera;
  editor_camera.position = {-4.0f, 5.0f, 12.0f};
  const engine::FilmViewportCamera camera_view =
      engine::resolveFilmViewportCamera(editor_camera, world, &preview);
  const engine::FilmViewportCamera stopped_view =
      engine::resolveFilmViewportCamera(editor_camera, world, nullptr);
  FilmFrameState non_camera_state;
  const engine::FilmViewportCamera non_camera_view =
      engine::resolveFilmViewportCamera(editor_camera, world, &non_camera_state, false);
  return camera_view.consumes_film_state && !camera_view.black_output &&
         camera_view.camera &&
         close(camera_view.camera->position.x, second_base.transform.translation.x) &&
         !engine::shouldShowEditorOverlays(true, camera_view, true) &&
         !stopped_view.consumes_film_state && stopped_view.camera &&
         stopped_view.camera->position == editor_camera.position &&
         engine::shouldShowEditorOverlays(true, stopped_view, true) &&
         non_camera_view.consumes_film_state && non_camera_view.camera &&
         non_camera_view.camera->position == editor_camera.position &&
         engine::shouldShowEditorOverlays(true, non_camera_view, false);
}

[[nodiscard]] bool testEvaluatedViewportPicking() {
  scene::World world;
  const scene::EntityId actor = world.createEntity("Animated Actor");
  world.setRig(actor, scene::RigComponent{});
  FilmFrameState state;
  math::Transform evaluated;
  evaluated.translation = {3.0f, 0.0f, 0.0f};
  state.transforms.push_back({actor, evaluated});
  camera::Camera camera;
  camera.position = {0.0f, 0.0f, 10.0f};
  const glm::vec2 viewport{1000.0f, 1000.0f};
  const glm::mat4 view_projection = camera.getViewProjectionMatrix(viewport);
  const math::ScreenPoint evaluated_pixel =
      math::projectPoint(evaluated.translation, view_projection, viewport);
  const math::ScreenPoint authored_pixel =
      math::projectPoint(glm::vec3(0.0f), view_projection, viewport);
  return evaluated_pixel.valid && authored_pixel.valid &&
         render::pickViewportEntityBounds(world, &camera, &state,
                                          evaluated_pixel.pixel, viewport, 0.35f) == actor &&
         !render::pickViewportEntityBounds(world, &camera, &state,
                                            authored_pixel.pixel, viewport, 0.35f) &&
         !render::pickViewportEntityBounds(world, nullptr, &state,
                                            evaluated_pixel.pixel, viewport, 0.35f);
}

[[nodiscard]] bool testBakeFailures() {
  struct BakeFailureCase final {
    std::string_view name;
    bool (*fails)();
  };
  const std::array cases{
      BakeFailureCase{"orphan target", orphanBakeFails},
      BakeFailureCase{"incompatible target", incompatibleBakeFails},
      BakeFailureCase{"missing animation", missingAnimationBakeFails},
  };
  for (const BakeFailureCase& item : cases) {
    if (!item.fails()) {
      std::cerr << "Bake accepted failure category: " << item.name << '\n';
      return false;
    }
  }
  return true;
}

}  // namespace

int main() {
  if (!testTargetClassification()) {
    return fail("valid Movie target classification failed");
  }
  if (!testSelectionAndPreviewState()) {
    return fail("target, sequence, clip, instance, scrub, playback, Stop, or empty-timeline workflow failed");
  }
  if (!testFilmCameraCreation()) {
    return fail("Film Camera creation failed");
  }
  if (!testViewportAndFrameConsistency()) {
    return fail("camera/non-camera preview or preview/export FilmFrameState consistency failed");
  }
  if (!testEvaluatedViewportPicking()) {
    return fail("evaluated viewport picking failed");
  }
  if (!testBakeFailures()) {
    return fail("orphan, incompatible target, or missing-animation Bake validation failed");
  }
  return 0;
}
