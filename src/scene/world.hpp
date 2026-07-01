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
  std::optional<CameraComponent> camera;
  std::optional<DirectionalLightComponent> directional_light;
  std::optional<PointLightComponent> point_light;
  bool alive = true;
};

class World final {
 public:
  EntityId createEntity(std::string parName);
  EntityId createEntityWithId(std::string parName, EntityId parEntity);
  void setStaticMesh(EntityId parEntity, StaticMeshComponent parStaticMesh);
  void setCamera(EntityId parEntity, CameraComponent parCamera);
  void setDirectionalLight(EntityId parEntity,
                           DirectionalLightComponent parLight);
  void setPointLight(EntityId parEntity, PointLightComponent parLight);
  void setVisible(EntityId parEntity, bool parVisible);
  bool deleteEntity(EntityId parEntity);

  [[nodiscard]] EntityRecord* findEntity(EntityId parEntity);
  [[nodiscard]] const EntityRecord* findEntity(EntityId parEntity) const;
  [[nodiscard]] std::span<const EntityRecord> getEntities() const;

 private:
  std::vector<EntityRecord> m_entities;
  std::uint32_t m_next_entity_id = 0;
};

}  // namespace kage::scene
