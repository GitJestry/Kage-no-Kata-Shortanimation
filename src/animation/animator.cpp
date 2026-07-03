#include "animation/animator.hpp"

#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <vector>

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

  const auto next = std::upper_bound(parTimes.begin(), parTimes.end(),
                                     parTimeSeconds);
  if (next == parTimes.begin()) {
    return 0;
  }
  return std::min<std::size_t>(
      static_cast<std::size_t>(std::distance(parTimes.begin(), next) - 1),
      parTimes.size() - 1);
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

SkeletonPose Animator::makeBindPose(const assets::ModelAsset& parAsset) {
  std::vector<math::Transform> locals;
  locals.reserve(parAsset.nodes.size());
  for (const assets::GltfNode& node : parAsset.nodes) {
    locals.push_back(node.local_transform);
  }
  return makePoseFromLocals(parAsset, std::move(locals));
}

SkeletonPose Animator::sampleClip(const assets::ModelAsset& parAsset,
                          std::size_t parClipIndex, float parTimeSeconds) {
  SkeletonPose pose = makeBindPose(parAsset);
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

SkeletonPose Animator::blendPoses(const assets::ModelAsset& parAsset,
                          const SkeletonPose& parFirst,
                          const SkeletonPose& parSecond,
                          float parAmount) {
  const std::size_t count =
      std::min(parFirst.local_transforms.size(),
               parSecond.local_transforms.size());
  std::vector<math::Transform> locals;
  locals.reserve(parAsset.nodes.size());
  for (const assets::GltfNode& node : parAsset.nodes) {
    locals.push_back(node.local_transform);
  }
  const float amount = std::clamp(parAmount, 0.0f, 1.0f);

  for (std::size_t index = 0; index < std::min(count, locals.size());
       ++index) {
    const math::Transform& first = parFirst.local_transforms[index];
    const math::Transform& second = parSecond.local_transforms[index];
    math::Transform transform;
    transform.translation =
        glm::mix(first.translation, second.translation, amount);
    transform.rotation =
        glm::normalize(glm::slerp(first.rotation, second.rotation, amount));
    transform.scale = glm::mix(first.scale, second.scale, amount);
    locals[index] = transform;
  }

  return makePoseFromLocals(parAsset, std::move(locals));
}

std::vector<glm::mat4> Animator::buildJointMatrices(
    const assets::ModelAsset& parAsset, std::size_t parSkinIndex,
    const SkeletonPose& parPose, const glm::mat4& parInverseMeshTransform) {
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
    matrices.push_back(parInverseMeshTransform *
                       parPose.global_transforms[node_index] *
                       skin.inverse_bind_matrices[joint_index]);
  }
  return matrices;
}

std::vector<glm::mat4> Animator::buildPrimitiveSkinMatrices(
    const assets::ModelAsset& parAsset,
    const assets::StaticPrimitive& parPrimitive,
    const SkeletonPose& parPose) {
  if (parPrimitive.skin_index == assets::INVALID_SKIN_INDEX) {
    return {};
  }

  return buildJointMatrices(parAsset, parPrimitive.skin_index, parPose,
                            getInverseMeshBindTransform(parAsset,
                                                        parPrimitive));
}

SkeletonPose Animator::makePoseFromLocals(
    const assets::ModelAsset& parAsset,
    std::vector<math::Transform> parLocalTransforms) {
  SkeletonPose pose;
  pose.local_transforms = std::move(parLocalTransforms);
  if (pose.local_transforms.size() < parAsset.nodes.size()) {
    pose.local_transforms.reserve(parAsset.nodes.size());
    for (std::size_t index = pose.local_transforms.size();
         index < parAsset.nodes.size(); ++index) {
      pose.local_transforms.push_back(parAsset.nodes[index].local_transform);
    }
  }
  if (pose.local_transforms.size() > parAsset.nodes.size()) {
    pose.local_transforms.resize(parAsset.nodes.size());
  }

  pose.global_transforms.resize(parAsset.nodes.size(), glm::mat4(1.0f));
  enum class VisitState {
    Unvisited,
    Visiting,
    Done
  };
  std::vector<VisitState> states(parAsset.nodes.size(), VisitState::Unvisited);

  const auto compute_node = [&](const auto& self,
                                std::size_t parNodeIndex) -> void {
    if (parNodeIndex >= parAsset.nodes.size()) {
      return;
    }
    if (states[parNodeIndex] == VisitState::Done) {
      return;
    }

    const glm::mat4 local = pose.local_transforms[parNodeIndex].toMatrix();
    if (states[parNodeIndex] == VisitState::Visiting) {
      pose.global_transforms[parNodeIndex] = local;
      states[parNodeIndex] = VisitState::Done;
      return;
    }

    states[parNodeIndex] = VisitState::Visiting;
    const std::uint32_t parent = parAsset.nodes[parNodeIndex].parent_index;
    if (parent != assets::INVALID_NODE_INDEX &&
        static_cast<std::size_t>(parent) < parAsset.nodes.size() &&
        parent != parNodeIndex) {
      self(self, static_cast<std::size_t>(parent));
      pose.global_transforms[parNodeIndex] =
          pose.global_transforms[static_cast<std::size_t>(parent)] * local;
    } else {
      pose.global_transforms[parNodeIndex] = local;
    }
    states[parNodeIndex] = VisitState::Done;
  };

  for (const std::uint32_t root_node : parAsset.root_nodes) {
    compute_node(compute_node, static_cast<std::size_t>(root_node));
  }
  for (std::size_t node_index = 0; node_index < parAsset.nodes.size();
       ++node_index) {
    compute_node(compute_node, node_index);
  }

  return pose;
}

}  // namespace kage::animation
