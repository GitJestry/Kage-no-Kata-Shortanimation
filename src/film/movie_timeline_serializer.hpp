#pragma once

#include "film/movie_timeline.hpp"
#include "scene/components.hpp"
#include "scene/world.hpp"

#include <string>
#include <string_view>

namespace kage::film {

// JSON syntax and all legacy DTOs stay private to the loader implementation.
// The World is read only after its entities have been constructed so migration
// can capture authoritative base state without mutating authored data.
[[nodiscard]] bool decodeMovieTimeline(std::string_view parJson,
                                       const scene::World& parWorld,
                                       const scene::SunLightSettings& parSun,
                                       MovieTimeline& parTimeline,
                                       bool& parMigratedLegacy,
                                       std::string& parError);

[[nodiscard]] std::string encodeMovieTimeline(
    const MovieTimeline& parTimeline);

}  // namespace kage::film
