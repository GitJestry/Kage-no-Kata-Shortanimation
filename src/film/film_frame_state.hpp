#pragma once

#include "assets/asset_types.hpp"
#include "math/transform.hpp"
#include "scene/entity_id.hpp"

#include <optional>
#include <vector>

namespace kage::film {

enum class FilmPropertyKind {
  CameraFov,
  LightEnabled,
  LightIntensity,
  LightColor,
  LightRange,
  LightCastsShadows,
};

struct RigAnimationPlayback final {
  assets::AnimationClipId clip_id = 0;
  float source_in = 0.0f;
  float source_out = 1.0f;
  float speed = 1.0f;
  bool looping = false;
};

struct TransformOverride final {
  scene::EntityId entity;
  math::Transform transform;
};

struct PropertyOverride final {
  scene::EntityId entity;
  FilmPropertyKind kind = FilmPropertyKind::CameraFov;
  glm::vec4 value{0.0f};
};

struct RigAnimationOverride final {
  scene::EntityId entity;
  RigAnimationPlayback animation;
  float local_time_seconds = 0.0f;
  float weight = 1.0f;
  bool final_pose = false;
};

enum class FilmOutputKind { Camera, Black };

struct EvaluatedCameraState final {
  scene::EntityId source_entity;
  math::Transform transform;
  float vertical_fov_degrees = 45.0f;
  float near_plane = 0.1f;
  float far_plane = 1000.0f;
};

struct FilmCameraOutput final {
  FilmOutputKind kind = FilmOutputKind::Black;
  std::optional<EvaluatedCameraState> camera;
};

struct EvaluatedSunState final {
  glm::vec3 direction_to_sun{0.0f, -1.0f, 0.0f};
  glm::vec3 color{1.0f};
  float intensity = 1.0f;
};

struct FilmFrameState final {
  std::vector<TransformOverride> transforms;
  std::vector<PropertyOverride> properties;
  std::vector<RigAnimationOverride> rig_animations;
  FilmCameraOutput camera_output;
  std::optional<EvaluatedSunState> sun;
};

}  // namespace kage::film
