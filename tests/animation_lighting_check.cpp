#include "check_helpers.hpp"

#include "animation/animation_system.hpp"
#include "animation/animator.hpp"
#include "lighting/lighting_system.hpp"

#include <glm/gtc/quaternion.hpp>

namespace {

using kage::test::close;
using kage::test::fail;

[[nodiscard]] kage::assets::ModelAsset makeAnimatedAsset() {
  using namespace kage::assets;
  ModelAsset asset;
  asset.nodes.resize(2);
  asset.nodes[0].name = "Root";
  asset.nodes[0].children = {1};
  asset.nodes[1].name = "Joint";
  asset.nodes[1].parent_index = 0;
  asset.nodes[1].local_transform.translation = {0.0f, 1.0f, 0.0f};
  asset.root_nodes = {0};

  GltfSkin skin;
  skin.name = "Skin";
  skin.skeleton_root = 0;
  skin.joints = {1};
  skin.inverse_bind_matrices = {glm::mat4(1.0f)};
  asset.skins.push_back(skin);
  asset.primitive_skin_bindings.push_back({0, glm::mat4(1.0f)});

  AnimationSampler sampler;
  sampler.input_times = {0.0f, 1.0f};
  sampler.translations = {{0.0f, 1.0f, 0.0f}, {2.0f, 1.0f, 0.0f}};
  AnimationClip clip;
  clip.id = 77;
  clip.name = "Move";
  clip.duration_seconds = 1.0f;
  clip.samplers.push_back(sampler);
  clip.channels.push_back({1, AnimationTargetPath::Translation, 0});
  asset.animation_clips.push_back(clip);
  return asset;
}

[[nodiscard]] bool testAnimator() {
  const kage::assets::ModelAsset asset = makeAnimatedAsset();
  const auto bind = kage::animation::Animator::makeBindPose(asset);
  const auto sampled = kage::animation::Animator::sampleClip(asset, 0, 0.5f);
  if (bind.local_transforms.size() != 2 || sampled.local_transforms.size() != 2 ||
      !close(sampled.local_transforms[1].translation.x, 1.0f) ||
      !close(sampled.global_transforms[1][3].x, 1.0f)) {
    return false;
  }
  const auto blended = kage::animation::Animator::blendPoses(asset, bind, sampled, 0.25f);
  const auto joints = kage::animation::Animator::buildJointMatrices(asset, 0, blended);
  return close(blended.local_transforms[1].translation.x, 0.25f) && joints.size() == 1 &&
         close(joints.front()[3].x, 0.25f) &&
         !kage::animation::Animator::buildJointMatrices(asset, 99, bind).size();
}

[[nodiscard]] bool testAnimationSystem() {
  kage::assets::AssetRegistry registry;
  const std::size_t asset_index =
      registry.registerModelAsset("Animated", "animated.gltf", makeAnimatedAsset());
  kage::scene::World world;
  const auto entity = world.createEntity("Rig");
  kage::scene::StaticMeshComponent mesh;
  mesh.asset_library_index = asset_index;
  world.setStaticMesh(entity, mesh);
  world.setRig(entity, {});

  kage::film::FilmFrameState frame;
  kage::film::RigAnimationOverride override;
  override.entity = entity;
  override.animation.clip_id = 77;
  override.local_time_seconds = 0.5f;
  override.weight = 1.0f;
  frame.rig_animations.push_back(override);
  std::vector<kage::animation::EvaluatedSkinPalette> palettes;
  kage::animation::AnimationSystem{}.evaluateFilmFrame(world, registry, frame, palettes);
  return palettes.size() == 1 && palettes.front().entity == entity &&
         palettes.front().primitive_skin_matrices.size() == 1 &&
         palettes.front().primitive_skin_matrices.front().size() == 1 &&
         close(palettes.front().primitive_skin_matrices.front().front()[3].x, 1.0f);
}

[[nodiscard]] bool testLightingRankingAndOverrides() {
  kage::scene::World world;
  for (int index = 0; index < 3; ++index) {
    const auto entity = world.createEntity("Light");
    auto* record = world.findEntity(entity);
    record->transform.transform.translation = {static_cast<float>(index), 0.0f, 0.0f};
    kage::scene::LightComponent light;
    light.intensity = 10.0f - static_cast<float>(index);
    light.range = 10.0f;
    light.casts_shadows = true;
    world.setLight(entity, light);
  }

  kage::scene::SunLightSettings sun;
  sun.direction_to_sun = {0.0f, 2.0f, 0.0f};
  kage::lighting::EnvironmentIllumination environment;
  environment.exposure = 1.5f;
  const auto state =
      kage::lighting::LightingSystem{}.extract(world, sun, {0.0f, 0.0f, 0.0f}, environment);
  if (state.point_light_count != 3 || !state.point_lights[0].casts_shadow ||
      !state.point_lights[1].casts_shadow || state.point_lights[2].casts_shadow ||
      !close(state.sun.direction_to_light.y, 1.0f) || !close(state.exposure, 1.5f)) {
    return false;
  }

  kage::film::FilmFrameState film;
  film.sun = kage::film::EvaluatedSunState{{1.0f, 0.0f, 0.0f}, {0.5f, 0.6f, 0.7f}, 0.0f};
  const auto overridden =
      kage::lighting::LightingSystem{}.extract(world, sun, {0.0f, 0.0f, 0.0f}, environment, &film);
  return !overridden.sun.enabled && close(overridden.sun.intensity, 0.0f);
}

} // namespace

int main() {
  if (!testAnimator()) {
    return fail("Animator sampling, blending, or palette regression");
  }
  if (!testAnimationSystem()) {
    return fail("AnimationSystem film-evaluation regression");
  }
  if (!testLightingRankingAndOverrides()) {
    return fail("lighting extraction or shadow ranking regression");
  }
  return 0;
}
