#include "film/movie_timeline_serializer.hpp"
#include "film/timeline_edit_service.hpp"
#include "scene/world.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

namespace {

using namespace kage;
using namespace kage::film;

[[nodiscard]] bool close(float parLeft, float parRight,
                         float parTolerance = 0.001f) {
  return std::abs(parLeft - parRight) <= parTolerance;
}

[[nodiscard]] bool close(const glm::vec3& parLeft, const glm::vec3& parRight) {
  return glm::length(parLeft - parRight) <= 0.002f;
}

[[nodiscard]] const TransformOverride* transformFor(const FilmFrameState& parState,
                                                    scene::EntityId parEntity) {
  const auto found = std::find_if(
      parState.transforms.begin(), parState.transforms.end(),
      [parEntity](const TransformOverride& item) { return item.entity == parEntity; });
  return found == parState.transforms.end() ? nullptr : &*found;
}

[[nodiscard]] const PropertyOverride* propertyFor(
    const FilmFrameState& parState, scene::EntityId parEntity,
    FilmPropertyKind parKind) {
  const auto found = std::find_if(
      parState.properties.begin(), parState.properties.end(),
      [=](const PropertyOverride& item) {
        return item.entity == parEntity && item.kind == parKind;
      });
  return found == parState.properties.end() ? nullptr : &*found;
}

int fail(const char* parMessage) {
  std::cerr << parMessage << '\n';
  return 1;
}

}  // namespace

