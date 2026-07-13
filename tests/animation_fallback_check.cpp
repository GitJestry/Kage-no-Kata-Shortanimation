#include "animation/animation_system.hpp"

#include <iostream>

namespace {

int fail(const char* parMessage) {
  std::cerr << parMessage << '\n';
  return 1;
}

}  // namespace

int main() {
  using namespace kage;
  assets::ModelAsset asset;
  asset.animation_clips = {{10, "First"}, {20, "Second"}};

  film::RigAnimationPlayback valid_id;
  valid_id.clip_id = 20;
  valid_id.legacy_clip_index = 0;
  if (animation::resolveAnimationClipIndex(asset, valid_id) != 1) {
    return fail("stable animation ID was not preferred");
  }

  film::RigAnimationPlayback stale_id;
  stale_id.clip_id = 999;
  stale_id.legacy_clip_index = 0;
  if (animation::resolveAnimationClipIndex(asset, stale_id) != 0) {
    return fail("stale animation ID did not fall back to the legacy index");
  }

  film::RigAnimationPlayback invalid;
  invalid.clip_id = 999;
  invalid.legacy_clip_index = 7;
  if (animation::resolveAnimationClipIndex(asset, invalid).has_value()) {
    return fail("invalid animation references did not select bind-pose fallback");
  }
  return 0;
}
