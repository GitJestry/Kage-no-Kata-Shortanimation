#include "film/movie_timeline_serializer.hpp"

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

  return 0;
}
