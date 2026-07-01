#pragma once

#include "assets/asset_types.hpp"
#include "math/transform.hpp"

#include <glm/glm.hpp>

#include <cstddef>
#include <vector>

namespace kage::animation {

struct Pose final {
  std::vector<math::Transform> local_transforms;
  std::vector<glm::mat4> global_transforms;
};

class Animator final {
 public:
  [[nodiscard]] static Pose makeBindPose(const assets::ModelAsset& parAsset);
  [[nodiscard]] static Pose sampleClip(const assets::ModelAsset& parAsset,
                                       std::size_t parClipIndex,
                                       float parTimeSeconds);
  [[nodiscard]] static std::vector<glm::mat4> buildJointMatrices(
      const assets::ModelAsset& parAsset, std::size_t parSkinIndex,
      const Pose& parPose);

 private:
  [[nodiscard]] static Pose makePoseFromLocals(
      const assets::ModelAsset& parAsset,
      std::vector<math::Transform> parLocalTransforms);
};

}  // namespace kage::animation
