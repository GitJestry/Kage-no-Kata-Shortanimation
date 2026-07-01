#pragma once

#include "engine/engine_core.hpp"

#include <glm/glm.hpp>

#include <cstddef>

namespace kage::editor {

class PlacementController final {
 public:
  enum class Kind {
    None,
    StaticAsset,
    Camera,
    PointLight
  };

  void beginStaticAsset(engine::EngineCore& parEngine,
                        std::size_t parAssetIndex);
  void beginCamera(engine::EngineCore& parEngine);
  void beginPointLight(engine::EngineCore& parEngine);
  void update(engine::EngineCore& parEngine, const glm::vec2& parCursorPixel,
              const glm::vec2& parViewportSize, bool parLeftMouseDown);
  bool commit(engine::EngineCore& parEngine);
  bool cancel(engine::EngineCore& parEngine);

  [[nodiscard]] bool isActive() const;
  [[nodiscard]] bool canCommit() const;
  [[nodiscard]] Kind getKind() const;
  [[nodiscard]] const char* getStatusLabel() const;

 private:
  void begin(engine::EngineCore& parEngine, Kind parKind,
             std::size_t parAssetIndex);
  void publishGhost(engine::EngineCore& parEngine) const;

  Kind m_kind = Kind::None;
  std::size_t m_asset_index = 0;
  math::Transform m_transform;
  bool m_commit_ready = false;
};

}  // namespace kage::editor
