#include "check_helpers.hpp"
#include "film/movie_timeline_serializer.hpp"
#include "film/timeline_edit_service.hpp"
#include "lighting/lighting_system.hpp"
#include "scene/world.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace {

using namespace kage;
using namespace kage::film;
using kage::test::close;
using kage::test::fail;

[[nodiscard]] bool close(const glm::vec3& parLeft, const glm::vec3& parRight) {
  return glm::length(parLeft - parRight) < 0.002f;
}

[[nodiscard]] bool close(const glm::quat& parLeft, const glm::quat& parRight) {
  return std::abs(std::abs(glm::dot(parLeft, parRight)) - 1.0f) < 0.002f;
}

[[nodiscard]] const TransformOverride* transformFor(const FilmFrameState& parState,
                                                    scene::EntityId parEntity) {
  const auto found =
      std::find_if(parState.transforms.begin(), parState.transforms.end(),
                   [parEntity](const TransformOverride& item) { return item.entity == parEntity; });
  return found == parState.transforms.end() ? nullptr : &*found;
}

[[nodiscard]] const EvaluatedPointLightState* pointLightFor(const FilmFrameState& parState,
                                                            scene::EntityId parEntity) {
  const auto found = std::find_if(parState.point_lights.begin(), parState.point_lights.end(),
                                  [parEntity](const EvaluatedPointLightState& item) {
                                    return item.source_entity == parEntity;
                                  });
  return found == parState.point_lights.end() ? nullptr : &*found;
}

[[nodiscard]] bool sameEvaluatedFrame(const FilmFrameState& parBefore,
                                      const FilmFrameState& parAfter, scene::EntityId parRig,
                                      scene::EntityId parLight) {
  const TransformOverride* before_transform = transformFor(parBefore, parRig);
  const TransformOverride* after_transform = transformFor(parAfter, parRig);
  const EvaluatedPointLightState* before_light = pointLightFor(parBefore, parLight);
  const EvaluatedPointLightState* after_light = pointLightFor(parAfter, parLight);
  return before_transform != nullptr && after_transform != nullptr &&
         close(before_transform->transform.translation, after_transform->transform.translation) &&
         before_light != nullptr && after_light != nullptr &&
         before_light->source_entity == after_light->source_entity &&
         before_light->enabled == after_light->enabled &&
         close(before_light->color, after_light->color) &&
         close(before_light->intensity, after_light->intensity) &&
         close(before_light->range, after_light->range) &&
         before_light->casts_shadows == after_light->casts_shadows &&
         parBefore.rig_animations.size() == 1 && parAfter.rig_animations.size() == 1 &&
         parBefore.rig_animations.front().animation.clip_id ==
             parAfter.rig_animations.front().animation.clip_id &&
         close(parBefore.rig_animations.front().local_time_seconds,
               parAfter.rig_animations.front().local_time_seconds) &&
         parBefore.camera && parAfter.camera &&
         parBefore.camera->source_entity == parAfter.camera->source_entity &&
         close(parBefore.camera->transform.translation, parAfter.camera->transform.translation) &&
         close(parBefore.camera->transform.rotation, parAfter.camera->transform.rotation) &&
         close(parBefore.camera->transform.scale, parAfter.camera->transform.scale) &&
         close(parBefore.camera->vertical_fov_degrees, parAfter.camera->vertical_fov_degrees) &&
         close(parBefore.camera->near_plane, parAfter.camera->near_plane) &&
         close(parBefore.camera->far_plane, parAfter.camera->far_plane);
}

} // namespace

