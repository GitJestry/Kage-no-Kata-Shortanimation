#pragma once

#include "assets/asset_registry.hpp"
#include "film/film_frame_state.hpp"
#include "scene/world.hpp"

#include <algorithm>
#include <optional>

namespace kage::animation {

[[nodiscard]] inline std::optional<std::size_t> resolveAnimationClipIndex(
    const assets::ModelAsset& parAsset,
    const film::RigAnimationPlayback& parAnimation) {
  if (parAnimation.clip_id != 0) {
    const auto clip = std::find_if(
        parAsset.animation_clips.begin(), parAsset.animation_clips.end(),
        [&](const assets::AnimationClip& item) {
          return item.id == parAnimation.clip_id;
        });
    if (clip != parAsset.animation_clips.end()) {
      return static_cast<std::size_t>(
          std::distance(parAsset.animation_clips.begin(), clip));
    }
  }
  return parAnimation.legacy_clip_index < parAsset.animation_clips.size()
             ? std::optional<std::size_t>(parAnimation.legacy_clip_index)
             : std::nullopt;
}

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
