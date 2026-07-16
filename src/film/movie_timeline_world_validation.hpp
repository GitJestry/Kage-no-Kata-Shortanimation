#pragma once

#include "film/movie_timeline.hpp"
#include "scene/world.hpp"

namespace kage::assets {
class AssetRegistry;
}

namespace kage::film {

// Adds World-lifecycle diagnostics without making evaluation depend on World.
[[nodiscard]] TimelineValidation validateMovieTimelineWithWorld(
    const MovieTimeline& parTimeline, const scene::World& parWorld,
    bool parForBake = false,
    const assets::AssetRegistry* parAssets = nullptr);

}  // namespace kage::film
