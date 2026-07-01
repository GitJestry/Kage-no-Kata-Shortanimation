#pragma once

#include "camera/camera_system.hpp"
#include "scene/scene_manager.hpp"

namespace kage::engine {

class EditorCameraBridge final {
 public:
  static void syncEntityFromCamera(scene::SceneManager::SceneRecord& parScene,
                                   const camera::CameraSystem& parCameraSystem);
  static void syncCameraFromEntity(const scene::SceneManager::SceneRecord& parScene,
                                   camera::CameraSystem& parCameraSystem);
};

}  // namespace kage::engine
