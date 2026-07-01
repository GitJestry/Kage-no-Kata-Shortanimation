#include "animation/animator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

[[nodiscard]] float wrapClipTime(float parTimeSeconds, float parDuration) {
  if (parDuration <= 0.0f) {
    return 0.0f;
  }

  float time = std::fmod(parTimeSeconds, parDuration);
  return time < 0.0f ? time + parDuration : time;
}

[[nodiscard]] std::size_t findKeyframe(
    const std::vector<float>& parTimes, float parTimeSeconds) {
  if (parTimes.size() <= 1 || parTimeSeconds <= parTimes.front()) {
    return 0;
  }

  for (std::size_t index = 0; index + 1 < parTimes.size(); ++index) {
    if (parTimeSeconds < parTimes[index + 1]) {
      return index;
    }
  }
  return parTimes.size() - 1;
}

[[nodiscard]] float getInterpolationAmount(const std::vector<float>& parTimes,
                                           std::size_t parIndex,
                                           float parTimeSeconds) {
  if (parIndex + 1 >= parTimes.size()) {
    return 0.0f;
  }

  const float begin = parTimes[parIndex];
  const float end = parTimes[parIndex + 1];
  if (end <= begin) {
    return 0.0f;
  }

  return std::clamp((parTimeSeconds - begin) / (end - begin), 0.0f, 1.0f);
}

}  // namespace

namespace kage::animation {

Pose Animator::makeBindPose(const assets::ModelAsset& parAsset) {
  std::vector<math::Transform> locals;
  locals.reserve(parAsset.nodes.size());
  for (const assets::GltfNode& node : parAsset.nodes) {
    locals.push_back(node.local_transform);
  }
  return makePoseFromLocals(parAsset, std::move(locals));
}

Pose Animator::sampleClip(const assets::ModelAsset& parAsset,
                          std::size_t parClipIndex, float parTimeSeconds) {
  Pose pose = makeBindPose(parAsset);
  if (parClipIndex >= parAsset.animation_clips.size()) {
    return pose;
  }

  const assets::AnimationClip& clip = parAsset.animation_clips[parClipIndex];
  const float time = wrapClipTime(parTimeSeconds, clip.duration_seconds);
  for (const assets::AnimationChannel& channel : clip.channels) {
    if (channel.target_node >= pose.local_transforms.size() ||
        channel.sampler_index >= clip.samplers.size()) {
      continue;
    }

    const assets::AnimationSampler& sampler =
        clip.samplers[channel.sampler_index];
    if (sampler.input_times.empty()) {
      continue;
    }

    const std::size_t keyframe = findKeyframe(sampler.input_times, time);
    const std::size_t next_keyframe =
        std::min(keyframe + 1, sampler.input_times.size() - 1);
    const bool step =
        sampler.interpolation == assets::AnimationInterpolation::Step;
    const float amount =
        step ? 0.0f : getInterpolationAmount(sampler.input_times, keyframe, time);
    math::Transform& transform = pose.local_transforms[channel.target_node];

    switch (channel.target_path) {
      case assets::AnimationTargetPath::Translation:
        if (next_keyframe < sampler.translations.size()) {
          transform.translation =
              glm::mix(sampler.translations[keyframe],
                       sampler.translations[next_keyframe], amount);
        }
        break;
      case assets::AnimationTargetPath::Rotation:
        if (next_keyframe < sampler.rotations.size()) {
          transform.rotation = glm::normalize(glm::slerp(
              sampler.rotations[keyframe], sampler.rotations[next_keyframe],
              amount));
        }
        break;
      case assets::AnimationTargetPath::Scale:
        if (next_keyframe < sampler.scales.size()) {
          transform.scale =
              glm::mix(sampler.scales[keyframe], sampler.scales[next_keyframe],
                       amount);
        }
        break;
    }
  }

  return makePoseFromLocals(parAsset, std::move(pose.local_transforms));
}

std::vector<glm::mat4> Animator::buildJointMatrices(
    const assets::ModelAsset& parAsset, std::size_t parSkinIndex,
    const Pose& parPose) {
  if (parSkinIndex >= parAsset.skins.size()) {
    return {};
  }

  const assets::GltfSkin& skin = parAsset.skins[parSkinIndex];
  std::vector<glm::mat4> matrices;
  matrices.reserve(skin.joints.size());
  for (std::size_t joint_index = 0; joint_index < skin.joints.size();
       ++joint_index) {
    const std::uint32_t node_index = skin.joints[joint_index];
    if (node_index >= parPose.global_transforms.size() ||
        joint_index >= skin.inverse_bind_matrices.size()) {
      matrices.push_back(glm::mat4(1.0f));
      continue;
    }
    matrices.push_back(parPose.global_transforms[node_index] *
                       skin.inverse_bind_matrices[joint_index]);
  }
  return matrices;
}

Pose Animator::makePoseFromLocals(const assets::ModelAsset& parAsset,
                                  std::vector<math::Transform> parLocalTransforms) {
  Pose pose;
  pose.local_transforms = std::move(parLocalTransforms);
  pose.global_transforms.resize(pose.local_transforms.size(), glm::mat4(1.0f));
  for (std::size_t node_index = 0; node_index < parAsset.nodes.size();
       ++node_index) {
    const glm::mat4 local = pose.local_transforms[node_index].toMatrix();
    const std::uint32_t parent = parAsset.nodes[node_index].parent_index;
    pose.global_transforms[node_index] =
        parent != assets::INVALID_NODE_INDEX &&
                parent < pose.global_transforms.size()
            ? pose.global_transforms[parent] * local
            : local;
  }
  return pose;
}

}  // namespace kage::animation
