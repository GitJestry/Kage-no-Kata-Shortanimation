#pragma once

#include "film/movie_timeline.hpp"
#include "scene/scene_manager.hpp"

#include <expected>
#include <string>

namespace kage::engine::detail {

enum class FilmCameraCreationFailurePoint {
  None,
  BeforeSequenceCreation,
  WhileAddingClips,
  WhilePlacingInstance,
};

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
    film::FilmFrame parStartFrame,
    FilmCameraCreationFailurePoint parFailurePoint =
        FilmCameraCreationFailurePoint::None);

}  // namespace kage::engine::detail
