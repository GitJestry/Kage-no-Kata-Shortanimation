#include "editor/selection_controller.hpp"

#include <optional>

namespace {

constexpr float DOUBLE_CLICK_SECONDS = 0.32f;
constexpr float DOUBLE_CLICK_PIXEL_RADIUS = 32.0f;

}  // namespace

namespace kage::editor {

void SelectionController::update(float parDeltaSeconds) {
  m_elapsed_seconds += parDeltaSeconds;
}

bool SelectionController::handleViewportLeftPress(
    engine::EngineCore& parEngine, const glm::vec2& parCursorPixel,
    const glm::vec2& parViewportSize) {
  const float elapsed_since_last =
      m_elapsed_seconds - m_last_left_click_seconds;
  const glm::vec2 click_delta = parCursorPixel - m_last_left_click_position;
  const bool near_last_click =
      glm::dot(click_delta, click_delta) <=
      DOUBLE_CLICK_PIXEL_RADIUS * DOUBLE_CLICK_PIXEL_RADIUS;
  m_last_left_click_seconds = m_elapsed_seconds;
  m_last_left_click_position = parCursorPixel;

  if (elapsed_since_last > DOUBLE_CLICK_SECONDS || !near_last_click) {
    return false;
  }

  const std::optional<scene::EntityId> picked =
      parEngine.pickEntity(parCursorPixel, parViewportSize);
  if (!picked.has_value()) {
    return false;
  }

  parEngine.selectEntity(*picked);
  return true;
}

void SelectionController::selectFromOutliner(engine::EngineCore& parEngine,
                                             scene::EntityId parEntity,
                                             bool parFrameEntity) {
  parEngine.selectEntity(parEntity);
  if (parFrameEntity) {
    parEngine.frameEntity(parEntity);
  }
}

}  // namespace kage::editor
