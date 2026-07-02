#include "animation/animator.hpp"
#include "assets/gltf_asset_loader.hpp"
#include "assets/model_validation.hpp"
#include "math/bounds.hpp"

#include <glm/glm.hpp>

#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

[[nodiscard]] bool isFinite(const glm::vec3& parValue) {
  return std::isfinite(parValue.x) && std::isfinite(parValue.y) &&
         std::isfinite(parValue.z);
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

[[nodiscard]] bool hasFiniteSkinnedBounds(
    const kage::assets::ModelAsset& parAsset, std::string& parError) {
  const kage::animation::Pose pose =
      parAsset.animation_clips.empty()
          ? kage::animation::Animator::makeBindPose(parAsset)
          : kage::animation::Animator::sampleClip(
                parAsset, 0, parAsset.animation_clips[0].duration_seconds * 0.5f);
  kage::math::Bounds3 bounds;
  for (const kage::assets::StaticPrimitive& primitive :
       parAsset.static_model.primitives) {
    if (!primitive.hasSkinInfluences() ||
        primitive.skin_index == kage::assets::INVALID_SKIN_INDEX) {
      continue;
    }

    const std::vector<glm::mat4> joints =
        kage::animation::Animator::buildJointMatrices(
            parAsset, primitive.skin_index, pose,
            getInverseMeshBindTransform(parAsset, primitive));
    if (joints.empty()) {
      parError = "skinned primitive has no joint matrix palette";
      return false;
    }

    for (std::size_t vertex_index = 0; vertex_index < primitive.vertices.size();
         ++vertex_index) {
      const kage::assets::SkinInfluence& influence =
          primitive.skin_influences[vertex_index];
      glm::mat4 skin(0.0f);
      for (int weight_index = 0; weight_index < 4; ++weight_index) {
        const std::uint32_t joint = influence.joints[weight_index];
        if (joint >= joints.size()) {
          parError = "joint index exceeds sampled palette";
          return false;
        }
        skin += joints[joint] * influence.weights[weight_index];
      }

      const glm::vec3 position = glm::vec3(
          primitive.transform * skin *
          glm::vec4(primitive.vertices[vertex_index].position, 1.0f));
      if (!isFinite(position)) {
        parError = "sampled skinned position is not finite";
        return false;
      }
      bounds.includePoint(position);
    }
  }

  if (!bounds.is_valid) {
    parError = "no skinned vertices were sampled";
    return false;
  }

  const float diagonal = glm::length(bounds.getSize());
  if (!std::isfinite(diagonal) || diagonal <= 0.05f) {
    parError = "sampled skinned bounds collapsed";
    return false;
  }
  return true;
}

}  // namespace

int main(int parArgumentCount, char** parArguments) {
  if (parArgumentCount < 2) {
    std::cerr << "usage: asset_import_check <asset.glb> [--require-rig]\n";
    return 2;
  }

  const std::filesystem::path asset_path = parArguments[1];
  const bool require_rig =
      parArgumentCount >= 3 && std::string(parArguments[2]) == "--require-rig";

  try {
    const kage::assets::GltfAssetLoader loader;
    const kage::assets::ModelAsset asset = loader.loadDocument(asset_path);
    if (asset.static_model.primitives.empty()) {
      std::cerr << asset_path << ": no mesh primitives imported\n";
      return 1;
    }
    if (asset.static_model.stats.vertex_count == 0 ||
        asset.static_model.stats.index_count == 0) {
      std::cerr << asset_path << ": no renderable vertex/index data\n";
      return 1;
    }

    if (require_rig) {
      const kage::assets::RigValidationReport report =
          kage::assets::validateRiggedModelAsset(asset);
      if (!report.passed()) {
        std::cerr << asset_path << ": rig validation failed\n";
        for (const std::string& error : report.errors) {
          std::cerr << "- " << error << '\n';
        }
        return 1;
      }
      std::string skinning_error;
      if (!hasFiniteSkinnedBounds(asset, skinning_error)) {
        std::cerr << asset_path << ": skinning smoke test failed: "
                  << skinning_error << '\n';
        return 1;
      }
    }

    std::cout << asset_path.filename().string() << ": primitives "
              << asset.stats.primitive_count << ", vertices "
              << asset.stats.vertex_count << ", skins "
              << asset.stats.skin_count << ", joints "
              << asset.stats.joint_count << ", animations "
              << asset.stats.animation_count << '\n';
  } catch (const std::exception& error) {
    std::cerr << asset_path << ": " << error.what() << '\n';
    return 1;
  }

  return 0;
}
