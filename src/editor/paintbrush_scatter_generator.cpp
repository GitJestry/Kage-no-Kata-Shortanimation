#include "editor/paintbrush_scatter_generator.hpp"

#include <cmath>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <random>

namespace kage::editor {

std::vector<PaintbrushScatterResult> PaintbrushScatterGenerator::generate(
    const PaintbrushSettings& parSettings, const glm::vec3& parCenter,
    const std::vector<std::size_t>& parSelectedAssetIndices, std::uint64_t parSeed) {

  std::vector<PaintbrushScatterResult> results;
  if (parSelectedAssetIndices.empty()) {
    return results;
  }
  std::mt19937_64 rng(parSeed);

  // Guaranteeing a point inside a circle using polar coordinates:
  // r = brush_size * sqrt(random_float_0_to_1) for uniform distribution
  std::uniform_real_distribution<float> random_zero_to_one(0.0f, 1.0f);
  std::uniform_real_distribution<float> rotation_distribution(0.0f, glm::two_pi<float>());
  std::uniform_real_distribution<float> scale_distribution(0.75f, 1.25f);
  std::uniform_int_distribution<std::size_t> asset_distribution(0,
                                                                parSelectedAssetIndices.size() - 1);

  const int spawn_count = std::max(1, parSettings.paint_density);
  results.reserve(spawn_count); // Pre-allocate memory to avoid vector re-allocations

  const float brush_size = static_cast<float>(parSettings.brush_size);

  for (int spawn_index = 0; spawn_index < spawn_count; ++spawn_index) {
    // 1. Math-guaranteed circular distribution (No rejection loop needed!)
    const float theta = rotation_distribution(rng);
    const float radius = brush_size * std::sqrt(random_zero_to_one(rng));

    const glm::vec3 offset(radius * std::cos(theta), 0.0f, radius * std::sin(theta));

    const glm::vec3 position = parCenter + offset;

    // 2. Rotation
    const float yaw_radians = rotation_distribution(rng);
    const glm::quat rotation = parSettings.randomize_rotation
                                   ? glm::angleAxis(yaw_radians, glm::vec3(0.0f, 1.0f, 0.0f))
                                   : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    // 3. Uniform Scaling (Keeps asset proportions intact)
    glm::vec3 scale(1.0f);
    if (parSettings.randomize_scale) {
      const float uniform_scale = scale_distribution(rng);
      scale = glm::vec3(uniform_scale);
    }

    // 4. Asset Selection & Result insertion
    const std::size_t selected_asset_index = parSelectedAssetIndices[asset_distribution(rng)];
    results.push_back({selected_asset_index, kage::math::Transform{position, rotation, scale}});
  }

  return results;
}

} // namespace kage::editor