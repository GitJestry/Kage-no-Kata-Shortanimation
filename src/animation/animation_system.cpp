#include "animation/animation_system.hpp"

#include "animation/animator.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace kage::animation {

void AnimationSystem::update(scene::World& parWorld,
                             const assets::AssetRegistry& parAssetRegistry,
                             float parDeltaSeconds) {
  for (scene::EntityRecord& entity : parWorld.getEntities()) {
    if (!entity.alive || !entity.static_mesh.has_value()) {
      continue;
    }

    const assets::ModelAsset* asset =
        parAssetRegistry.getLoadedAsset(entity.static_mesh->asset_library_index);
    if (asset == nullptr || asset->skins.empty()) {
      continue;
    }

    if (!entity.rig.has_value()) {
      scene::RigComponent rig;
      rig.primitive_skin_matrices.resize(
          asset->primitive_skin_bindings.size());
      entity.rig = std::move(rig);
    }

    scene::RigComponent& rig = *entity.rig;
    scene::AnimationPlayerComponent* animation =
        entity.animation_player.has_value() ? &*entity.animation_player
                                            : nullptr;
    const std::size_t clip_count = asset->animation_clips.size();
    if (animation != nullptr && clip_count > 0) {
      animation->clip_index = std::min(animation->clip_index, clip_count - 1);
      animation->blend_clip_index =
          std::min(animation->blend_clip_index, clip_count - 1);
      if (animation->playing) {
        const float delta =
            std::max(parDeltaSeconds * animation->playback_speed, 0.0f);
        animation->time_seconds += delta;
        if (animation->blend_duration_seconds > 0.0f) {
          animation->blend_time_seconds += delta;
        }
        const float duration =
            asset->animation_clips[animation->clip_index].duration_seconds;
        if (duration > 0.0f) {
          if (animation->looping) {
            animation->time_seconds =
                std::fmod(animation->time_seconds, duration);
          } else if (animation->time_seconds >= duration) {
            animation->time_seconds = duration;
            animation->playing = false;
          }
        }
      }
    }

    SkeletonPose pose =
        animation != nullptr && clip_count > 0
            ? Animator::sampleClip(*asset, animation->clip_index,
                                   animation->time_seconds)
            : Animator::makeBindPose(*asset);
    if (animation != nullptr && clip_count > 1 &&
        animation->blend_duration_seconds > 0.0f) {
      const float amount = std::clamp(animation->blend_time_seconds /
                                          animation->blend_duration_seconds,
                                      0.0f, 1.0f);
      const SkeletonPose blend_pose = Animator::sampleClip(
          *asset, animation->blend_clip_index, animation->time_seconds);
      pose = Animator::blendPoses(*asset, pose, blend_pose, amount);
      if (amount >= 1.0f) {
        animation->clip_index = animation->blend_clip_index;
        animation->blend_time_seconds = 0.0f;
        animation->blend_duration_seconds = 0.0f;
      }
    }

    rig.primitive_skin_matrices.resize(asset->primitive_skin_bindings.size());
    for (std::size_t primitive_index = 0;
         primitive_index < asset->primitive_skin_bindings.size();
         ++primitive_index) {
      const assets::PrimitiveSkinBinding& binding =
          asset->primitive_skin_bindings[primitive_index];
      if (binding.skin_index == assets::INVALID_SKIN_INDEX) {
        rig.primitive_skin_matrices[primitive_index].clear();
        continue;
      }

      rig.primitive_skin_matrices[primitive_index] = Animator::buildJointMatrices(
          *asset, binding.skin_index, pose,
          binding.inverse_mesh_bind_transform);
    }
  }
}

}  // namespace kage::animation
