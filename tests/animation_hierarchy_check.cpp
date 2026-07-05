#include "animation/animator.hpp"

#include <glm/glm.hpp>

#include <cmath>
#include <iostream>

namespace {

[[nodiscard]] bool nearlyEqual(const glm::vec3& parLeft,
                               const glm::vec3& parRight) {
  return glm::length(parLeft - parRight) <= 0.0001f;
}

}  // namespace

int main() {
  kage::assets::ModelAsset asset;
  asset.nodes.resize(2);
  asset.root_nodes.push_back(1);

  asset.nodes[0].name = "ChildBeforeParent";
  asset.nodes[0].parent_index = 1;
  asset.nodes[0].local_transform.translation = glm::vec3(0.0f, 5.0f, 0.0f);

  asset.nodes[1].name = "ParentRoot";
  asset.nodes[1].children.push_back(0);
  asset.nodes[1].local_transform.translation = glm::vec3(1.0f, 2.0f, 3.0f);

  const kage::animation::SkeletonPose pose =
      kage::animation::Animator::makeBindPose(asset);
  const glm::vec3 child_world =
      glm::vec3(pose.global_transforms[0] * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
  const glm::vec3 parent_world =
      glm::vec3(pose.global_transforms[1] * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

  if (!nearlyEqual(parent_world, glm::vec3(1.0f, 2.0f, 3.0f))) {
    std::cerr << "parent root transform was not evaluated correctly\n";
    return 1;
  }
  if (!nearlyEqual(child_world, glm::vec3(1.0f, 7.0f, 3.0f))) {
    std::cerr << "child-before-parent hierarchy was evaluated in index order\n";
    return 1;
  }

  return 0;
}
