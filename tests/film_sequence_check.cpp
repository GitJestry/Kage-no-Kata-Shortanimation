#include "film/film_sequence.hpp"

#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <iostream>

int main() {
  using namespace kage;
  film::FilmTimeline timeline;
  timeline.duration_frames = 90;
  timeline.camera_cuts.push_back({1, 0, 90, scene::EntityId{7}});

  film::FilmMovement camera_move;
  camera_move.end.translation = glm::vec3(6.0f, 3.0f, -2.0f);
  camera_move.end.rotation = glm::angleAxis(glm::radians(90.0f),
                                            glm::vec3(0.0f, 1.0f, 0.0f));
  if (timeline.addClip(scene::EntityId{7}, 0, 60, camera_move) == nullptr) {
    std::cerr << "camera movement clip was rejected\n";
    return 1;
  }
  film::FilmProperty fov;
  fov.kind = film::FilmPropertyKind::CameraFov;
  fov.start_value = fov.control_1 = glm::vec4(40.0f);
  fov.end_value = fov.control_2 = glm::vec4(60.0f);
  fov.control_1.x = 40.0f + 20.0f / 3.0f;
  fov.control_2.x = 60.0f - 20.0f / 3.0f;
  if (timeline.addClip(scene::EntityId{7}, 0, 60, fov) == nullptr) {
    std::cerr << "independent FOV lane was rejected\n";
    return 1;
  }

  if (timeline.validate().has_value()) {
    std::cerr << "valid timeline was rejected\n";
    return 1;
  }
  const auto sample = timeline.evaluateCamera(30.0);
  if (!sample.has_value() || sample->camera.value != 7 ||
      !sample->transform.has_value() ||
      std::abs(sample->transform->translation.x - 3.0f) > 0.01f ||
      !sample->vertical_fov_degrees.has_value() ||
      std::abs(*sample->vertical_fov_degrees - 50.0f) > 0.01f) {
    std::cerr << "camera clip evaluation failed\n";
    return 1;
  }
  if (timeline.evaluateCamera(90.0).has_value()) {
    std::cerr << "exclusive camera-cut end was ignored\n";
    return 1;
  }

  film::FilmMovement object_move;
  object_move.end.translation = glm::vec3(3.0f, 0.0f, 0.0f);
  film::FilmClip* object_clip =
      timeline.addClip(scene::EntityId{9}, 0, 30, object_move);
  const film::FilmClipId object_clip_id =
      object_clip != nullptr ? object_clip->id : 0;
  if (object_clip == nullptr ||
      timeline.addClip(scene::EntityId{9}, 15, 40, object_move) != nullptr) {
    std::cerr << "same-lane overlap rule failed\n";
    return 1;
  }
  const auto object_sample =
      timeline.evaluateTransform(scene::EntityId{9}, 15.0);
  if (!object_sample.has_value() ||
      std::abs(object_sample->translation.x - 1.5f) > 0.01f) {
    std::cerr << "movement Bezier interpolation failed\n";
    return 1;
  }
  if (!timeline.moveClip(object_clip_id, 30, 60) ||
      timeline.evaluateTransform(scene::EntityId{9}, 15.0).has_value()) {
    std::cerr << "clip move rule failed\n";
    return 1;
  }
  film::RigAnimation rig;
  rig.legacy_clip_index = 2;
  rig.source_in = 0.2f;
  rig.source_out = 0.8f;
  rig.blend_in_seconds = 0.5f;
  if (timeline.addClip(scene::EntityId{9}, 0, 30, rig) == nullptr) {
    std::cerr << "independent rig lane was rejected\n";
    return 1;
  }
  const film::FilmFrameState rig_frame = timeline.evaluate(7.5);
  if (rig_frame.rig_animations.size() != 1 ||
      std::abs(rig_frame.rig_animations.front().weight - 0.5f) > 0.01f) {
    std::cerr << "rig clip boundary/blend evaluation failed\n";
    return 1;
  }
  film::FilmFrameState solo;
  timeline.evaluateClip(object_clip_id, 45.0, solo);
  if (solo.transforms.size() != 1 || !solo.rig_animations.empty() ||
      solo.transforms.front().entity.value != 9) {
    std::cerr << "selected clip isolation failed\n";
    return 1;
  }
  film::FilmTimeline gap = timeline;
  gap.camera_cuts.front().end_frame = 30;
  if (gap.evaluateCamera(30.0).has_value()) {
    std::cerr << "camera-cut gap was not preserved\n";
    return 1;
  }
  return 0;
}
