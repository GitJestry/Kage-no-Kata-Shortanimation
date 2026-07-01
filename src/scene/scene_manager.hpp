#pragma once

#include "scene/entity_id.hpp"
#include "scene/world.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace kage::scene {

class SceneManager final {
 public:
  struct SceneRecord final {
    std::string name;
    World world;
    EntityId selected_entity;
    EntityId editor_camera_entity;
    EntityId primary_light_entity;
  };

  std::size_t createScene(std::string parName);
  bool deleteScene(std::size_t parSceneIndex);
  void clearScenes();
  void setActiveScene(std::size_t parSceneIndex);
  void renameScene(std::size_t parSceneIndex, std::string parName);
  void selectEntity(EntityId parEntity);

  [[nodiscard]] SceneRecord& getActiveScene();
  [[nodiscard]] const SceneRecord& getActiveScene() const;
  [[nodiscard]] SceneRecord* getScene(std::size_t parSceneIndex);
  [[nodiscard]] const SceneRecord* getScene(std::size_t parSceneIndex) const;
  [[nodiscard]] std::span<const SceneRecord> getScenes() const;
  [[nodiscard]] std::size_t getActiveSceneIndex() const;
  [[nodiscard]] EntityId getSelectedEntity() const;
  [[nodiscard]] EntityId getEditorCameraEntity() const;
  [[nodiscard]] EntityId getPrimaryLightEntity() const;

 private:
  std::vector<SceneRecord> m_scenes;
  std::size_t m_active_scene_index = 0;
};

}  // namespace kage::scene
