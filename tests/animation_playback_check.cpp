#include "animation/animator.hpp"
#include "assets/gltf_asset_loader.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

[[nodiscard]] std::size_t findClip(const kage::assets::ModelAsset& parAsset,
                                   std::string_view parName) {
  for (std::size_t index = 0; index < parAsset.animation_clips.size(); ++index) {
    if (parAsset.animation_clips[index].name == parName) {
      return index;
    }
  }
  return static_cast<std::size_t>(-1);
}

[[nodiscard]] float maxMatrixDifference(
    const std::vector<glm::mat4>& parLeft,
    const std::vector<glm::mat4>& parRight) {
  const std::size_t count = std::min(parLeft.size(), parRight.size());
  float difference = 0.0f;
  for (std::size_t matrix_index = 0; matrix_index < count; ++matrix_index) {
    for (int column = 0; column < 4; ++column) {
      for (int row = 0; row < 4; ++row) {
        difference = std::max(
            difference,
            std::abs(parLeft[matrix_index][column][row] -
                     parRight[matrix_index][column][row]));
      }
    }
  }
  return difference;
}

}  // namespace

int main(int parArgumentCount, char** parArguments) {
  if (parArgumentCount != 2) {
    std::cerr << "usage: animation_playback_check <samurai.glb>\n";
    return 2;
  }

  const kage::assets::GltfAssetLoader loader;
  const kage::assets::ModelAsset asset =
      loader.loadDocument(std::filesystem::path(parArguments[1]));
  const std::size_t clip_index = findClip(asset, "ArmAction");
  if (clip_index == static_cast<std::size_t>(-1)) {
    std::cerr << "ArmAction clip was not found\n";
    return 1;
  }
  if (asset.static_model.primitives.empty() || asset.skins.empty()) {
    std::cerr << "asset has no skinned primitive to test\n";
    return 1;
  }

  const kage::assets::StaticPrimitive* skinned_primitive = nullptr;
  for (const kage::assets::StaticPrimitive& primitive :
       asset.static_model.primitives) {
    if (primitive.hasSkinInfluences() &&
        primitive.skin_index != kage::assets::INVALID_SKIN_INDEX) {
      skinned_primitive = &primitive;
      break;
    }
  }
  if (skinned_primitive == nullptr) {
    std::cerr << "asset has no skinned primitive to test\n";
    return 1;
  }

  const float duration = asset.animation_clips[clip_index].duration_seconds;
  const kage::animation::SkeletonPose begin =
      kage::animation::Animator::sampleClip(asset, clip_index, 0.0f);
  const kage::animation::SkeletonPose middle =
      kage::animation::Animator::sampleClip(asset, clip_index,
                                            duration * 0.5f);
  const std::vector<glm::mat4> begin_joints =
      kage::animation::Animator::buildPrimitiveSkinMatrices(
          asset, *skinned_primitive, begin);
  const std::vector<glm::mat4> middle_joints =
      kage::animation::Animator::buildPrimitiveSkinMatrices(
          asset, *skinned_primitive, middle);

  if (maxMatrixDifference(begin_joints, middle_joints) < 0.001f) {
    std::cerr << "ArmAction joint matrices do not change over time\n";
    return 1;
  }

  return 0;
}
