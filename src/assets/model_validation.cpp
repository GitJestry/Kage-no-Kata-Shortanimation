#include "assets/model_validation.hpp"

#include "animation/animator.hpp"

#include <glm/common.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace {

[[nodiscard]] bool isFinite(const glm::vec4& parValue) {
  return std::isfinite(parValue.x) && std::isfinite(parValue.y) &&
         std::isfinite(parValue.z) && std::isfinite(parValue.w);
}

[[nodiscard]] bool isFinite(const glm::mat4& parValue) {
  for (int column = 0; column < 4; ++column) {
    if (!isFinite(parValue[column])) {
      return false;
    }
  }
  return true;
}

}  // namespace

namespace kage::assets {

RigValidationReport validateRiggedModelAsset(const ModelAsset& parAsset) {
  RigValidationReport report;
  if (parAsset.skins.empty()) {
    report.errors.push_back("asset has no glTF skin");
  }
  if (parAsset.stats.joint_count == 0) {
    report.errors.push_back("asset has no joints");
  }
  if (parAsset.stats.skinned_vertex_count == 0) {
    report.errors.push_back("asset has no skinned vertices");
  }
  if (parAsset.animation_clips.empty()) {
    report.errors.push_back("asset has no animation clips");
  }

  bool has_positive_duration_clip = false;
  for (const AnimationClip& clip : parAsset.animation_clips) {
    has_positive_duration_clip |= clip.duration_seconds > 0.0f;
  }
  if (!has_positive_duration_clip) {
    report.errors.push_back("asset has no positive-duration animation clip");
  }

  std::size_t largest_joint_count = 0;
  for (const GltfSkin& skin : parAsset.skins) {
    largest_joint_count = std::max(largest_joint_count, skin.joints.size());
    if (skin.inverse_bind_matrices.size() != skin.joints.size()) {
      report.errors.push_back(
          "inverse bind matrix count does not match skin joints");
    }
    for (const std::uint32_t joint_node : skin.joints) {
      if (joint_node >= parAsset.nodes.size()) {
        report.errors.push_back("skin joint node index is out of range");
      }
    }
    for (const glm::mat4& inverse_bind : skin.inverse_bind_matrices) {
      if (!isFinite(inverse_bind)) {
        report.errors.push_back("skin has a non-finite inverse bind matrix");
        break;
      }
    }
  }

  for (const StaticPrimitive& primitive : parAsset.static_model.primitives) {
    if (!primitive.hasSkinInfluences()) {
      continue;
    }
    if (primitive.skin_index == INVALID_SKIN_INDEX ||
        primitive.skin_index >= parAsset.skins.size()) {
      report.errors.push_back("skinned primitive has no valid skin index");
    }
    for (const SkinInfluence& influence : primitive.skin_influences) {
      if (!isFinite(influence.weights)) {
        report.errors.push_back("skin weights contain non-finite values");
        break;
      }
      const float weight_sum = influence.weights.x + influence.weights.y +
                               influence.weights.z + influence.weights.w;
      if (std::abs(weight_sum - 1.0f) > 0.02f) {
        report.errors.push_back("skin weights are not normalized");
        break;
      }
      for (int component = 0; component < 4; ++component) {
        if (influence.joints[component] >= largest_joint_count) {
          report.errors.push_back("joint influence index is out of range");
          break;
        }
      }
    }
  }

  const animation::Pose bind_pose = animation::Animator::makeBindPose(parAsset);
  for (const glm::mat4& global_transform : bind_pose.global_transforms) {
    if (!isFinite(global_transform)) {
      report.errors.push_back("bind pose contains non-finite transforms");
      break;
    }
  }

  if (!parAsset.animation_clips.empty() && !parAsset.skins.empty()) {
    const animation::Pose sampled_pose = animation::Animator::sampleClip(
        parAsset, 0, parAsset.animation_clips.front().duration_seconds * 0.5f);
    const std::vector<glm::mat4> joint_matrices =
        animation::Animator::buildJointMatrices(parAsset, 0, sampled_pose);
    if (joint_matrices.empty()) {
      report.errors.push_back("sampled clip produced no joint matrices");
    }
    for (const glm::mat4& joint_matrix : joint_matrices) {
      if (!isFinite(joint_matrix)) {
        report.errors.push_back("sampled clip produced non-finite joint matrix");
        break;
      }
    }
  }

  return report;
}

}  // namespace kage::assets
