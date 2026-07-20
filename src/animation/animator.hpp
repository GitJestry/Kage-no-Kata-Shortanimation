#pragma once

#include "assets/asset_types.hpp"
#include "math/transform.hpp"

#include <glm/glm.hpp>

#include <cstddef>
#include <vector>

namespace kage::animation {

struct SkeletonPose final {
  std::vector<math::Transform> local_transforms;
  std::vector<glm::mat4> global_transforms;
};

class Animator final {
 public:
  [[nodiscard]] static SkeletonPose makeBindPose(const assets::ModelAsset& parAsset);
  [[nodiscard]] static SkeletonPose sampleClip(const assets::ModelAsset& parAsset,
                                       std::size_t parClipIndex,
                                       float parTimeSeconds);
  [[nodiscard]] static SkeletonPose blendPoses(const assets::ModelAsset& parAsset,
                                       const SkeletonPose& parFirst,
                                       const SkeletonPose& parSecond,
                                       float parAmount);
  [[nodiscard]] static std::vector<glm::mat4> buildJointMatrices(
      const assets::ModelAsset& parAsset, std::size_t parSkinIndex,
      const SkeletonPose& parPose,
      const glm::mat4& parInverseMeshTransform = glm::mat4(1.0f));

 private:
  [[nodiscard]] static SkeletonPose makePoseFromLocals(
      const assets::ModelAsset& parAsset,
      std::vector<math::Transform> parLocalTransforms);
};

}  // namespace kage::animation
