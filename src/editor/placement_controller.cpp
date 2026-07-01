#include "editor/placement_controller.hpp"

#include "assets/asset_registry.hpp"
#include "scene/components.hpp"

#include <glm/gtc/quaternion.hpp>

namespace {

constexpr float CAMERA_PLACEMENT_HEIGHT = 1.6f;
constexpr float LIGHT_PLACEMENT_HEIGHT = 2.8f;
constexpr float DEFAULT_GHOST_OPACITY = 0.38f;

}  // namespace

namespace kage::editor {

void PlacementController::beginStaticAsset(engine::EngineCore& parEngine,
                                           std::size_t parAssetIndex) {
  begin(parEngine, Kind::StaticAsset, parAssetIndex);
}

void PlacementController::beginCamera(engine::EngineCore& parEngine) {
  begin(parEngine, Kind::Camera, 0);
}

void PlacementController::beginPointLight(engine::EngineCore& parEngine) {
  begin(parEngine, Kind::PointLight, 0);
}

void PlacementController::update(engine::EngineCore& parEngine,
                                 const glm::vec2& parCursorPixel,
                                 const glm::vec2& parViewportSize,
                                 bool parLeftMouseDown) {
  if (!isActive()) {
    return;
  }

  glm::vec3 position =
      parEngine.getPlacementPointOnFloor(parCursorPixel, parViewportSize);
  switch (m_kind) {
    case Kind::StaticAsset:
      position.y = 0.0f;
      break;
    case Kind::Camera:
      position.y += CAMERA_PLACEMENT_HEIGHT;
      m_transform.rotation = parEngine.getCameraSystem().getCamera().orientation;
      break;
    case Kind::PointLight:
      position.y += LIGHT_PLACEMENT_HEIGHT;
      break;
    case Kind::None:
      break;
  }
  m_transform.translation = position;
  if (!parLeftMouseDown) {
    m_commit_ready = true;
  }
  publishGhost(parEngine);
}

bool PlacementController::commit(engine::EngineCore& parEngine) {
  if (!isActive() || !m_commit_ready) {
    return false;
  }

  switch (m_kind) {
    case Kind::StaticAsset:
      parEngine.instantiateAssetAt(m_asset_index, m_transform.translation);
      break;
    case Kind::Camera:
      parEngine.createCameraEntityAt("Camera", m_transform.translation);
      break;
    case Kind::PointLight:
      parEngine.createPointLightEntityAt("Point Light", m_transform.translation);
      break;
    case Kind::None:
      return false;
  }

  m_kind = Kind::None;
  m_commit_ready = false;
  parEngine.clearPlacementGhost();
  return true;
}

bool PlacementController::cancel(engine::EngineCore& parEngine) {
  if (!isActive()) {
    return false;
  }

  m_kind = Kind::None;
  m_commit_ready = false;
  parEngine.clearPlacementGhost();
  return true;
}

bool PlacementController::isActive() const {
  return m_kind != Kind::None;
}

bool PlacementController::canCommit() const {
  return isActive() && m_commit_ready;
}

PlacementController::Kind PlacementController::getKind() const {
  return m_kind;
}

const char* PlacementController::getStatusLabel() const {
  if (isActive() && !m_commit_ready) {
    return "Move into viewport";
  }

  switch (m_kind) {
    case Kind::StaticAsset:
      return "Placing asset";
    case Kind::Camera:
      return "Placing camera";
    case Kind::PointLight:
      return "Placing point light";
    case Kind::None:
      return "Ready";
  }

  return "Ready";
}

void PlacementController::begin(engine::EngineCore& parEngine, Kind parKind,
                                std::size_t parAssetIndex) {
  m_kind = parKind;
  m_asset_index = parAssetIndex;
  m_transform = {};
  m_commit_ready = false;

  glm::vec3 position = parEngine.getPointInFrontOfCamera(4.0f);
  position.y = 0.0f;
  if (m_kind == Kind::Camera) {
    position.y = CAMERA_PLACEMENT_HEIGHT;
    m_transform.rotation = parEngine.getCameraSystem().getCamera().orientation;
  } else if (m_kind == Kind::PointLight) {
    position.y = LIGHT_PLACEMENT_HEIGHT;
  }
  m_transform.translation = position;
  publishGhost(parEngine);
}

void PlacementController::publishGhost(engine::EngineCore& parEngine) const {
  render::PlacementGhost ghost;
  ghost.transform = m_transform;
  ghost.opacity = DEFAULT_GHOST_OPACITY;

  switch (m_kind) {
    case Kind::StaticAsset: {
      const assets::AssetRegistry::AssetLibraryEntry* asset =
          parEngine.getAssetLibraryEntry(m_asset_index);
      if (asset == nullptr) {
        return;
      }
      ghost.kind = render::PlacementGhost::Kind::StaticAsset;
      ghost.mesh_handle = asset->mesh_handle;
      break;
    }
    case Kind::Camera:
      ghost.kind = render::PlacementGhost::Kind::Camera;
      break;
    case Kind::PointLight:
      ghost.kind = render::PlacementGhost::Kind::PointLight;
      break;
    case Kind::None:
      break;
  }

  parEngine.setPlacementGhost(ghost);
}

}  // namespace kage::editor
