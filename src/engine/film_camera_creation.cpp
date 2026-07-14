#include "engine/film_camera_creation.hpp"

#include "film/timeline_edit_service.hpp"

#include <glm/glm.hpp>

#include <type_traits>
#include <utility>

namespace kage::engine {

static_assert(std::is_nothrow_move_assignable_v<scene::World>);
static_assert(std::is_nothrow_move_assignable_v<film::MovieTimeline>);

std::expected<FilmCameraCreationResult, std::string>
createFilmCameraAtomically(
    scene::SceneManager::SceneRecord& parScene,
    const math::Transform& parTransform,
    const film::CapturedCameraState& parCamera,
    film::FilmFrame parStartFrame) {
  constexpr film::FilmFrame CAMERA_SEQUENCE_DURATION = 300;
  if (parStartFrame < 0 ||
      parStartFrame > film::MAX_FILM_FRAMES - CAMERA_SEQUENCE_DURATION) {
    return std::unexpected("Film camera would exceed the frame limit");
  }

  scene::World candidate_world = parScene.world;
  film::MovieTimeline candidate_timeline = parScene.movie_timeline;

  const scene::EntityId entity = candidate_world.createEntity("Film Camera");
  scene::EntityRecord* record = candidate_world.findEntity(entity);
  if (record == nullptr) {
    return std::unexpected("Could not create a film camera");
  }
  record->transform.transform = parTransform;
  scene::CameraComponent camera;
  camera.vertical_fov_degrees = parCamera.vertical_fov_degrees;
  camera.near_plane = parCamera.near_plane;
  camera.far_plane = parCamera.far_plane;
  candidate_world.setCamera(entity, camera);

  film::CapturedEntityBaseState base;
  base.transform = parTransform;
  base.camera = parCamera;
  film::TimelineEditService edits(candidate_timeline);
  const auto sequence = edits.createSequence(
      "Film Camera", {film::TimelineTargetKind::Camera, entity}, base);
  if (!sequence.has_value()) {
    return std::unexpected(sequence.error());
  }

  film::MovementClip movement;
  movement.end = parTransform;
  const auto movement_clip = edits.appendClipToLane(
      *sequence, CAMERA_SEQUENCE_DURATION, movement);
  if (!movement_clip.has_value()) {
    return std::unexpected(movement_clip.error());
  }
  film::PropertyClip fov;
  fov.kind = film::PropertyKind::CameraFov;
  fov.start_value = glm::vec4(parCamera.vertical_fov_degrees);
  fov.control_1 = fov.start_value;
  fov.control_2 = fov.start_value;
  fov.end_value = fov.start_value;
  const auto fov_clip = edits.appendClipToLane(
      *sequence, CAMERA_SEQUENCE_DURATION, fov);
  if (!fov_clip.has_value()) {
    return std::unexpected(fov_clip.error());
  }

  const auto instance = edits.placeSequence(*sequence, parStartFrame);
  if (!instance.has_value()) {
    return std::unexpected(instance.error());
  }
  parScene.world = std::move(candidate_world);
  parScene.movie_timeline = std::move(candidate_timeline);
  return FilmCameraCreationResult{entity, *sequence, *instance};
}

}  // namespace kage::engine
