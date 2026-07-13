#include "lighting/lighting_system.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace kage::lighting {

LightingState LightingSystem::extract(
    const scene::World& parWorld, const scene::SunLightSettings& parSun,
    const glm::vec3& parCameraPosition,
    const EnvironmentIllumination& parEnvironment,
    const film::FilmFrameState* parFilmState) const {
  LightingState state;
  state.ambient_diffuse = parEnvironment.diffuse;
  state.ambient_specular = parEnvironment.specular;
  state.exposure = parEnvironment.exposure;
  state.sun.enabled = parSun.enabled && parSun.intensity > 0.0f;
  const float direction_length = glm::length(parSun.direction_to_sun);
  state.sun.direction_to_light =
      direction_length > 0.0001f
          ? parSun.direction_to_sun / direction_length
          : glm::vec3(0.35f, 0.85f, 0.45f);
  state.sun.color = parSun.color;
  state.sun.intensity = std::max(parSun.intensity, 0.0f);
  if (parFilmState != nullptr && parFilmState->sun.has_value()) {
    const film::EvaluatedSunState& sun = *parFilmState->sun;
    const float evaluated_length = glm::length(sun.direction_to_sun);
    state.sun.direction_to_light =
        evaluated_length > 0.0001f
            ? sun.direction_to_sun / evaluated_length
            : state.sun.direction_to_light;
    state.sun.color = glm::max(sun.color, glm::vec3(0.0f));
    state.sun.intensity = std::max(sun.intensity, 0.0f);
    state.sun.enabled = state.sun.intensity > 0.0f;
  }

  struct Candidate final {
    PointLight light;
    bool wants_shadow = false;
    float influence = 0.0f;
  };
  thread_local std::vector<Candidate> candidates;
  candidates.clear();
  for (const scene::EntityRecord& entity : parWorld.getEntities()) {
    if (!entity.alive || !entity.light.has_value() ||
        entity.light->type != scene::LightType::Point) {
      continue;
    }
    scene::LightComponent source = *entity.light;
    if (parFilmState != nullptr) {
      for (const film::PropertyOverride& property :
           parFilmState->properties) {
        if (property.entity != entity.id) {
          continue;
        }
        switch (property.kind) {
          case film::FilmPropertyKind::LightEnabled:
            source.enabled = property.value.x >= 0.5f;
            break;
          case film::FilmPropertyKind::LightIntensity:
            source.intensity = std::max(property.value.x, 0.0f);
            break;
          case film::FilmPropertyKind::LightColor:
            source.color = glm::max(glm::vec3(property.value), glm::vec3(0.0f));
            break;
          case film::FilmPropertyKind::LightRange:
            source.range = std::max(property.value.x, 0.001f);
            break;
          case film::FilmPropertyKind::LightCastsShadows:
            source.casts_shadows = property.value.x >= 0.5f;
            break;
          case film::FilmPropertyKind::CameraFov:
            break;
        }
      }
    }
    if (!source.enabled || source.intensity <= 0.0f) {
      continue;
    }
    Candidate candidate;
    candidate.light.enabled = true;
    candidate.light.entity_id = entity.id.value;
    candidate.light.position = entity.transform.transform.translation;
    if (parFilmState != nullptr) {
      const auto transform = std::find_if(
          parFilmState->transforms.begin(), parFilmState->transforms.end(),
          [&](const film::TransformOverride& item) {
            return item.entity == entity.id;
          });
      if (transform != parFilmState->transforms.end()) {
        candidate.light.position = transform->transform.translation;
      }
    }
    candidate.light.color = source.color;
    candidate.light.intensity = source.intensity;
    candidate.light.range = source.range;
    candidate.wants_shadow = source.casts_shadows;
    const glm::vec3 delta = candidate.light.position - parCameraPosition;
    candidate.influence = source.intensity * source.range * source.range /
                          (1.0f + glm::dot(delta, delta));
    candidates.push_back(candidate);
  }
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& left, const Candidate& right) {
              if (std::abs(left.influence - right.influence) > 0.000001f) {
                return left.influence > right.influence;
              }
              return left.light.entity_id < right.light.entity_id;
            });
  std::size_t shadow_count = 0;
  state.point_light_count =
      std::min(candidates.size(), state.point_lights.size());
  for (std::size_t index = 0; index < state.point_light_count; ++index) {
    state.point_lights[index] = candidates[index].light;
    if (candidates[index].wants_shadow &&
        shadow_count < MAX_POINT_SHADOWS) {
      state.point_lights[index].casts_shadow = true;
      ++shadow_count;
    }
  }
  return state;
}

}  // namespace kage::lighting