int main() {
  scene::World world;
  const scene::EntityId rig = world.createEntityWithId("Samurai", {101});
  world.findEntity(rig)->transform.transform.translation = {-8.0f, 0.0f, 3.0f};
  world.setRig(rig, scene::RigComponent{});
  const scene::EntityId camera = world.createEntityWithId("Movie Camera", {102});
  world.findEntity(camera)->transform.transform.translation = {4.0f, 3.0f, 12.0f};
  world.setCamera(camera, scene::CameraComponent{false, 52.0f, 0.2f, 500.0f});
  const scene::EntityId light = world.createEntityWithId("Lantern", {103});
  scene::LightComponent light_component;
  light_component.enabled = false;
  light_component.color = {0.6f, 0.3f, 0.1f};
  light_component.intensity = 2.5f;
  light_component.range = 42.0f;
  world.setLight(light, light_component);

  const math::Transform rig_before = world.findEntity(rig)->transform.transform;
  const math::Transform camera_before = world.findEntity(camera)->transform.transform;
  const scene::LightComponent light_before = *world.findEntity(light)->light;

  MovieTimeline empty;
  std::string error;
  if (!decodeMovieTimeline({}, empty, error) || empty.durationFrames() != 0 ||
      !decodeMovieTimeline("null", empty, error) || empty.durationFrames() != 0) {
    return fail("missing or null Film data did not load as an empty timeline");
  }
  for (const std::string& unsupported : {"{}", "{\"schema_version\":1}",
                                         "{\"schema_version\":3}"}) {
    if (decodeMovieTimeline(unsupported, empty, error) ||
        error != "Unsupported Film schema version; expected 2") {
      return fail("unsupported Film schema did not report the expected error");
    }
  }

  MovieTimeline timeline;
  TimelineEditService edits(timeline);
  const auto rig_sequence = edits.createSequence(
      "Samurai approach", {TimelineTargetKind::RiggedEntity, rig},
      CapturedEntityBaseState{rig_before, std::nullopt, std::nullopt});
  const auto camera_sequence = edits.createSequence(
      "Shrine camera", {TimelineTargetKind::Camera, camera},
      CapturedEntityBaseState{camera_before,
                              CapturedCameraState{52.0f, 0.2f, 500.0f},
                              std::nullopt});
  const auto light_sequence = edits.createSequence(
      "Lantern light", {TimelineTargetKind::PointLight, light},
      CapturedEntityBaseState{
          world.findEntity(light)->transform.transform, std::nullopt,
          CapturedPointLightState{light_before.enabled, light_before.color,
                                  light_before.intensity, light_before.range,
                                  light_before.casts_shadows}});
  if (!rig_sequence.has_value() || !camera_sequence.has_value() ||
      !light_sequence.has_value()) {
    return fail("Film v2 sequence setup failed");
  }

  MovementClip rig_movement;
  rig_movement.end.translation = {2.0f, 0.0f, -4.0f};
  rig_movement.curve.automatic_position_controls = false;
  rig_movement.curve.position_control_1 = {-7.0f, 0.0f, -1.0f};
  rig_movement.curve.position_control_2 = {-1.0f, 0.0f, -5.0f};
  rig_movement.curve.timing_control_1 = 0.18f;
  rig_movement.curve.timing_control_2 = 0.84f;
  RigAnimationClip arm_action;
  arm_action.clip_id = 7001;
  arm_action.source_in = 0.1f;
  arm_action.source_out = 0.8f;
  arm_action.speed = 0.5f;
  arm_action.looping = true;
  MovementClip camera_movement;
  camera_movement.end.translation = {1.0f, 5.0f, 7.0f};
  camera_movement.curve.automatic_position_controls = false;
  camera_movement.curve.position_control_1 = {5.0f, 8.0f, 10.0f};
  camera_movement.curve.position_control_2 = {-2.0f, 4.0f, 8.0f};
  camera_movement.curve.timing_control_1 = 0.25f;
  camera_movement.curve.timing_control_2 = 0.75f;
  PropertyClip camera_fov;
  camera_fov.kind = PropertyKind::CameraFov;
  camera_fov.start_value = camera_fov.control_1 = glm::vec4(52.0f);
  camera_fov.control_2 = camera_fov.end_value = glm::vec4(65.0f);
  PropertyClip light_intensity;
  light_intensity.kind = PropertyKind::PointLightIntensity;
  light_intensity.start_value = light_intensity.control_1 = glm::vec4(2.5f);
  light_intensity.control_2 = light_intensity.end_value = glm::vec4(6.0f);

  const auto rig_movement_id = edits.appendClipToLane(*rig_sequence, 60, rig_movement);
  const auto arm_action_id = edits.appendClipToLane(*rig_sequence, 60, arm_action);
  const auto camera_movement_id =
      edits.appendClipToLane(*camera_sequence, 60, camera_movement);
  const auto camera_fov_id = edits.appendClipToLane(*camera_sequence, 60, camera_fov);
  const auto light_intensity_id =
      edits.appendClipToLane(*light_sequence, 60, light_intensity);
  const auto rig_instance = edits.placeSequence(*rig_sequence, 0);
  const auto camera_instance = edits.placeSequence(*camera_sequence, 0);
  const auto light_instance = edits.placeSequence(*light_sequence, 0);
  if (!rig_movement_id.has_value() || !arm_action_id.has_value() ||
      !camera_movement_id.has_value() || !camera_fov_id.has_value() ||
      !light_intensity_id.has_value() || !rig_instance.has_value() ||
      !camera_instance.has_value() || !light_instance.has_value() ||
      validateMovieTimeline(timeline, true).hasErrors()) {
    return fail("Film v2 authoring setup is invalid");
  }

  const FilmFrameState preview = evaluateMovieTimeline(timeline, 30);
  const TransformOverride* rig_transform = transformFor(preview, rig);
  const PropertyOverride* light_enabled =
      propertyFor(preview, light, FilmPropertyKind::LightEnabled);
  const PropertyOverride* light_range =
      propertyFor(preview, light, FilmPropertyKind::LightRange);
  const PropertyOverride* evaluated_intensity =
      propertyFor(preview, light, FilmPropertyKind::LightIntensity);
  if (rig_transform == nullptr ||
      preview.rig_animations.size() != 1 ||
      preview.rig_animations.front().animation.clip_id != arm_action.clip_id ||
      !close(preview.rig_animations.front().local_time_seconds, 0.5f) ||
      !preview.rig_animations.front().animation.looping ||
      !preview.camera_output.camera.has_value() ||
      !close(preview.camera_output.camera->vertical_fov_degrees, 58.5f) ||
      light_enabled == nullptr || light_range == nullptr ||
      evaluated_intensity == nullptr || light_enabled->value.x != 0.0f ||
      !close(light_range->value.x, 42.0f) ||
      !close(evaluated_intensity->value.x, 4.25f)) {
    return fail("Film v2 evaluation did not preserve current Movie behavior");
  }

  const std::string saved = encodeMovieTimeline(timeline);
  MovieTimeline reloaded;
  if (!decodeMovieTimeline(saved, reloaded, error) ||
      reloaded.next_sequence_id != timeline.next_sequence_id ||
      reloaded.next_instance_id != timeline.next_instance_id ||
      reloaded.next_clip_id != timeline.next_clip_id ||
      reloaded.instances.size() != 3) {
    return fail("Film v2 round trip did not preserve stable IDs");
  }
  const SequenceClip* reloaded_arm_clip = reloaded.findClip(*arm_action_id);
  const SequenceClip* reloaded_camera_clip = reloaded.findClip(*camera_movement_id);
  const auto* reloaded_arm = reloaded_arm_clip == nullptr
                                 ? nullptr
                                 : std::get_if<RigAnimationClip>(
                                       &reloaded_arm_clip->payload);
  const auto* reloaded_camera = reloaded_camera_clip == nullptr
                                    ? nullptr
                                    : std::get_if<MovementClip>(
                                          &reloaded_camera_clip->payload);
  if (reloaded_arm == nullptr || reloaded_camera == nullptr ||
      reloaded_arm->clip_id != arm_action.clip_id ||
      !close(reloaded_arm->source_in, arm_action.source_in) ||
      !close(reloaded_arm->source_out, arm_action.source_out) ||
      !close(reloaded_arm->speed, arm_action.speed) ||
      !reloaded_arm->looping || reloaded_camera->curve.automatic_position_controls ||
      !close(reloaded_camera->curve.position_control_1,
             camera_movement.curve.position_control_1) ||
      !close(reloaded_camera->curve.position_control_2,
             camera_movement.curve.position_control_2)) {
    return fail("Film v2 reload changed animation or curve data");
  }

  const FilmFrameState reloaded_preview = evaluateMovieTimeline(reloaded, 30);
  const TransformOverride* reloaded_rig_transform = transformFor(reloaded_preview, rig);
  const PropertyOverride* reloaded_light_range =
      propertyFor(reloaded_preview, light, FilmPropertyKind::LightRange);
  if (reloaded_rig_transform == nullptr || reloaded_light_range == nullptr ||
      !close(reloaded_rig_transform->transform.translation,
             rig_transform->transform.translation) ||
      !close(reloaded_light_range->value.x, light_range->value.x) ||
      !reloaded_preview.camera_output.camera.has_value() ||
      !close(reloaded_preview.camera_output.camera->transform.translation,
             preview.camera_output.camera->transform.translation) ||
      !close(reloaded_preview.camera_output.camera->vertical_fov_degrees,
             preview.camera_output.camera->vertical_fov_degrees)) {
    return fail("Film v2 round trip changed evaluated data");
  }
  if (!close(world.findEntity(rig)->transform.transform.translation,
             rig_before.translation) ||
      !close(world.findEntity(camera)->transform.transform.translation,
             camera_before.translation) ||
      world.findEntity(light)->light->enabled != light_before.enabled ||
      !close(world.findEntity(light)->light->range, light_before.range)) {
    return fail("Movie evaluation mutated World state");
  }
  return 0;
}
