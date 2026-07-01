#include "engine/editor_camera_bridge.hpp"

#include <glm/gtc/quaternion.hpp>

namespace kage::engine {

void EditorCameraBridge::syncEntityFromCamera(
    scene::SceneManager::SceneRecord& parScene,
    const camera::CameraSystem& parCameraSystem) {
  scene::EntityRecord* entity =
      parScene.world.findEntity(parScene.editor_camera_entity);
  if (entity == nullptr || !entity->camera.has_value()) {
    return;
  }

  const camera::Camera& camera = parCameraSystem.getCamera();
  entity->transform.transform.translation = camera.position;
  entity->transform.transform.rotation = camera.orientation;
  entity->camera->vertical_fov_degrees = camera.vertical_fov_degrees;
  entity->camera->near_plane = camera.near_plane;
  entity->camera->far_plane = camera.far_plane;
}

void EditorCameraBridge::syncCameraFromEntity(
    const scene::SceneManager::SceneRecord& parScene,
    camera::CameraSystem& parCameraSystem) {
  const scene::EntityRecord* entity =
      parScene.world.findEntity(parScene.editor_camera_entity);
  if (entity == nullptr || !entity->camera.has_value()) {
    return;
  }

  camera::Camera& camera = parCameraSystem.getCamera();
  camera.position = entity->transform.transform.translation;
  camera.orientation = glm::normalize(entity->transform.transform.rotation);
  camera.vertical_fov_degrees = entity->camera->vertical_fov_degrees;
  camera.near_plane = entity->camera->near_plane;
  camera.far_plane = entity->camera->far_plane;
  parCameraSystem.syncFlyControllerFromCamera();
}

}  // namespace kage::engine
