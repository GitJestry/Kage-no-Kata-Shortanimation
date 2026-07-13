#pragma once

#include "film/film_sequence.hpp"
#include "lighting/light.hpp"
#include "scene/components.hpp"
#include "scene/world.hpp"

namespace kage::lighting {

struct EnvironmentIllumination final {
  glm::vec3 diffuse{0.08f};
  glm::vec3 specular{0.035f};
  float exposure = 1.0f;
};

class LightingSystem final {
 public:
  [[nodiscard]] LightingState extract(
      const scene::World& parWorld, const scene::SunLightSettings& parSun,
      const glm::vec3& parCameraPosition,
      const EnvironmentIllumination& parEnvironment,
      const film::FilmFrameState* parFilmState = nullptr) const;
};

}  // namespace kage::lighting
