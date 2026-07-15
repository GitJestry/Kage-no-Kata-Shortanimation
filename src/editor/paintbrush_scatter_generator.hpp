#pragma once

#include "editor/paintbrush_settings.hpp"
#include "math/transform.hpp"

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace kage::editor {

struct PaintbrushScatterResult final {
  std::size_t asset_index = 0;
  kage::math::Transform transform{};
};

class PaintbrushScatterGenerator final {
 public:
  static std::vector<PaintbrushScatterResult> generate(
      const PaintbrushSettings& parSettings,
      const glm::vec3& parCenter,
      const std::vector<std::size_t>& parSelectedAssetIndices,
      std::uint64_t parSeed);
};

}  // namespace kage::editor
