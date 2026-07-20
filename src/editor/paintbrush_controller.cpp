#include "editor/paintbrush_controller.hpp"

#include "editor/paintbrush_scatter_generator.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <iostream>
namespace kage::editor {

namespace {
constexpr float kMinimumStampDistance = 1.0f;
}

PaintbrushController::PaintbrushController(PaintCallback parPaintCallback)
    : m_paint_callback(std::move(parPaintCallback)) {}

void PaintbrushController::setPaintCallback(PaintCallback parPaintCallback) {
  m_paint_callback = std::move(parPaintCallback);
}
bool PaintbrushController::processInput(const glm::vec3& parWorldPosition,
                                        const kage::input::EditorInputSnapshot& parInput,
                                        const PaintbrushSettings& parSettings,
                                        const std::vector<std::size_t>& parSelectedAssetIndices) {

  // Early out if no assets are selected to paint with
  if (parSelectedAssetIndices.empty()) {
    m_stroke_active = false;
    return false;
  }

  // Determine active painting state
  const bool is_starting_stroke = parInput.left_mouse_pressed;
  const bool is_continuing_stroke = m_stroke_active && parInput.left_mouse_down;

  // If the user isn't clicking or dragging, release stroke state and let other tools use the input
  if (!is_starting_stroke && !is_continuing_stroke) {
    m_stroke_active = false;
    return false;
  }

  // 1. STROKE START
  if (is_starting_stroke) {
    m_stroke_active = true;
    m_last_world_position = parWorldPosition;
    m_last_stamp_position = parWorldPosition;
    m_stamp_index = 0;
    m_stroke_seed = m_next_stroke_seed++;
    m_distance_accumulator = 0.0f; // Reset accumulator on new click

    paintStampAt(parWorldPosition, parSettings, parSelectedAssetIndices);
  }
  // 2. STROKE ACTIVE & DRAGGING
  else if (is_continuing_stroke) {
    const float distance_moved = glm::distance(m_last_stamp_position, parWorldPosition);
    m_distance_accumulator += distance_moved;

    const float stamp_spacing = (parSettings.brush_spacing > 0.0f)
                                    ? parSettings.brush_spacing
                                    : static_cast<float>(parSettings.brush_size) * 0.5f;

    // Delegate segment interpolation to paintPathSegment or step along the path
    if (stamp_spacing > 0.0f && m_distance_accumulator >= stamp_spacing) {
      // Option A: Use paintPathSegment if available
      paintPathSegment(m_last_stamp_position, parWorldPosition, parSettings,
                       parSelectedAssetIndices);

      // Option B: Step distance directly from m_last_stamp_position if doing inline:
      // while (m_distance_accumulator >= stamp_spacing) {
      //   m_distance_accumulator -= stamp_spacing;
      //   m_last_stamp_position += glm::normalize(parWorldPosition - m_last_stamp_position) *
      //   stamp_spacing; paintStampAt(m_last_stamp_position, parSettings, parSelectedAssetIndices);
      // }
    }

    m_last_world_position = parWorldPosition;
  }

  // Return true ONLY when we actively handled paint input on this frame to block gizmos/selection
  return true;
}

void PaintbrushController::resetStroke() {
  m_stroke_active = false;
  m_stamp_index = 0;
  m_last_world_position = glm::vec3(0.0f);
  m_last_stamp_position = glm::vec3(0.0f);
}

bool PaintbrushController::paintStampAt(const glm::vec3& parPosition,
                                        const PaintbrushSettings& parSettings,
                                        const std::vector<std::size_t>& parSelectedAssetIndices) {
  if (parSelectedAssetIndices.empty() || !m_paint_callback) {
    return false;
  }

  const std::uint64_t seed = m_stroke_seed + static_cast<std::uint64_t>(m_stamp_index);
  const auto scatter_results =
      PaintbrushScatterGenerator::generate(parSettings, parPosition, parSelectedAssetIndices, seed);
  ++m_stamp_index;

  for (const auto& result : scatter_results) {
    m_paint_callback(result.asset_index, result.transform);
  }
  return true;
}

void PaintbrushController::paintPathSegment(
    const glm::vec3& parFrom, const glm::vec3& parTo, const PaintbrushSettings& parSettings,
    const std::vector<std::size_t>& parSelectedAssetIndices) {
  if (parSelectedAssetIndices.empty()) {
    return;
  }

  const float spacing = getStampSpacing(parSettings.brush_size);
  const glm::vec3 delta = parTo - parFrom;
  const float segment_length = glm::length(delta);
  if (segment_length <= 0.0f) {
    return;
  }

  const glm::vec3 direction = delta / segment_length;
  float remaining_distance = segment_length;
  float distance_since_last_stamp = glm::distance(m_last_stamp_position, parFrom);
  float next_distance = spacing - distance_since_last_stamp;
  if (next_distance <= 0.0f) {
    next_distance = spacing;
  }

  glm::vec3 current_position = parFrom;
  while (remaining_distance >= next_distance) {
    current_position += direction * next_distance;
    if (paintStampAt(current_position, parSettings, parSelectedAssetIndices)) {
      m_last_stamp_position = current_position;
    }
    remaining_distance -= next_distance;
    next_distance = spacing;
  }
}

float PaintbrushController::getStampSpacing(int parBrushSize) {
  return std::max(kMinimumStampDistance, static_cast<float>(parBrushSize) * 0.5f);
}

} // namespace kage::editor
