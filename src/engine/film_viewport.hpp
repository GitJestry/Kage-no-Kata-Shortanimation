#pragma once

#include "camera/camera.hpp"
#include "film/film_frame_state.hpp"
#include "scene/world.hpp"

#include <glm/gtc/quaternion.hpp>

#include <optional>

namespace kage::engine {

struct FilmViewportCamera final {
  std::optional<camera::Camera> camera;
  bool consumes_film_state = false;
  bool black_output = false;
};

[[nodiscard]] inline FilmViewportCamera resolveFilmViewportCamera(
    const camera::Camera& parEditorCamera, const scene::World& parWorld,
    const film::FilmFrameState* parFilmState,
    bool parUseFilmCamera = true) {
  if (parFilmState == nullptr) {
    return {parEditorCamera, false, false};
  }
  if (!parUseFilmCamera) {
    return {parEditorCamera, true, false};
  }
  if (!parFilmState->camera.has_value()) {
    return {std::nullopt, true, true};
  }

  const film::EvaluatedCameraState& sample = *parFilmState->camera;
  const scene::EntityRecord* entity = parWorld.findEntity(sample.source_entity);
  if (entity == nullptr || !entity->camera.has_value()) {
    return {std::nullopt, true, true};
  }

  camera::Camera result;
  result.position = sample.transform.translation;
  result.orientation = glm::normalize(sample.transform.rotation);
  result.vertical_fov_degrees = sample.vertical_fov_degrees;
  result.near_plane = sample.near_plane;
  result.far_plane = sample.far_plane;
  return {result, true, false};
}

[[nodiscard]] inline bool shouldShowEditorOverlays(
    bool parRequested, const FilmViewportCamera& parViewportCamera,
    bool parUseFilmCamera) {
  return parRequested &&
         !(parUseFilmCamera && parViewportCamera.consumes_film_state);
}

}  // namespace kage::engine
