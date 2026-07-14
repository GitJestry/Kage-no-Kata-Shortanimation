#pragma once

#include "editor/editor_session.hpp"
#include "engine/engine_core.hpp"

#include <glm/glm.hpp>

namespace kage::editor {

class SelectionController final {
 public:
  void update(float parDeltaSeconds);
  bool handleViewportLeftPress(engine::EngineCore& parEngine,
                               const glm::vec2& parCursorPixel,
                               const glm::vec2& parViewportSize);
  void selectFromOutliner(engine::EngineCore& parEngine,
                          scene::EntityId parEntity,
                          bool parFrameEntity);
  bool selectMovieTarget(engine::EngineCore& parEngine,
                         EditorSession& parSession,
                         const glm::vec2& parCursorPixel,
                         const glm::vec2& parViewportSize);

 private:
  float m_elapsed_seconds = 0.0f;
  float m_last_left_click_seconds = -10.0f;
  glm::vec2 m_last_left_click_position{0.0f};
};

}  // namespace kage::editor
