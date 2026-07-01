#include "scene/scene_manager.hpp"

#include <stdexcept>
#include <utility>

namespace kage::scene {

std::size_t SceneManager::createScene(std::string parName) {
  if (parName.empty()) {
    parName = "Scene " + std::to_string(m_scenes.size() + 1);
  }

  SceneRecord scene;
  scene.name = std::move(parName);
  m_scenes.push_back(std::move(scene));
  m_active_scene_index = m_scenes.size() - 1;
  return m_active_scene_index;
}

bool SceneManager::deleteScene(std::size_t parSceneIndex) {
  if (m_scenes.size() <= 1 || parSceneIndex >= m_scenes.size()) {
    return false;
  }

  m_scenes.erase(m_scenes.begin() + static_cast<std::ptrdiff_t>(parSceneIndex));
  if (m_active_scene_index >= m_scenes.size()) {
    m_active_scene_index = m_scenes.size() - 1;
  } else if (m_active_scene_index > parSceneIndex) {
    --m_active_scene_index;
  }
  return true;
}

void SceneManager::clearScenes() {
  m_scenes.clear();
  m_active_scene_index = 0;
}

void SceneManager::setActiveScene(std::size_t parSceneIndex) {
  if (parSceneIndex < m_scenes.size()) {
    m_active_scene_index = parSceneIndex;
  }
}

void SceneManager::renameScene(std::size_t parSceneIndex, std::string parName) {
  if (parSceneIndex >= m_scenes.size() || parName.empty()) {
    return;
  }

  m_scenes[parSceneIndex].name = std::move(parName);
}

void SceneManager::selectEntity(EntityId parEntity) {
  SceneRecord& scene = getActiveScene();
  if (!parEntity.isValid()) {
    scene.selected_entity = {};
    return;
  }
  if (scene.world.findEntity(parEntity) != nullptr) {
    scene.selected_entity = parEntity;
  }
}

SceneManager::SceneRecord& SceneManager::getActiveScene() {
  if (m_scenes.empty()) {
    throw std::runtime_error("Engine has no active scene");
  }

  return m_scenes[m_active_scene_index];
}

const SceneManager::SceneRecord& SceneManager::getActiveScene() const {
  if (m_scenes.empty()) {
    throw std::runtime_error("Engine has no active scene");
  }

  return m_scenes[m_active_scene_index];
}

SceneManager::SceneRecord* SceneManager::getScene(std::size_t parSceneIndex) {
  if (parSceneIndex >= m_scenes.size()) {
    return nullptr;
  }

  return &m_scenes[parSceneIndex];
}

const SceneManager::SceneRecord* SceneManager::getScene(
    std::size_t parSceneIndex) const {
  if (parSceneIndex >= m_scenes.size()) {
    return nullptr;
  }

  return &m_scenes[parSceneIndex];
}

std::span<const SceneManager::SceneRecord> SceneManager::getScenes() const {
  return m_scenes;
}

std::size_t SceneManager::getActiveSceneIndex() const {
  return m_active_scene_index;
}

EntityId SceneManager::getSelectedEntity() const {
  return getActiveScene().selected_entity;
}

EntityId SceneManager::getEditorCameraEntity() const {
  return getActiveScene().editor_camera_entity;
}

EntityId SceneManager::getPrimaryLightEntity() const {
  return getActiveScene().primary_light_entity;
}

}  // namespace kage::scene
