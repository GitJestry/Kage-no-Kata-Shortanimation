#include "film/movie_timeline_serializer.hpp"
#include "film/timeline_edit_service.hpp"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

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

template <typename Value>
[[nodiscard]] Value cubic(const Value& p0, const Value& p1, const Value& p2,
                          const Value& p3, float t) {
  const float inverse = 1.0f - t;
  return inverse * inverse * inverse * p0 +
         3.0f * inverse * inverse * t * p1 +
         3.0f * inverse * t * t * p2 + t * t * t * p3;
}

[[nodiscard]] math::Transform oldCameraMovement(int parFrame) {
  const float linear = static_cast<float>(parFrame) / 12.0f;
  const float t = cubic(0.0f, 0.2f, 0.8f, 1.0f, linear);
  math::Transform result;
  result.translation = cubic(glm::vec3(1, 2, 3), glm::vec3(2, 8, 4),
                             glm::vec3(11, -2, -1), glm::vec3(13, 5, -3), t);
  result.rotation = glm::normalize(glm::slerp(
      glm::quat(1, 0, 0, 0), glm::quat(0.9238795f, 0, 0.3826834f, 0), t));
  result.scale = glm::mix(glm::vec3(1), glm::vec3(2, 1, 0.5f), t);
  return result;
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
  using namespace kage;
  using namespace kage::film;

  scene::World world;
  const scene::EntityId camera = world.createEntityWithId("Camera", {7});
  world.findEntity(camera)->transform.transform.translation = {91, 92, 93};
  world.setCamera(camera, scene::CameraComponent{false, 55.0f, 0.2f, 400.0f});
  const scene::EntityId rig = world.createEntityWithId("Rig", {8});
  world.findEntity(rig)->transform.transform.translation = {-4, 3, 2};
  const scene::EntityId light = world.createEntityWithId("Light", {9});
  scene::LightComponent light_component;
  light_component.enabled = false;
  light_component.range = 42.0f;
  world.setLight(light, light_component);

  const math::Transform camera_before = world.findEntity(camera)->transform.transform;
  const math::Transform rig_before = world.findEntity(rig)->transform.transform;
  const scene::LightComponent light_before = *world.findEntity(light)->light;

  std::ifstream fixture(std::filesystem::path(KAGE_TEST_SOURCE_DIR) /
                        "tests/fixtures/legacy_film_v1.json");
  std::ostringstream fixture_text;
  fixture_text << fixture.rdbuf();
  if (!fixture || fixture_text.str().empty()) {
    return fail("legacy fixture could not be read");
  }

  MovieTimeline timeline;
  bool migrated = false;
  std::string error;
  if (!decodeMovieTimeline(fixture_text.str(), world, {}, timeline, migrated,
                           error) ||
      !migrated) {
    return fail("legacy timeline migration failed");
  }
  if (timeline.durationFrames() != 12 ||
      timeline.camera_gap_mode != CameraGapMode::Black) {
    return fail("legacy duration or camera gap mode was not preserved");
  }

  if (!close(world.findEntity(camera)->transform.transform.translation,
             camera_before.translation) ||
      !close(world.findEntity(rig)->transform.transform.translation,
             rig_before.translation) ||
      world.findEntity(light)->light->enabled != light_before.enabled ||
      !close(world.findEntity(light)->light->range, light_before.range)) {
    return fail("migration mutated World state");
  }

  for (int frame = 0; frame < 12; ++frame) {
    const FilmFrameState state = evaluateMovieTimeline(timeline, frame);
    const TransformOverride* rig_transform = transformFor(state, rig);
    if (rig_transform == nullptr ||
        !close(rig_transform->transform.translation,
               glm::vec3(static_cast<float>(frame), 0, 0))) {
      return fail("migrated rig transform differs from legacy evaluation");
    }
    if (state.rig_animations.size() != 1 ||
        state.rig_animations[0].animation.clip_id != 7001 ||
        state.rig_animations[0].animation.legacy_clip_index != 3 ||
        !close(state.rig_animations[0].local_time_seconds,
               static_cast<float>(frame) / 30.0f * 0.5f)) {
      return fail("migrated animation identity or source time changed");
    }
    const float u = static_cast<float>(frame) / 12.0f;
    const PropertyOverride* enabled =
        propertyFor(state, light, FilmPropertyKind::LightEnabled);
    const PropertyOverride* range =
        propertyFor(state, light, FilmPropertyKind::LightRange);
    if (enabled == nullptr || range == nullptr ||
        !close(enabled->value.x, cubic(0.0f, 0.0f, 1.0f, 1.0f, u)) ||
        !close(range->value.x, cubic(4.0f, 8.0f, 16.0f, 24.0f, u))) {
      return fail("legacy LightEnabled or LightRange evaluation changed");
    }
    if (frame < 2 || frame >= 10) {
      if (state.camera_output.kind != FilmOutputKind::Black) {
        return fail("legacy camera cut gaps did not migrate to black");
      }
    } else {
      const math::Transform expected = oldCameraMovement(frame);
      if (!state.camera_output.camera.has_value() ||
          !close(state.camera_output.camera->transform.translation,
                 expected.translation) ||
          !close(state.camera_output.camera->vertical_fov_degrees,
                 cubic(40.0f, 52.0f, 70.0f, 80.0f, u))) {
        return fail("clipped camera curve migration is not frame-equivalent");
      }
    }
  }
  if (!close(world.findEntity(camera)->transform.transform.translation,
             camera_before.translation) ||
      !close(world.findEntity(rig)->transform.transform.translation,
             rig_before.translation) ||
      world.findEntity(light)->light->enabled != light_before.enabled ||
      !close(world.findEntity(light)->light->range, light_before.range)) {
    return fail("movie evaluation mutated World state");
  }

  const std::string saved = encodeMovieTimeline(timeline);
  if (saved.find("\"schema_version\":2") == std::string::npos ||
      saved.find("camera_cuts") != std::string::npos ||
      saved.find("duration_frames") != std::string::npos ||
      saved.find("\"tracks\"") != std::string::npos) {
    return fail("save emitted a legacy film schema field");
  }
  MovieTimeline round_trip;
  bool round_trip_migrated = true;
  if (!decodeMovieTimeline(saved, world, {}, round_trip, round_trip_migrated,
                           error) ||
      round_trip_migrated || round_trip.next_clip_id != timeline.next_clip_id ||
      round_trip.next_sequence_id != timeline.next_sequence_id ||
      round_trip.next_instance_id != timeline.next_instance_id) {
    return fail("new schema round trip did not preserve stable IDs");
  }
  for (int frame = 0; frame < 12; ++frame) {
    const FilmFrameState before = evaluateMovieTimeline(timeline, frame);
    const FilmFrameState after = evaluateMovieTimeline(round_trip, frame);
    if (before.transforms.size() != after.transforms.size() ||
        before.properties.size() != after.properties.size() ||
        before.rig_animations.size() != after.rig_animations.size() ||
        before.camera_output.kind != after.camera_output.kind) {
      return fail("new schema round trip changed evaluated frame structure");
    }
    const PropertyOverride* before_range =
        propertyFor(before, light, FilmPropertyKind::LightRange);
    const PropertyOverride* after_range =
        propertyFor(after, light, FilmPropertyKind::LightRange);
    if (before_range == nullptr || after_range == nullptr ||
        !close(before_range->value.x, after_range->value.x) ||
        before.camera_output.camera.has_value() !=
            after.camera_output.camera.has_value() ||
        (before.camera_output.camera.has_value() &&
         (!close(before.camera_output.camera->transform.translation,
                 after.camera_output.camera->transform.translation) ||
          !close(before.camera_output.camera->vertical_fov_degrees,
                 after.camera_output.camera->vertical_fov_degrees)))) {
      return fail("new schema round trip changed persisted film values");
    }
  }

  MovieTimeline empty;
  MovieTimeline empty_round_trip;
  bool empty_migrated = true;
  if (!decodeMovieTimeline(encodeMovieTimeline(empty), world, {},
                           empty_round_trip, empty_migrated, error) ||
      empty_migrated || empty_round_trip.durationFrames() != 0) {
    return fail("zero-duration movie did not round trip");
  }

  MovieTimeline orphan_source;
  TargetSequence orphan_sequence;
  orphan_sequence.id = (TargetSequenceId{1} << 40U) + 5;
  orphan_sequence.name = "Missing actor";
  orphan_sequence.target = {TimelineTargetKind::RiggedEntity, {404}};
  orphan_sequence.captured_base = CapturedEntityBaseState{};
  orphan_source.sequences.push_back(orphan_sequence);
  orphan_source.next_sequence_id = orphan_sequence.id + 1;
  MovieTimeline orphan_round_trip;
  bool orphan_migrated = true;
  if (!decodeMovieTimeline(encodeMovieTimeline(orphan_source), world, {},
                           orphan_round_trip, orphan_migrated, error) ||
      orphan_migrated || orphan_round_trip.sequences.size() != 1 ||
      orphan_round_trip.sequences.front().id != orphan_sequence.id ||
      orphan_round_trip.sequences.front().target.entity.value != 404 ||
      !orphan_round_trip.sequences.front().clips.empty()) {
    return fail("empty orphan sequence did not survive save and reload");
  }

  const auto find_camera_sequence = [](const MovieTimeline& value,
                                       const std::string& name)
      -> const TargetSequence* {
    const auto found = std::find_if(
        value.sequences.begin(), value.sequences.end(),
        [&](const TargetSequence& sequence) {
          return sequence.target.kind == TimelineTargetKind::Camera &&
                 sequence.name == name;
        });
    return found == value.sequences.end() ? nullptr : &*found;
  };
  const auto has_movement_range = [](const TargetSequence& sequence,
                                     FilmFrame start, FilmFrame end) {
    return std::any_of(
        sequence.clips.begin(), sequence.clips.end(),
        [=](const SequenceClip& clip) {
          return clip.start_frame == start && clip.end_frame == end &&
                 std::holds_alternative<MovementClip>(clip.payload);
        });
  };
  const auto migrate_camera_fixture = [&](const std::string& fixture,
                                           MovieTimeline& output) {
    bool fixture_migrated = false;
    return decodeMovieTimeline(fixture, world, {}, output, fixture_migrated,
                               error) &&
           fixture_migrated;
  };

  MovieTimeline no_cuts;
  if (!migrate_camera_fixture(R"json(
      {"tracks":[{"target":7,"clips":[
        {"id":501,"start_frame":0,"end_frame":10,"type":"movement"}
      ]}]})json", no_cuts) ||
      no_cuts.instances.size() != 1 ||
      find_camera_sequence(no_cuts, "Migrated Camera Track") == nullptr ||
      !has_movement_range(*find_camera_sequence(no_cuts, "Migrated Camera Track"),
                          0, 10)) {
    return fail("camera track without cuts was not retained as a runtime sequence");
  }

  MovieTimeline partly_outside;
  if (!migrate_camera_fixture(R"json(
      {"camera_cuts":[{"id":1,"start_frame":2,"end_frame":8,"camera":7}],
       "tracks":[{"target":7,"clips":[
         {"id":502,"start_frame":0,"end_frame":10,"type":"movement"}
       ]}]})json", partly_outside)) {
    return fail("partly outside camera clip migration failed");
  }
  const TargetSequence* partial_remainder =
      find_camera_sequence(partly_outside, "Migrated Camera Remainder");
  if (partial_remainder == nullptr || partly_outside.instances.size() != 1 ||
      !has_movement_range(*partial_remainder, 0, 2) ||
      !has_movement_range(*partial_remainder, 8, 10)) {
    return fail("camera portions outside a cut were discarded");
  }
  MovieTimeline partial_round_trip;
  bool partial_round_trip_migrated = true;
  if (!decodeMovieTimeline(encodeMovieTimeline(partly_outside), world, {},
                           partial_round_trip, partial_round_trip_migrated,
                           error) ||
      partial_round_trip_migrated ||
      find_camera_sequence(partial_round_trip, "Migrated Camera Remainder") ==
          nullptr ||
      !has_movement_range(
          *find_camera_sequence(partial_round_trip,
                                "Migrated Camera Remainder"),
          0, 2) ||
      !has_movement_range(
          *find_camera_sequence(partial_round_trip,
                                "Migrated Camera Remainder"),
          8, 10)) {
    return fail("saving migration lost camera data outside a cut");
  }

  MovieTimeline entirely_outside;
  if (!migrate_camera_fixture(R"json(
      {"camera_cuts":[{"id":1,"start_frame":0,"end_frame":5,"camera":7}],
       "tracks":[{"target":7,"clips":[
         {"id":503,"start_frame":8,"end_frame":12,"type":"movement"}
       ]}]})json", entirely_outside)) {
    return fail("entirely outside camera clip migration failed");
  }
  const TargetSequence* outside_remainder =
      find_camera_sequence(entirely_outside, "Migrated Camera Remainder");
  if (outside_remainder == nullptr ||
      !has_movement_range(*outside_remainder, 8, 12)) {
    return fail("camera clip entirely outside cuts was discarded");
  }

  MovieTimeline multiple_cuts;
  if (!migrate_camera_fixture(R"json(
      {"camera_cuts":[
         {"id":1,"start_frame":0,"end_frame":4,"camera":7},
         {"id":2,"start_frame":8,"end_frame":12,"camera":7}],
       "tracks":[{"target":7,"clips":[
         {"id":504,"start_frame":0,"end_frame":12,"type":"movement"}
       ]}]})json", multiple_cuts)) {
    return fail("multiple-cut camera migration failed");
  }
  const std::size_t migrated_cut_count = static_cast<std::size_t>(std::count_if(
      multiple_cuts.sequences.begin(), multiple_cuts.sequences.end(),
      [](const TargetSequence& sequence) {
        return sequence.name == "Migrated Camera Cut";
      }));
  const TargetSequence* multiple_remainder =
      find_camera_sequence(multiple_cuts, "Migrated Camera Remainder");
  if (migrated_cut_count != 2 || multiple_remainder == nullptr ||
      !has_movement_range(*multiple_remainder, 4, 8)) {
    return fail("one camera track was not preserved across multiple cuts");
  }

  MovieTimeline control_source;
  TargetSequence control_sequence;
  control_sequence.id = 900;
  control_sequence.name = "Custom curve controls";
  control_sequence.target = {TimelineTargetKind::RiggedEntity, {8}};
  control_sequence.captured_base = CapturedEntityBaseState{};
  MovementClip custom_movement;
  custom_movement.curve.automatic_position_controls = false;
  custom_movement.curve.position_control_1 = {1.0f, 2.0f, 3.0f};
  custom_movement.curve.position_control_2 = {4.0f, 5.0f, 6.0f};
  custom_movement.curve.timing_control_1 = 0.2f;
  custom_movement.curve.timing_control_2 = 0.8f;
  control_sequence.clips.push_back({901, 0, 10, custom_movement});
  control_source.sequences.push_back(control_sequence);
  control_source.next_sequence_id = 901;
  control_source.next_clip_id = 902;
  MovieTimeline control_round_trip;
  bool controls_migrated = true;
  if (!decodeMovieTimeline(encodeMovieTimeline(control_source), world, {},
                           control_round_trip, controls_migrated, error) ||
      controls_migrated || control_round_trip.sequences.size() != 1 ||
      control_round_trip.sequences.front().clips.size() != 1) {
    return fail("custom movement controls did not round trip");
  }
  const auto* loaded_movement = std::get_if<MovementClip>(
      &control_round_trip.sequences.front().clips[0].payload);
  if (loaded_movement == nullptr ||
      loaded_movement->curve.automatic_position_controls ||
      !close(loaded_movement->curve.position_control_1,
             custom_movement.curve.position_control_1) ||
      !close(loaded_movement->curve.position_control_2,
             custom_movement.curve.position_control_2) ||
      !close(loaded_movement->curve.timing_control_1, 0.2f) ||
      !close(loaded_movement->curve.timing_control_2, 0.8f)) {
    return fail("custom curve values changed during round trip");
  }

  // Acceptance workflow persistence check: author a reusable Samurai sequence
  // and a camera sequence, place both, then prove save/reload preserves the
  // authoring data and that evaluation still leaves World Edit untouched.
  scene::World acceptance_world;
  const scene::EntityId samurai =
      acceptance_world.createEntityWithId("Samurai", {101});
  acceptance_world.findEntity(samurai)->transform.transform.translation =
      {-8.0f, 0.0f, 3.0f};
  acceptance_world.setRig(samurai, scene::RigComponent{});
  const scene::EntityId acceptance_camera =
      acceptance_world.createEntityWithId("Movie Camera", {102});
  acceptance_world.findEntity(acceptance_camera)->transform.transform.translation =
      {4.0f, 3.0f, 12.0f};
  acceptance_world.setCamera(
      acceptance_camera, scene::CameraComponent{false, 52.0f, 0.2f, 500.0f});
  const math::Transform samurai_world_before =
      acceptance_world.findEntity(samurai)->transform.transform;
  const math::Transform camera_world_before =
      acceptance_world.findEntity(acceptance_camera)->transform.transform;

  MovieTimeline acceptance_timeline;
  TimelineEditService acceptance_edits(acceptance_timeline);
  const TimelineTarget samurai_target{TimelineTargetKind::RiggedEntity, samurai};
  const TimelineTarget camera_target{TimelineTargetKind::Camera, acceptance_camera};
  const auto samurai_sequence = acceptance_edits.createSequence(
      "Samurai shrine approach", samurai_target,
      CapturedEntityBaseState{samurai_world_before, std::nullopt, std::nullopt});
  const auto camera_sequence = acceptance_edits.createSequence(
      "Shrine camera", camera_target,
      CapturedEntityBaseState{camera_world_before,
                              CapturedCameraState{52.0f, 0.2f, 500.0f},
                              std::nullopt});
  MovementClip samurai_movement;
  samurai_movement.end.translation = {2.0f, 0.0f, -4.0f};
  samurai_movement.curve.automatic_position_controls = false;
  samurai_movement.curve.position_control_1 = {-7.0f, 0.0f, -1.0f};
  samurai_movement.curve.position_control_2 = {-1.0f, 0.0f, -5.0f};
  samurai_movement.curve.timing_control_1 = 0.18f;
  samurai_movement.curve.timing_control_2 = 0.84f;
  RigAnimationClip arm_action;
  arm_action.clip_id = 7001;
  arm_action.legacy_clip_index = 3;
  arm_action.source_in = 0.0f;
  arm_action.source_out = 0.8f;  // 0.2 seconds removed from a one-second action.
  arm_action.speed = 0.5f;
  arm_action.looping = false;
  MovementClip camera_movement;
  camera_movement.end.translation = {1.0f, 5.0f, 7.0f};
  camera_movement.curve.automatic_position_controls = false;
  camera_movement.curve.position_control_1 = {5.0f, 8.0f, 10.0f};
  camera_movement.curve.position_control_2 = {-2.0f, 4.0f, 8.0f};
  camera_movement.curve.timing_control_1 = 0.25f;
  camera_movement.curve.timing_control_2 = 0.75f;
  const auto samurai_move_id = samurai_sequence.has_value()
                                   ? acceptance_edits.appendClipToLane(
                                         *samurai_sequence, 60, samurai_movement)
                                   : std::expected<SequenceClipId, std::string>(
                                         std::unexpected("setup failed"));
  const auto arm_action_id = samurai_sequence.has_value()
                                 ? acceptance_edits.appendClipToLane(
                                       *samurai_sequence, 60, arm_action)
                                 : std::expected<SequenceClipId, std::string>(
                                       std::unexpected("setup failed"));
  const auto camera_move_id = camera_sequence.has_value()
                                  ? acceptance_edits.appendClipToLane(
                                        *camera_sequence, 90, camera_movement)
                                  : std::expected<SequenceClipId, std::string>(
                                        std::unexpected("setup failed"));
  const auto samurai_instance = samurai_sequence.has_value()
                                    ? acceptance_edits.placeSequence(
                                          *samurai_sequence, 0)
                                    : std::expected<SequenceInstanceId, std::string>(
                                          std::unexpected("setup failed"));
  const auto camera_instance = camera_sequence.has_value()
                                   ? acceptance_edits.placeSequence(
                                         *camera_sequence, 0)
                                   : std::expected<SequenceInstanceId, std::string>(
                                         std::unexpected("setup failed"));
  if (!samurai_sequence.has_value() || !camera_sequence.has_value() ||
      !samurai_move_id.has_value() || !arm_action_id.has_value() ||
      !camera_move_id.has_value() || !samurai_instance.has_value() ||
      !camera_instance.has_value()) {
    return fail("acceptance workflow authoring failed");
  }
  const FilmFrameState acceptance_preview =
      evaluateMovieTimeline(acceptance_timeline, 30);
  const TransformOverride* acceptance_transform =
      transformFor(acceptance_preview, samurai);
  if (acceptance_transform == nullptr ||
      acceptance_preview.rig_animations.size() != 1 ||
      acceptance_preview.rig_animations.front().animation.clip_id != 7001 ||
      !close(acceptance_preview.rig_animations.front().local_time_seconds, 0.5f) ||
      !acceptance_preview.camera_output.camera.has_value() ||
      acceptance_preview.camera_output.camera->source_entity != acceptance_camera) {
    return fail("acceptance workflow preview did not evaluate authored data");
  }

  MovieTimeline acceptance_reload;
  bool acceptance_migrated = true;
  if (!decodeMovieTimeline(encodeMovieTimeline(acceptance_timeline),
                           acceptance_world, {}, acceptance_reload,
                           acceptance_migrated, error) ||
      acceptance_migrated) {
    return fail("acceptance workflow save and reload failed");
  }
  const TargetSequence* loaded_samurai =
      acceptance_reload.findSequence(*samurai_sequence);
  const TargetSequence* loaded_camera =
      acceptance_reload.findSequence(*camera_sequence);
  const SequenceClip* loaded_samurai_movement =
      acceptance_reload.findClip(*samurai_move_id);
  const SequenceClip* loaded_arm_action = acceptance_reload.findClip(*arm_action_id);
  const SequenceClip* loaded_camera_movement =
      acceptance_reload.findClip(*camera_move_id);
  const auto* loaded_arm = loaded_arm_action == nullptr
                               ? nullptr
                               : std::get_if<RigAnimationClip>(
                                     &loaded_arm_action->payload);
  const auto* loaded_camera_curve = loaded_camera_movement == nullptr
                                        ? nullptr
                                        : std::get_if<MovementClip>(
                                              &loaded_camera_movement->payload);
  if (loaded_samurai == nullptr || loaded_camera == nullptr ||
      loaded_samurai->target != samurai_target ||
      loaded_camera->target != camera_target ||
      loaded_samurai_movement == nullptr || loaded_arm == nullptr ||
      loaded_camera_curve == nullptr || loaded_arm->clip_id != arm_action.clip_id ||
      loaded_arm->legacy_clip_index != arm_action.legacy_clip_index ||
      !close(loaded_arm->source_out, 0.8f) || !close(loaded_arm->speed, 0.5f) ||
      loaded_arm->looping || loaded_samurai_movement->start_frame != 0 ||
      loaded_samurai_movement->end_frame != 60 ||
      loaded_camera_movement->end_frame != 90 ||
      loaded_camera_curve->curve.automatic_position_controls ||
      !close(loaded_camera_curve->curve.position_control_1,
             camera_movement.curve.position_control_1) ||
      !close(loaded_camera_curve->curve.position_control_2,
             camera_movement.curve.position_control_2) ||
      !close(loaded_camera_curve->curve.timing_control_1, 0.25f) ||
      !close(loaded_camera_curve->curve.timing_control_2, 0.75f) ||
      acceptance_reload.instances.size() != 2 ||
      acceptance_reload.instances[0].id != *samurai_instance ||
      acceptance_reload.instances[1].id != *camera_instance) {
    return fail("acceptance workflow reload changed IDs, timing, trim, or curves");
  }
  const FilmFrameState acceptance_reloaded_preview =
      evaluateMovieTimeline(acceptance_reload, 30);
  const TransformOverride* reloaded_transform =
      transformFor(acceptance_reloaded_preview, samurai);
  if (reloaded_transform == nullptr ||
      !close(reloaded_transform->transform.translation,
             acceptance_transform->transform.translation) ||
      !acceptance_reloaded_preview.camera_output.camera.has_value() ||
      acceptance_reloaded_preview.camera_output.camera->source_entity !=
          acceptance_camera ||
      !close(acceptance_world.findEntity(samurai)->transform.transform.translation,
             samurai_world_before.translation) ||
      !close(acceptance_world.findEntity(acceptance_camera)->transform.transform.translation,
             camera_world_before.translation)) {
    return fail("acceptance workflow reload or preview mutated World Edit state");
  }

  return 0;
}
