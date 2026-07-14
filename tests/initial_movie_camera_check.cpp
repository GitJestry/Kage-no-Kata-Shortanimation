#include "engine/film_camera_creation.hpp"
#include "film/movie_timeline.hpp"

#include <array>
#include <iostream>
#include <string_view>
#include <utility>
#include <variant>

namespace {

int fail(const char* parMessage) {
  std::cerr << parMessage << '\n';
  return 1;
}

int checkAtomicFailure(
    kage::engine::detail::FilmCameraCreationFailurePoint parFailurePoint,
    std::string_view parLabel) {
  kage::scene::SceneManager::SceneRecord scene;
  const kage::scene::EntityId sentinel = scene.world.createEntity("Sentinel");
  scene.selected_entity = sentinel;
  const kage::scene::EntityRecord* storage_before =
      scene.world.getEntities().data();
  scene.movie_timeline.next_sequence_id = 41;
  scene.movie_timeline.next_clip_id = 51;
  scene.movie_timeline.next_instance_id = 61;

  kage::math::Transform transform;
  transform.translation = {2.0f, 3.0f, 4.0f};
  const kage::film::CapturedCameraState camera{55.0f, 0.05f, 800.0f};
  const auto result = kage::engine::detail::createFilmCameraAtomically(
      scene, transform, camera, 12, parFailurePoint);
  if (result.has_value()) {
    std::cerr << parLabel << " unexpectedly succeeded\n";
    return 1;
  }

  const auto entities = scene.world.getEntities();
  if (entities.size() != 1 || entities.data() != storage_before ||
      entities.front().id != sentinel || entities.front().camera.has_value() ||
      !entities.front().alive || entities.front().name.name != "Sentinel" ||
      scene.selected_entity != sentinel) {
    std::cerr << parLabel << " changed World entity storage\n";
    return 1;
  }
  if (!scene.movie_timeline.sequences.empty() ||
      !scene.movie_timeline.instances.empty() ||
      scene.movie_timeline.next_sequence_id != 41 ||
      scene.movie_timeline.next_clip_id != 51 ||
      scene.movie_timeline.next_instance_id != 61) {
    std::cerr << parLabel << " changed MovieTimeline state or counters\n";
    return 1;
  }
  if (scene.world.createEntity("After failure").value != sentinel.value + 1) {
    std::cerr << parLabel << " changed the World entity ID counter\n";
    return 1;
  }
  return 0;
}

}  // namespace

int main() {
  kage::film::MovieTimeline empty_timeline;
  if (!kage::film::requiresInitialFilmCamera(
          kage::film::MovieTimelineOrigin::NewProject, empty_timeline)) {
    return fail("new project without film data did not require initial camera");
  }
  if (kage::film::initialFilmCameraCreationFrame(
          kage::film::MovieTimelineOrigin::NewProject, empty_timeline) != 0) {
    return fail("new project did not create its initial camera at frame zero");
  }
  if (kage::film::requiresInitialFilmCamera(
          kage::film::MovieTimelineOrigin::LoadedProject, empty_timeline)) {
    return fail("loading an existing project created initial film data");
  }
  if (kage::film::initialFilmCameraCreationFrame(
          kage::film::MovieTimelineOrigin::LoadedProject, empty_timeline)
          .has_value()) {
    return fail("loading an existing project created an initial camera");
  }

  empty_timeline.sequences.push_back({});
  if (kage::film::requiresInitialFilmCamera(
          kage::film::MovieTimelineOrigin::NewProject, empty_timeline)) {
    return fail("new project with film data required another initial camera");
  }
  if (kage::film::initialFilmCameraCreationFrame(
          kage::film::MovieTimelineOrigin::NewProject, empty_timeline)
          .has_value()) {
    return fail("existing film data created another initial camera");
  }

  using kage::engine::detail::FilmCameraCreationFailurePoint;
  constexpr std::array failure_cases{
      std::pair{FilmCameraCreationFailurePoint::BeforeSequenceCreation,
                std::string_view{"failure before sequence creation"}},
      std::pair{FilmCameraCreationFailurePoint::WhileAddingClips,
                std::string_view{"failure while adding clips"}},
      std::pair{FilmCameraCreationFailurePoint::WhilePlacingInstance,
                std::string_view{"failure while placing the instance"}},
  };
  for (const auto& [failure_point, label] : failure_cases) {
    if (checkAtomicFailure(failure_point, label) != 0) {
      return 1;
    }
  }

  kage::scene::SceneManager::SceneRecord scene;
  scene.selected_entity = {777};
  kage::math::Transform transform;
  transform.translation = {5.0f, 6.0f, 7.0f};
  const kage::film::CapturedCameraState camera{60.0f, 0.03f, 900.0f};
  const auto created = kage::engine::detail::createFilmCameraAtomically(
      scene, transform, camera, 24);
  if (!created.has_value()) {
    return fail("atomic film camera creation failed");
  }
  const kage::scene::EntityRecord* entity =
      scene.world.findEntity(created->entity);
  if (scene.world.getEntities().size() != 1 || entity == nullptr ||
      !entity->camera.has_value() ||
      entity->transform.transform.translation != transform.translation ||
      entity->camera->vertical_fov_degrees != camera.vertical_fov_degrees ||
      entity->camera->near_plane != camera.near_plane ||
      entity->camera->far_plane != camera.far_plane) {
    return fail("successful creation did not create exactly one captured camera");
  }
  const kage::film::MovieTimeline& timeline = scene.movie_timeline;
  if (timeline.sequences.size() != 1 || timeline.instances.size() != 1 ||
      timeline.sequences.front().clips.size() != 2 ||
      timeline.sequences.front().id != created->sequence_id ||
      timeline.sequences.front().target.entity != created->entity ||
      timeline.instances.front().id != created->instance_id ||
      timeline.instances.front().sequence_id != created->sequence_id ||
      timeline.instances.front().start_frame != 24 ||
      timeline.next_sequence_id != 2 || timeline.next_clip_id != 3 ||
      timeline.next_instance_id != 2 || scene.selected_entity.value != 777 ||
      !std::holds_alternative<kage::film::MovementClip>(
          timeline.sequences.front().clips[0].payload) ||
      !std::holds_alternative<kage::film::PropertyClip>(
          timeline.sequences.front().clips[1].payload)) {
    return fail(
        "successful creation did not create one sequence, two clips, and one instance");
  }
  if (scene.world.createEntity("After success").value !=
      created->entity.value + 1) {
    return fail("successful creation did not advance the World entity ID once");
  }
  return 0;
}
