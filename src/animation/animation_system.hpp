#pragma once

#include "assets/asset_registry.hpp"
#include "scene/world.hpp"

namespace kage::animation {

class AnimationSystem final {
 public:
  void update(scene::World& parWorld,
              const assets::AssetRegistry& parAssetRegistry,
              float parDeltaSeconds);
};

}  // namespace kage::animation
