#include "film/movie_timeline.hpp"
#include "lighting/lighting_system.hpp"

#include <iostream>

namespace {

int fail(const char* parMessage) {
  std::cerr << parMessage << '\n';
  return 1;
}

}  // namespace

int main() {
  using namespace kage;
  using namespace kage::film;

  scene::World world;
  const scene::EntityId light = world.createEntityWithId("Light", {41});
  scene::LightComponent world_light;
  world_light.enabled = true;
  world_light.intensity = 5.0f;
  world_light.range = 12.0f;
  world_light.casts_shadows = false;
  world.setLight(light, world_light);

  CapturedEntityBaseState captured;
  captured.point_light = CapturedPointLightState{
      true, glm::vec3(0.25f, 0.5f, 1.0f), 3.0f, 9.0f, false};
  TargetSequence sequence;
  sequence.id = 1;
  sequence.target = {TimelineTargetKind::PointLight, light};
  sequence.captured_base = captured;
  sequence.clips.push_back({1, 0, 1, MovementClip{}});
  MovieTimeline timeline;
  timeline.sequences.push_back(sequence);
  timeline.instances.push_back({1, sequence.id, 0});

  const FilmFrameState captured_frame = evaluateMovieTimeline(timeline, 0);
  world.findEntity(light)->light->casts_shadows = true;
  lighting::LightingSystem lighting;
  const lighting::LightingState result = lighting.extract(
      world, {}, {}, {}, &captured_frame);
  if (result.point_light_count != 1 ||
      result.point_lights[0].casts_shadow ||
      result.point_lights[0].intensity != 3.0f ||
      result.point_lights[0].range != 9.0f) {
    return fail("captured point-light state changed after a World edit");
  }
  return 0;
}
