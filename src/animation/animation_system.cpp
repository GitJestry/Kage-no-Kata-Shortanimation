#include "animation/animation_system.hpp"

#include "animation/animator.hpp"

#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>

namespace {

[[nodiscard]] glm::mat4 getInverseMeshBindTransform(
    const kage::assets::ModelAsset& parAsset,
    const kage::assets::StaticPrimitive& parPrimitive) {
  if (parPrimitive.node_index == kage::assets::INVALID_NODE_INDEX ||
      static_cast<std::size_t>(parPrimitive.node_index) >=
          parAsset.nodes.size()) {
    return glm::inverse(parPrimitive.transform);
  }

  return glm::inverse(
      parAsset.nodes[static_cast<std::size_t>(parPrimitive.node_index)]
          .global_transform);
}

}  // namespace

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

    if (!entity.animation.has_value()) {
      scene::AnimationComponent animation;
      animation.playing = !asset->animation_clips.empty();
      animation.blend_clip_index =
          asset->animation_clips.size() > 1 ? 1 : 0;
      animation.primitive_skin_matrices.resize(
          asset->static_model.primitives.size());
      entity.animation = std::move(animation);
    }

    scene::AnimationComponent& animation = *entity.animation;
    const std::size_t clip_count = asset->animation_clips.size();
    if (clip_count > 0) {
      animation.clip_index = std::min(animation.clip_index, clip_count - 1);
      animation.blend_clip_index =
          std::min(animation.blend_clip_index, clip_count - 1);
      if (animation.playing) {
        const float delta =
            std::max(parDeltaSeconds * animation.playback_speed, 0.0f);
        animation.time_seconds += delta;
        if (animation.blend_duration_seconds > 0.0f) {
          animation.blend_time_seconds += delta;
        }
        if (!animation.looping) {
          const float duration =
              asset->animation_clips[animation.clip_index].duration_seconds;
          animation.time_seconds = std::min(animation.time_seconds, duration);
        }
      }
    }

    Pose pose = clip_count > 0
                    ? Animator::sampleClip(*asset, animation.clip_index,
                                           animation.time_seconds)
                    : Animator::makeBindPose(*asset);
    if (clip_count > 1 && animation.blend_duration_seconds > 0.0f) {
      const float amount = std::clamp(animation.blend_time_seconds /
                                          animation.blend_duration_seconds,
                                      0.0f, 1.0f);
      const Pose blend_pose = Animator::sampleClip(
          *asset, animation.blend_clip_index, animation.time_seconds);
      pose = Animator::blendPoses(*asset, pose, blend_pose, amount);
      if (amount >= 1.0f) {
        animation.clip_index = animation.blend_clip_index;
        animation.blend_time_seconds = 0.0f;
        animation.blend_duration_seconds = 0.0f;
      }
    }

    animation.primitive_skin_matrices.resize(
        asset->static_model.primitives.size());
    for (std::size_t primitive_index = 0;
         primitive_index < asset->static_model.primitives.size();
         ++primitive_index) {
      const assets::StaticPrimitive& primitive =
          asset->static_model.primitives[primitive_index];
      if (primitive.skin_index == assets::INVALID_SKIN_INDEX) {
        animation.primitive_skin_matrices[primitive_index].clear();
        continue;
      }

      animation.primitive_skin_matrices[primitive_index] =
          Animator::buildJointMatrices(
              *asset, primitive.skin_index, pose,
              getInverseMeshBindTransform(*asset, primitive));
    }
  }
}

}  // namespace kage::animation
