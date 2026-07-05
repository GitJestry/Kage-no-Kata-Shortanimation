#pragma once

#include "scene/components.hpp"
#include "scene/entity_id.hpp"

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace kage::scene {

struct EntityRecord final {
  EntityId id;
  NameComponent name;
  TransformComponent transform;
  std::optional<StaticMeshComponent> static_mesh;
  std::optional<RigComponent> rig;
  std::optional<AnimationPlayerComponent> animation_player;
  std::optional<CameraComponent> camera;
  std::optional<LightComponent> light;
  bool alive = true;
};

class World final {
 public:
  EntityId createEntity(std::string parName);
  EntityId createEntityWithId(std::string parName, EntityId parEntity);
  void setStaticMesh(EntityId parEntity, StaticMeshComponent parStaticMesh);
  void setRig(EntityId parEntity, RigComponent parRig);
  void setAnimationPlayer(EntityId parEntity,
                          AnimationPlayerComponent parAnimationPlayer);
  void clearAnimationPlayer(EntityId parEntity);
  void setCamera(EntityId parEntity, CameraComponent parCamera);
  void setLight(EntityId parEntity, LightComponent parLight);
  void setVisible(EntityId parEntity, bool parVisible);
  bool deleteEntity(EntityId parEntity);

  [[nodiscard]] EntityRecord* findEntity(EntityId parEntity);
  [[nodiscard]] const EntityRecord* findEntity(EntityId parEntity) const;
  [[nodiscard]] std::span<EntityRecord> getEntities();
  [[nodiscard]] std::span<const EntityRecord> getEntities() const;

 private:
  std::vector<EntityRecord> m_entities;
  std::uint32_t m_next_entity_id = 0;
};

}  // namespace kage::scene
