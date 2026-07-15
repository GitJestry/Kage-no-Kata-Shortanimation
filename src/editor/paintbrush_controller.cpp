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
bool PaintbrushController::processInput(
    const glm::vec3& parWorldPosition,
    const kage::input::EditorInputSnapshot& parInput,
    const PaintbrushSettings& parSettings,
    const std::vector<std::size_t>& parSelectedAssetIndices) {
  
  const bool wants_paint = parInput.left_mouse_pressed || parInput.left_mouse_down ||
                           parInput.left_mouse_released;
  if (!wants_paint) {
    return false;
  }

  // 1. STROKE START
  if (parInput.left_mouse_pressed) {
    m_stroke_active = true;
    m_last_world_position = parWorldPosition;
    m_last_stamp_position = parWorldPosition;
    m_stamp_index = 0;
    m_stroke_seed = m_next_stroke_seed++;
    m_distance_accumulator = 0.0f; // Reset accumulator on new click

    if (!parSelectedAssetIndices.empty()) {
      paintStampAt(parWorldPosition, parSettings, parSelectedAssetIndices);
    }
  }

  // 2. STROKE ACTIVE & DRAGGING
  if (m_stroke_active && parInput.left_mouse_down) {
    if (!parSelectedAssetIndices.empty()) {
      // Calculate how far the cursor traveled in world space this frame
      const float distance_moved = glm::distance(m_last_world_position, parWorldPosition);
      m_distance_accumulator += distance_moved;

      // Define spacing (e.g., a spacing parameter, or default to 50% of brush size)
      // Note: adjust 'brush_spacing' according to what exists in your PaintbrushSettings
      const float stamp_spacing = (parSettings.brush_spacing > 0.0f) 
                                  ? parSettings.brush_spacing 
                                  : static_cast<float>(parSettings.brush_size) * 0.5f;

      // If we've dragged far enough, deposit stamps along the vector
      while (m_distance_accumulator >= stamp_spacing) {
        // Interpolate along the movement segment to find the exact stamp coordinate
        float t = 1.0f - (m_distance_accumulator / distance_moved);
        t = glm::clamp(t, 0.0f, 1.0f);
        
        glm::vec3 stamp_pos = glm::mix(m_last_world_position, parWorldPosition, t);
        
        paintStampAt(stamp_pos, parSettings, parSelectedAssetIndices);
        
        // Consume the distance chunk
        m_distance_accumulator -= stamp_spacing;
      }
    }
    m_last_world_position = parWorldPosition;
  }

  // 3. STROKE END
  if (parInput.left_mouse_released) {
    m_stroke_active = false;
  }

  return true;
}

void PaintbrushController::resetStroke() {
  m_stroke_active = false;
  m_stamp_index = 0;
  m_last_world_position = glm::vec3(0.0f);
  m_last_stamp_position = glm::vec3(0.0f);
}

bool PaintbrushController::paintStampAt(
    const glm::vec3& parPosition,
    const PaintbrushSettings& parSettings,
    const std::vector<std::size_t>& parSelectedAssetIndices) {
  if (parSelectedAssetIndices.empty() || !m_paint_callback) {
    return false;
  }

  const std::uint64_t seed = m_stroke_seed + static_cast<std::uint64_t>(m_stamp_index);
  const auto scatter_results = PaintbrushScatterGenerator::generate(
      parSettings, parPosition, parSelectedAssetIndices, seed);
  ++m_stamp_index;

  for (const auto& result : scatter_results) {
    m_paint_callback(result.asset_index, result.transform);
  }
  return true;
}

void PaintbrushController::paintPathSegment(
    const glm::vec3& parFrom,
    const glm::vec3& parTo,
    const PaintbrushSettings& parSettings,
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

}  // namespace kage::editor
