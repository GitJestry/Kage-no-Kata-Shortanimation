#include "assets/gltf_asset_loader.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

int main(int parArgumentCount, char** parArguments) {
  if (parArgumentCount < 2) {
    std::cerr << "usage: asset_import_check <asset.glb> [--require-rig] "
                 "[--min-animation-clips count] "
                 "[--min-real-clip-duration seconds] "
                 "[--min-clip-keys count] "
                 "[--max-render-primitives count]\n";
    return 2;
  }

  const std::filesystem::path asset_path = parArguments[1];
  bool require_rig = false;
  std::size_t min_animation_clips = 0;
  float min_real_clip_duration = 0.0f;
  std::size_t min_clip_keys = 0;
  std::size_t max_render_primitives = 0;
  for (int argument_index = 2; argument_index < parArgumentCount;
       ++argument_index) {
    const std::string argument = parArguments[argument_index];
    if (argument == "--require-rig") {
      require_rig = true;
    } else if (argument == "--min-animation-clips" &&
               argument_index + 1 < parArgumentCount) {
      min_animation_clips =
          static_cast<std::size_t>(std::stoull(parArguments[++argument_index]));
    } else if (argument == "--min-real-clip-duration" &&
               argument_index + 1 < parArgumentCount) {
      min_real_clip_duration = std::stof(parArguments[++argument_index]);
    } else if (argument == "--min-clip-keys" &&
               argument_index + 1 < parArgumentCount) {
      min_clip_keys =
          static_cast<std::size_t>(std::stoull(parArguments[++argument_index]));
    } else if (argument == "--max-render-primitives" &&
               argument_index + 1 < parArgumentCount) {
      max_render_primitives =
          static_cast<std::size_t>(std::stoull(parArguments[++argument_index]));
    } else {
      std::cerr << "unknown argument: " << argument << '\n';
      return 2;
    }
  }

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
    if (max_render_primitives > 0 &&
        asset.static_model.primitives.size() > max_render_primitives) {
      std::cerr << asset_path << ": expected at most "
                << max_render_primitives << " optimized primitives, found "
                << asset.static_model.primitives.size() << '\n';
      return 1;
    }

    if (require_rig) {
      if (asset.stats.skin_count == 0 || asset.stats.joint_count == 0 ||
          asset.stats.skinned_vertex_count == 0) {
        std::cerr << asset_path << ": expected a skinned rig\n";
        return 1;
      }
    }
    if (asset.animation_clips.size() < min_animation_clips) {
      std::cerr << asset_path << ": expected at least "
                << min_animation_clips << " animation clips, found "
                << asset.animation_clips.size() << '\n';
      return 1;
    }
    if (min_real_clip_duration > 0.0f || min_clip_keys > 0) {
      bool found_real_clip = false;
      for (const kage::assets::AnimationClip& clip :
           asset.animation_clips) {
        std::size_t key_count = 0;
        for (const kage::assets::AnimationSampler& sampler : clip.samplers) {
          key_count = std::max(key_count, sampler.input_times.size());
        }
        found_real_clip |= clip.duration_seconds >= min_real_clip_duration &&
                           key_count >= min_clip_keys;
      }
      if (!found_real_clip) {
        std::cerr << asset_path
                  << ": no animation clip meets duration/key requirements\n";
        return 1;
      }
    }
    std::cout << asset_path.filename().string() << ": primitives "
              << asset.stats.primitive_count << ", vertices "
              << asset.stats.vertex_count << ", skins "
              << asset.stats.skin_count << ", joints "
              << asset.stats.joint_count << ", animations "
              << asset.stats.animation_count << '\n';
    for (const kage::assets::AnimationClip& clip : asset.animation_clips) {
      std::size_t key_count = 0;
      for (const kage::assets::AnimationSampler& sampler : clip.samplers) {
        key_count = std::max(key_count, sampler.input_times.size());
      }
      std::cout << "- clip "
                << (clip.name.empty() ? "<unnamed>" : clip.name)
                << ": duration " << clip.duration_seconds << "s, max keys "
                << key_count << '\n';
    }
  } catch (const std::exception& error) {
    std::cerr << asset_path << ": " << error.what() << '\n';
    return 1;
  }

  return 0;
}
