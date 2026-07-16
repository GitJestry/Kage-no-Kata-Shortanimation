#include "animation/animation_system.hpp"

#include "animation/animator.hpp"

#include <cmath>
#include <utility>

namespace kage::animation {

namespace {

void storePose(std::vector<std::vector<glm::mat4>>& parMatrices,
               const assets::ModelAsset& parAsset,
               const SkeletonPose& parPose) {
  parMatrices.resize(
      parAsset.primitive_skin_bindings.size());
  for (std::size_t primitive_index = 0;
       primitive_index < parAsset.primitive_skin_bindings.size();
       ++primitive_index) {
    const assets::PrimitiveSkinBinding& binding =
        parAsset.primitive_skin_bindings[primitive_index];
    if (binding.skin_index == assets::INVALID_SKIN_INDEX) {
      parMatrices[primitive_index].clear();
      continue;
    }
    parMatrices[primitive_index] =
        Animator::buildJointMatrices(parAsset, binding.skin_index, parPose,
                                     binding.inverse_mesh_bind_transform);
  }
}

}  // namespace

void AnimationSystem::evaluateFilmFrame(
    const scene::World& parWorld,
    const assets::AssetRegistry& parAssetRegistry,
    const film::FilmFrameState& parFrame,
    std::vector<EvaluatedSkinPalette>& parPalettes) const {
  parPalettes.clear();
  for (const scene::EntityRecord& entity : parWorld.getEntities()) {
    if (!entity.alive || !entity.static_mesh.has_value()) {
      continue;
    }
    const assets::ModelAsset* asset = parAssetRegistry.getLoadedAsset(
        entity.static_mesh->asset_library_index);
    if (asset == nullptr || asset->skins.empty()) {
      continue;
    }
    const auto active = std::find_if(
        parFrame.rig_animations.begin(), parFrame.rig_animations.end(),
        [&](const film::RigAnimationOverride& item) {
          return item.entity == entity.id;
        });
    SkeletonPose pose = Animator::makeBindPose(*asset);
    const std::optional<std::size_t> clip_index =
        active != parFrame.rig_animations.end()
            ? resolveAnimationClipIndex(*asset, active->animation)
            : std::nullopt;
    if (active != parFrame.rig_animations.end() && clip_index.has_value()) {
      const float duration = asset->animation_clips[*clip_index].duration_seconds;
      const float source_in =
          duration * std::clamp(active->animation.source_in, 0.0f, 1.0f);
      const float source_out = duration * std::clamp(
          active->animation.source_out, active->animation.source_in, 1.0f);
      const float source_length = std::max(source_out - source_in, 0.0001f);
      float local_time = active->local_time_seconds;
      if (active->final_pose && active->animation.looping) {
        local_time = std::nextafter(local_time, 0.0f);
      }
      float time = source_in + local_time;
      if (active->animation.looping) {
        time = source_in + std::fmod(std::max(time - source_in, 0.0f),
                                    source_length);
      } else {
        time = std::clamp(time, source_in,
                          std::nextafter(source_out, source_in));
      }
      const SkeletonPose animated = Animator::sampleClip(
          *asset, *clip_index, time);
      pose = Animator::blendPoses(*asset, pose, animated, active->weight);
    }
    EvaluatedSkinPalette palette;
    palette.entity = entity.id;
    storePose(palette.primitive_skin_matrices, *asset, pose);
    parPalettes.push_back(std::move(palette));
  }
}

}  // namespace kage::animation