int main() {
  scene::World world;
  const scene::EntityId rig = world.createEntityWithId("Rig", {101});
  world.findEntity(rig)->transform.transform.translation = {-8.0f, 0.0f, 3.0f};
  world.setRig(rig, scene::RigComponent{});
  const scene::EntityId camera = world.createEntityWithId("Movie Camera", {102});
  world.findEntity(camera)->transform.transform.translation = {4.0f, 3.0f, 12.0f};
  world.setCamera(camera, scene::CameraComponent{
                              .vertical_fov_degrees = 52.0f,
                              .near_plane = 0.2f,
                              .far_plane = 500.0f,
                          });
  const scene::EntityId light = world.createEntityWithId("Light", {103});
  scene::LightComponent light_component;
  light_component.enabled = true;
  light_component.color = {0.6f, 0.3f, 0.1f};
  light_component.intensity = 2.5f;
  light_component.range = 42.0f;
  light_component.casts_shadows = false;
  world.setLight(light, light_component);

  const math::Transform rig_before = world.findEntity(rig)->transform.transform;
  const math::Transform camera_before = world.findEntity(camera)->transform.transform;
  const scene::LightComponent light_before = *world.findEntity(light)->light;

  MovieTimeline timeline;
  TimelineEditService edits(timeline);
  const auto rig_sequence =
      edits.createSequence("Rig movement", {TimelineTargetKind::RiggedEntity, rig},
                           CapturedEntityBaseState{rig_before, std::nullopt, std::nullopt});
  const auto camera_sequence = edits.createSequence(
      "Camera movement", {TimelineTargetKind::Camera, camera},
      CapturedEntityBaseState{camera_before, CapturedCameraState{52.0f, 0.2f, 500.0f},
                              std::nullopt});
  const auto light_sequence = edits.createSequence(
      "Light animation", {TimelineTargetKind::PointLight, light},
      CapturedEntityBaseState{world.findEntity(light)->transform.transform, std::nullopt,
                              CapturedPointLightState{light_before.enabled, light_before.color,
                                                      light_before.intensity, light_before.range,
                                                      light_before.casts_shadows}});
  if (!rig_sequence || !camera_sequence || !light_sequence) {
    return fail("Film sequence setup failed");
  }

  MovementClip rig_movement;
  rig_movement.end.translation = {2.0f, 0.0f, -4.0f};
  rig_movement.curve.automatic_position_controls = false;
  rig_movement.curve.position_control_1 = {-7.0f, 0.0f, -1.0f};
  rig_movement.curve.position_control_2 = {-1.0f, 0.0f, -5.0f};
  rig_movement.curve.timing_control_1 = 0.18f;
  rig_movement.curve.timing_control_2 = 0.84f;
  RigAnimationClip animation_clip{
      .clip_id = 7001, .source_in = 0.1f, .source_out = 0.8f, .speed = 0.5f, .looping = true};
  MovementClip camera_movement;
  camera_movement.end.translation = {1.0f, 5.0f, 7.0f};
  MovementClip camera_transition;
  camera_transition.start_mode = MovementStartMode::ExplicitPosition;
  camera_transition.explicit_start = camera_movement.end;
  camera_transition.explicit_start->translation = {7.0f, 6.0f, 4.0f};
  camera_transition.end.translation = {10.0f, 8.0f, 1.0f};
  camera_transition.transition_before = {
      true, {{2.0f, 7.0f, 9.0f}, {6.0f, 8.0f, 2.0f}, 0.21f, 0.79f, false}};
  PropertyClip camera_fov;
  camera_fov.kind = PropertyKind::CameraFov;
  camera_fov.start_value = camera_fov.control_1 = glm::vec4(52.0f);
  camera_fov.end_value = camera_fov.control_2 = glm::vec4(65.0f);
  PropertyClip light_intensity;
  light_intensity.kind = PropertyKind::PointLightIntensity;
  light_intensity.start_value = light_intensity.control_1 = glm::vec4(2.5f);
  light_intensity.end_value = light_intensity.control_2 = glm::vec4(6.0f);

  const auto rig_move_id = edits.appendClipToLane(*rig_sequence, 60, rig_movement);
  const auto animation_id = edits.appendClipToLane(*rig_sequence, 130, animation_clip);
  const auto camera_move_id = edits.appendClipToLane(*camera_sequence, 60, camera_movement);
  const auto camera_transition_id = edits.appendClipToLane(*camera_sequence, 40, camera_transition);
  const auto fov_id = edits.appendClipToLane(*camera_sequence, 60, camera_fov);
  const auto intensity_id = edits.appendClipToLane(*light_sequence, 60, light_intensity);
  const auto rig_instance = edits.placeSequence(*rig_sequence, 0);
  const auto camera_instance = edits.placeSequence(*camera_sequence, 0);
  const auto light_instance = edits.placeSequence(*light_sequence, 0);
  if (!rig_move_id || !animation_id || !camera_move_id || !camera_transition_id ||
      !edits.moveClip(*camera_transition_id, 90, 130) || !fov_id || !intensity_id ||
      !rig_instance || !camera_instance || !light_instance ||
      validateMovieTimeline(timeline, true).hasErrors()) {
    return fail("Film authoring setup is invalid");
  }

  const FilmFrameState before = evaluateMovieTimeline(timeline, 30);
  const EvaluatedPointLightState* evaluated_light = pointLightFor(before, light);
  if (!sameEvaluatedFrame(before, before, rig, light) ||
      before.rig_animations.front().animation.clip_id != animation_clip.clip_id ||
      !before.rig_animations.front().animation.looping || evaluated_light == nullptr ||
      !evaluated_light->enabled || !close(evaluated_light->color, light_before.color) ||
      !close(evaluated_light->range, light_before.range) ||
      evaluated_light->casts_shadows != light_before.casts_shadows) {
    return fail("Film evaluation did not preserve authored values");
  }

  const std::string encoded = encodeMovieTimeline(timeline);
  MovieTimeline reloaded;
  std::string error;
  if (!decodeMovieTimeline(encoded, reloaded, error) || encodeMovieTimeline(reloaded) != encoded ||
      reloaded.next_sequence_id != timeline.next_sequence_id ||
      reloaded.next_instance_id != timeline.next_instance_id ||
      reloaded.next_clip_id != timeline.next_clip_id ||
      reloaded.sequences.size() != timeline.sequences.size() ||
      reloaded.instances.size() != timeline.instances.size()) {
    return fail("Film did not encode and decode correctly");
  }
  const SequenceClip* reloaded_animation = reloaded.findClip(*animation_id);
  const SequenceClip* reloaded_camera_move = reloaded.findClip(*camera_move_id);
  const auto* animation = reloaded_animation == nullptr
                              ? nullptr
                              : std::get_if<RigAnimationClip>(&reloaded_animation->payload);
  const auto* movement = reloaded_camera_move == nullptr
                             ? nullptr
                             : std::get_if<MovementClip>(&reloaded_camera_move->payload);
  if (reloaded.findSequence(*rig_sequence) == nullptr ||
      reloaded.findSequence(*camera_sequence) == nullptr ||
      reloaded.findSequence(*light_sequence) == nullptr ||
      reloaded.findClip(*rig_move_id) == nullptr || reloaded.findClip(*fov_id) == nullptr ||
      reloaded.findClip(*intensity_id) == nullptr ||
      reloaded.findInstance(*rig_instance) == nullptr ||
      reloaded.findInstance(*camera_instance) == nullptr ||
      reloaded.findInstance(*light_instance) == nullptr || animation == nullptr ||
      movement == nullptr || animation->clip_id != animation_clip.clip_id || !animation->looping ||
      movement->end.translation != camera_movement.end.translation) {
    return fail("Film round trip did not retain IDs, sequences, clips, and instances");
  }

  const FilmFrameState after = evaluateMovieTimeline(reloaded, 30);
  if (!sameEvaluatedFrame(before, after, rig, light) ||
      !sameEvaluatedFrame(evaluateMovieTimeline(timeline, 75), evaluateMovieTimeline(reloaded, 75),
                          rig, light)) {
    return fail("Film round trip changed evaluated output");
  }
  if (!close(world.findEntity(rig)->transform.transform.translation, rig_before.translation) ||
      !close(world.findEntity(camera)->transform.transform.translation,
             camera_before.translation) ||
      world.findEntity(light)->light->enabled != light_before.enabled ||
      !close(world.findEntity(light)->light->range, light_before.range)) {
    return fail("Film evaluation mutated authored World data");
  }

  world.findEntity(light)->light->enabled = false;
  world.findEntity(light)->light->color = {0.0f, 1.0f, 0.0f};
  world.findEntity(light)->light->intensity = 99.0f;
  world.findEntity(light)->light->range = 3.0f;
  world.findEntity(light)->light->casts_shadows = true;
  const FilmFrameState captured = evaluateMovieTimeline(reloaded, 0);
  const lighting::LightingState lighting =
      lighting::LightingSystem{}.extract(world, {}, {}, {}, &captured);
  if (lighting.point_light_count != 1 || !lighting.point_lights[0].enabled ||
      lighting.point_lights[0].casts_shadow ||
      !close(lighting.point_lights[0].color, light_before.color) ||
      !close(lighting.point_lights[0].intensity, light_before.intensity) ||
      !close(lighting.point_lights[0].range, light_before.range)) {
    return fail("captured point-light state changed after World edits");
  }
  return 0;
}
