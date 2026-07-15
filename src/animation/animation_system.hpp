#pragma once

#include "assets/asset_registry.hpp"
#include "film/film_sequence.hpp"
#include "scene/world.hpp"

namespace kage::animation {

struct EvaluatedSkinPalette final {
  scene::EntityId entity;
  std::vector<std::vector<glm::mat4>> primitive_skin_matrices;
};

class AnimationSystem final {
 public:
  void evaluateFilmFrame(
      const scene::World& parWorld,
      const assets::AssetRegistry& parAssetRegistry,
      const film::FilmFrameState& parFrame,
      std::vector<EvaluatedSkinPalette>& parPalettes) const;
};

}  // namespace kage::animation
