#include "lighting/lighting_system.hpp"

#include <iostream>

int main() {
  kage::scene::World world;
  for (std::uint32_t id : {9u, 3u, 7u}) {
    const auto entity = world.createEntityWithId(
        "Lantern", kage::scene::EntityId{id});
    auto* record = world.findEntity(entity);
    record->transform.transform.translation = glm::vec3(1.0f, 0.0f, 0.0f);
    kage::scene::LightComponent light;
    light.intensity = 4.0f;
    light.range = 8.0f;
    world.setLight(entity, light);
  }
  kage::film::FilmFrameState frame;
  frame.properties.push_back(
      {kage::scene::EntityId{9},
       kage::film::FilmPropertyKind::LightIntensity, glm::vec4(8.0f)});
  kage::lighting::EnvironmentIllumination environment;
  const kage::lighting::LightingState state =
      kage::lighting::LightingSystem{}.extract(
          world, {}, glm::vec3(0.0f), environment, &frame);
  if (state.point_light_count != 3 ||
      state.point_lights[0].entity_id != 9 ||
      !state.point_lights[0].casts_shadow ||
      !state.point_lights[1].casts_shadow ||
      state.point_lights[2].casts_shadow ||
      state.ambient_diffuse == glm::vec3(0.0f)) {
    std::cerr << "deterministic light extraction/shadow ranking failed\n";
    return 1;
  }
  return 0;
}
