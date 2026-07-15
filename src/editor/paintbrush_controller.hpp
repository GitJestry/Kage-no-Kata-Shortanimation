#pragma once

#include "editor/paintbrush_settings.hpp"
#include "input/input_events.hpp"
#include "math/transform.hpp"

#include <glm/glm.hpp>

#include <functional>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace kage::editor {

class PaintbrushController final {
 public:
  using PaintCallback = std::function<void(std::size_t, const kage::math::Transform&)>;

  explicit PaintbrushController(PaintCallback parPaintCallback = {});

  void setPaintCallback(PaintCallback parPaintCallback);
  bool processInput(const glm::vec3& parWorldPosition,
                    const kage::input::EditorInputSnapshot& parInput,
                    const PaintbrushSettings& parSettings,
                    const std::vector<std::size_t>& parSelectedAssetIndices);
  void resetStroke();

 private:
  bool paintStampAt(const glm::vec3& parPosition,
                    const PaintbrushSettings& parSettings,
                    const std::vector<std::size_t>& parSelectedAssetIndices);
  void paintPathSegment(const glm::vec3& parFrom,
                        const glm::vec3& parTo,
                        const PaintbrushSettings& parSettings,
                        const std::vector<std::size_t>& parSelectedAssetIndices);
  static float getStampSpacing(int parBrushSize);

  PaintCallback m_paint_callback;
  glm::vec3 m_last_world_position{0.0f};
  glm::vec3 m_last_stamp_position{0.0f};
  bool m_stroke_active = false;
  std::uint64_t m_stroke_seed = 0;
  std::size_t m_stamp_index = 0;
  std::uint64_t m_next_stroke_seed = 0;
  float m_distance_accumulator = 0.0f;
};

}  // namespace kage::editor
