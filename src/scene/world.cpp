#include "scene/world.hpp"

#include <algorithm>
#include <utility>

namespace kage::scene {

EntityId World::createEntity(std::string parName) {
  EntityRecord entity;
  entity.id.value = m_next_entity_id++;
  entity.name.name = std::move(parName);
  m_entities.push_back(std::move(entity));
  return m_entities.back().id;
}

EntityId World::createEntityWithId(std::string parName, EntityId parEntity) {
  if (!parEntity.isValid()) {
    return createEntity(std::move(parName));
  }
  if (findEntity(parEntity) != nullptr) {
    return {};
  }

  EntityRecord entity;
  entity.id = parEntity;
  entity.name.name = std::move(parName);
  m_entities.push_back(std::move(entity));
  m_next_entity_id = std::max(m_next_entity_id, parEntity.value + 1);
  return m_entities.back().id;
}

void World::setStaticMesh(EntityId parEntity,
                          StaticMeshComponent parStaticMesh) {
  EntityRecord* entity = findEntity(parEntity);
  if (entity == nullptr) {
    return;
  }

  entity->static_mesh = parStaticMesh;
}

void World::setRig(EntityId parEntity, RigComponent parRig) {
  EntityRecord* entity = findEntity(parEntity);
  if (entity == nullptr) {
    return;
  }

  entity->rig = std::move(parRig);
}

void World::setCamera(EntityId parEntity, CameraComponent parCamera) {
  EntityRecord* entity = findEntity(parEntity);
  if (entity == nullptr) {
    return;
  }

  entity->camera = parCamera;
}

void World::setLight(EntityId parEntity, LightComponent parLight) {
  EntityRecord* entity = findEntity(parEntity);
  if (entity == nullptr) {
    return;
  }

  entity->light = parLight;
}

bool World::deleteEntity(EntityId parEntity) {
  EntityRecord* entity = findEntity(parEntity);
  if (entity == nullptr) {
    return false;
  }

  entity->alive = false;
  return true;
}

EntityRecord* World::findEntity(EntityId parEntity) {
  if (!parEntity.isValid()) {
    return nullptr;
  }

  for (EntityRecord& entity : m_entities) {
    if (entity.id == parEntity) {
      return entity.alive ? &entity : nullptr;
    }
  }
  return nullptr;
}

const EntityRecord* World::findEntity(EntityId parEntity) const {
  if (!parEntity.isValid()) {
    return nullptr;
  }

  for (const EntityRecord& entity : m_entities) {
    if (entity.id == parEntity) {
      return entity.alive ? &entity : nullptr;
    }
  }
  return nullptr;
}

std::span<EntityRecord> World::getEntities() {
  return m_entities;
}

std::span<const EntityRecord> World::getEntities() const {
  return m_entities;
}

}  // namespace kage::scene
