#include "assets/gltf_asset_loader.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <unordered_set>

namespace {

struct ExpectedAsset final {
  const char* filename;
  std::size_t vertices;
  std::size_t indices;
  std::size_t mask_materials;
  std::size_t blend_materials;
  bool require_double_sided;
  bool require_large_texture;
};

constexpr std::array EXPECTED = {
    ExpectedAsset{"house.glb", 1968260, 5533824, 113, 0, false, false},
    ExpectedAsset{"banner.glb", 59300, 60876, 0, 1, true, true},
    ExpectedAsset{"banner_pole.glb", 49918, 51036, 0, 1, true, true},
    ExpectedAsset{"grasspatch.glb", 248282, 587091, 4, 0, true, false},
    ExpectedAsset{"tree_yellow_full.glb", 789383, 1910748, 1, 0, true,
                  true},
    ExpectedAsset{"vase.glb", 51817, 234726, 0, 2, true, false},
    ExpectedAsset{"shrine.glb", 58551, 63600, 0, 0, false, false},
    ExpectedAsset{"samurai.glb", 126600, 576315, 0, 2, true, true},
};

}  // namespace

int main(int parArgumentCount, char** parArguments) {
  if (parArgumentCount != 2) {
    std::cerr << "usage: asset_fidelity_check <model-directory>\n";
    return 2;
  }
  const std::filesystem::path directory = parArguments[1];
  const kage::assets::GltfAssetLoader loader;
  for (const ExpectedAsset& expected : EXPECTED) {
    const kage::assets::ModelAsset asset =
        loader.loadDocument(directory / expected.filename);
    const auto& model = asset.static_model;
    std::unordered_set<kage::assets::AnimationClipId> clip_ids;
    for (const kage::assets::AnimationClip& clip : asset.animation_clips) {
      if (clip.id == 0 || !clip_ids.insert(clip.id).second) {
        std::cerr << expected.filename
                  << ": animation clip IDs are not stable and unique\n";
        return 1;
      }
    }
    const std::size_t vertices = std::accumulate(
        model.primitives.begin(), model.primitives.end(), std::size_t{0},
        [](std::size_t total, const kage::assets::StaticPrimitive& primitive) {
          return total + primitive.vertices.size();
        });
    const std::size_t indices = std::accumulate(
        model.primitives.begin(), model.primitives.end(), std::size_t{0},
        [](std::size_t total, const kage::assets::StaticPrimitive& primitive) {
          return total + primitive.indices.size();
        });
    std::size_t masks = 0;
    std::size_t blends = 0;
    bool double_sided = false;
    for (const kage::assets::StaticMaterial& material : model.materials) {
      masks += material.alpha_mode == kage::assets::AlphaMode::Mask;
      blends += material.alpha_mode == kage::assets::AlphaMode::Blend;
      double_sided |= material.double_sided;
    }
    int largest_texture = 0;
    for (const kage::assets::StaticImage& image : model.images) {
      largest_texture =
          std::max({largest_texture, image.width, image.height});
      const std::size_t expected_bytes =
          static_cast<std::size_t>(image.width) * image.height *
          image.component_count;
      if (image.width <= 0 || image.height <= 0 ||
          image.pixels.size() < expected_bytes) {
        std::cerr << expected.filename << ": incomplete source image\n";
        return 1;
      }
    }
    if (vertices != expected.vertices || indices != expected.indices ||
        vertices != model.stats.vertex_count ||
        indices != model.stats.index_count || masks != expected.mask_materials ||
        blends != expected.blend_materials ||
        (expected.require_double_sided && !double_sided) ||
        (expected.require_large_texture && largest_texture <= 1024)) {
      std::cerr << expected.filename
                << ": authored geometry/material/texture fidelity changed"
                << " vertices=" << vertices << " indices=" << indices
                << " stats=" << model.stats.vertex_count << "/"
                << model.stats.index_count << " mask=" << masks
                << " blend=" << blends << " double=" << double_sided
                << " texture=" << largest_texture << '\n';
      return 1;
    }
  }
  return 0;
}
