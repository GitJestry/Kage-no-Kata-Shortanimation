#include "engine/film_viewport.hpp"
#include "film/film_output_format.hpp"
#include "film/movie_timeline.hpp"
#include "film/timeline_edit_service.hpp"
#include "math/screen_projection.hpp"
#include "render/viewport_picking.hpp"

#include <cmath>
#include <iostream>

namespace {

[[nodiscard]] bool close(float parLeft, float parRight) {
  return std::abs(parLeft - parRight) < 0.001f;
}

int fail(const char* parMessage) {
  std::cerr << parMessage << '\n';
  return 1;
}

}  // namespace

int main() {
  using namespace kage;

  camera::Camera framing_camera;
  framing_camera.vertical_fov_degrees = 63.0f;
  const glm::mat4 preview_projection =
      framing_camera.getProjectionMatrix({1280.0f, 720.0f});
  const glm::mat4 bake_projection = framing_camera.getProjectionMatrix(
      {static_cast<float>(film::FILM_OUTPUT_WIDTH),
       static_cast<float>(film::FILM_OUTPUT_HEIGHT)});
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      if (!close(preview_projection[column][row],
                 bake_projection[column][row])) {
        return fail("16:9 Movie preview projection differs from Bake");
      }
    }
  }

  scene::World world;
  const scene::EntityId first_camera = world.createEntity("First Camera");
  const scene::EntityId second_camera = world.createEntity("Second Camera");
  world.setCamera(first_camera, scene::CameraComponent{});
  world.setCamera(second_camera, scene::CameraComponent{});

  film::MovieTimeline timeline;
  film::TimelineEditService edits(timeline);
  film::CapturedEntityBaseState first_base;
  first_base.transform.translation = {0.0f, 0.0f, 10.0f};
  first_base.camera = film::CapturedCameraState{40.0f, 0.1f, 300.0f};
  const auto first_sequence = edits.createSequence(
      "First", {film::TimelineTargetKind::Camera, first_camera}, first_base);
  film::MovementClip first_movement;
  first_movement.end = first_base.transform;
  const auto first_clip = first_sequence.has_value()
                              ? edits.appendClipToLane(*first_sequence, 10,
                                                       first_movement)
                              : std::expected<film::SequenceClipId,
                                              std::string>{
                                    std::unexpected("setup failed")};

  film::CapturedEntityBaseState second_base;
  second_base.transform.translation = {3.0f, 1.0f, 8.0f};
  second_base.camera = film::CapturedCameraState{70.0f, 0.2f, 600.0f};
  const auto second_sequence = edits.createSequence(
      "Second", {film::TimelineTargetKind::Camera, second_camera}, second_base);
  film::MovementClip second_movement;
  second_movement.end = second_base.transform;
  const auto second_clip = second_sequence.has_value()
                               ? edits.appendClipToLane(*second_sequence, 10,
                                                        second_movement)
                               : std::expected<film::SequenceClipId,
                                               std::string>{
                                     std::unexpected("setup failed")};
  const auto first_instance = first_sequence.has_value()
                                  ? edits.placeSequence(*first_sequence, 0)
                                  : std::expected<film::SequenceInstanceId,
                                                  std::string>{
                                        std::unexpected("setup failed")};
  const auto second_instance = second_sequence.has_value()
                                   ? edits.placeSequence(*second_sequence, 10)
                                   : std::expected<film::SequenceInstanceId,
                                                   std::string>{
                                         std::unexpected("setup failed")};
  if (!first_clip.has_value() || !second_clip.has_value() ||
      !first_instance.has_value() || !second_instance.has_value()) {
    return fail("camera timeline setup failed");
  }

  const film::FilmFrameState before_cut =
      film::evaluateMovieTimeline(timeline, 9);
  const film::FilmFrameState at_cut = film::evaluateMovieTimeline(timeline, 10);
  if (!before_cut.camera_output.camera.has_value() ||
      before_cut.camera_output.camera->source_entity != first_camera ||
      !at_cut.camera_output.camera.has_value() ||
      at_cut.camera_output.camera->source_entity != second_camera ||
      !close(at_cut.camera_output.camera->vertical_fov_degrees, 70.0f)) {
    return fail("camera cut did not select the expected FilmCameraOutput");
  }

  film::FilmPlayback playback;
  playback.playing = true;
  playback.previewing = true;
  if (!film::requiresFilmFrameState(true, false, -1.0, playback)) {
    return fail("master Play did not consume FilmFrameState");
  }
  camera::Camera editor_camera;
  editor_camera.position = {-4.0f, 5.0f, 12.0f};
  editor_camera.vertical_fov_degrees = 52.0f;
  const engine::FilmViewportCamera master_view =
      engine::resolveFilmViewportCamera(editor_camera, world, &at_cut);
  if (!master_view.consumes_film_state || master_view.black_output ||
      !master_view.camera.has_value() ||
      !close(master_view.camera->position.x,
             second_base.transform.translation.x) ||
      !close(master_view.camera->vertical_fov_degrees, 70.0f)) {
    return fail("master Play did not resolve the authored film camera");
  }
  if (engine::shouldShowEditorOverlays(true, master_view, true)) {
    return fail("Movie Timeline camera preview retained editor overlays");
  }

  if (!edits.moveInstance(*second_instance, 20).has_value()) {
    return fail("camera gap setup failed");
  }
  const film::FilmFrameState held_gap =
      film::evaluateMovieTimeline(timeline, 15);
  if (!held_gap.camera_output.camera.has_value() ||
      held_gap.camera_output.camera->source_entity != first_camera) {
    return fail("HoldLastCameraState did not hold the preceding camera");
  }
  if (!edits.setCameraGapMode(film::CameraGapMode::Black).has_value()) {
    return fail("black camera gap setup failed");
  }
  const film::FilmFrameState black_gap =
      film::evaluateMovieTimeline(timeline, 15);
  const engine::FilmViewportCamera black_view =
      engine::resolveFilmViewportCamera(editor_camera, world, &black_gap);
  if (black_gap.camera_output.kind != film::FilmOutputKind::Black ||
      black_view.camera.has_value() || !black_view.black_output) {
    return fail("Black camera gap did not produce a black viewport output");
  }
  if (engine::shouldShowEditorOverlays(true, black_view, true)) {
    return fail("black camera preview retained editor overlays");
  }

  const engine::FilmViewportCamera stopped_view =
      engine::resolveFilmViewportCamera(editor_camera, world, nullptr);
  if (stopped_view.consumes_film_state || stopped_view.black_output ||
      !stopped_view.camera.has_value() ||
      stopped_view.camera->position != editor_camera.position ||
      !close(stopped_view.camera->vertical_fov_degrees,
             editor_camera.vertical_fov_degrees)) {
    return fail("Stop did not resolve immediately back to the editor camera");
  }
  if (!engine::shouldShowEditorOverlays(true, stopped_view, true) ||
      engine::shouldShowEditorOverlays(false, stopped_view, true)) {
    return fail("normal viewport did not restore the requested overlay state");
  }
  film::FilmFrameState non_camera_sequence_state;
  const engine::FilmViewportCamera non_camera_sequence_view =
      engine::resolveFilmViewportCamera(editor_camera, world,
                                        &non_camera_sequence_state, false);
  if (!non_camera_sequence_view.consumes_film_state ||
      non_camera_sequence_view.black_output ||
      !non_camera_sequence_view.camera.has_value() ||
      non_camera_sequence_view.camera->position != editor_camera.position) {
    return fail("non-camera sequence preview did not keep the editor camera");
  }
  if (!engine::shouldShowEditorOverlays(
          true, non_camera_sequence_view, false)) {
    return fail("non-camera sequence preview suppressed editor overlays");
  }
  if (world.findEntity(second_camera)->transform.transform.translation !=
      glm::vec3(0.0f)) {
    return fail("film camera resolution mutated authored World state");
  }

  scene::World pick_world;
  const scene::EntityId actor = pick_world.createEntity("Animated Actor");
  pick_world.setRig(actor, scene::RigComponent{});
  film::FilmFrameState pick_state;
  math::Transform evaluated_actor;
  evaluated_actor.translation = {3.0f, 0.0f, 0.0f};
  pick_state.transforms.push_back({actor, evaluated_actor});
  camera::Camera pick_camera;
  pick_camera.position = {0.0f, 0.0f, 10.0f};
  const glm::vec2 viewport{1000.0f, 1000.0f};
  const glm::mat4 view_projection =
      pick_camera.getViewProjectionMatrix(viewport);
  const math::ScreenPoint evaluated_pixel = math::projectPoint(
      evaluated_actor.translation, view_projection, viewport);
  const math::ScreenPoint authored_pixel = math::projectPoint(
      glm::vec3(0.0f), view_projection, viewport);
  if (!evaluated_pixel.valid || !authored_pixel.valid ||
      render::pickViewportEntityBounds(
          pick_world, &pick_camera, &pick_state, evaluated_pixel.pixel,
          viewport, 0.35f) != actor ||
      render::pickViewportEntityBounds(
          pick_world, &pick_camera, &pick_state, authored_pixel.pixel,
          viewport, 0.35f)
          .has_value() ||
      render::pickViewportEntityBounds(
          pick_world, nullptr, &pick_state, evaluated_pixel.pixel, viewport,
          0.35f)
          .has_value()) {
    return fail("Movie preview picking did not follow the evaluated viewport");
  }

  return 0;
}
