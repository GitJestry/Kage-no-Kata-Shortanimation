#pragma once

#include "engine/engine_core.hpp"

#include <glm/glm.hpp>

namespace kage::editor {

class GizmoController final {
 public:
  enum class TransformMode {
    Move,
    Scale,
    Rotate
  };

  enum class AxisSpace {
    Local,
    World
  };

  enum class Operation {
    None,
    Move,
    MoveAxis,
    ScaleAxis,
    ScaleUniform,
    Rotate
  };

  void setMode(TransformMode parMode);
  void setAxisSpace(AxisSpace parAxisSpace);
  [[nodiscard]] TransformMode getMode() const;
  [[nodiscard]] AxisSpace getAxisSpace() const;

  bool begin(engine::EngineCore& parEngine, const glm::vec2& parCursorPixel,
             const glm::vec2& parViewportSize);
  void update(engine::EngineCore& parEngine, const glm::vec2& parPixelDelta,
              const glm::vec2& parCursorPixel,
              const glm::vec2& parViewportSize, bool parLeftButton);
  void end();

  [[nodiscard]] bool isActive() const;
  [[nodiscard]] Operation getOperation() const;

 private:
  [[nodiscard]] bool pickAxis(engine::EngineCore& parEngine,
                              scene::EntityId parEntity,
                              const glm::vec2& parCursorPixel,
                              const glm::vec2& parViewportSize,
                              glm::vec3& parAxis) const;

  TransformMode m_mode = TransformMode::Move;
  AxisSpace m_axis_space = AxisSpace::Local;
  Operation m_operation = Operation::None;
  scene::EntityId m_entity;
  glm::vec3 m_drag_offset{0.0f};
  glm::vec3 m_axis{1.0f, 0.0f, 0.0f};
  float m_drag_height = 0.0f;
};

}  // namespace kage::editor
