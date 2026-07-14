#pragma once

#include "film/movie_timeline.hpp"
#include "scene/scene_manager.hpp"

#include <expected>
#include <string>

namespace kage::engine {

struct FilmCameraCreationResult final {
  scene::EntityId entity;
  film::TargetSequenceId sequence_id = 0;
  film::SequenceInstanceId instance_id = 0;
};

[[nodiscard]] std::expected<FilmCameraCreationResult, std::string>
createFilmCameraAtomically(
    scene::SceneManager::SceneRecord& parScene,
    const math::Transform& parTransform,
    const film::CapturedCameraState& parCamera,
    film::FilmFrame parStartFrame);

}  // namespace kage::engine
