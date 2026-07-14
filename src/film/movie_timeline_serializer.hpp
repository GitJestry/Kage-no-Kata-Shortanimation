#pragma once

#include "film/movie_timeline.hpp"

#include <string>
#include <string_view>

namespace kage::film {

[[nodiscard]] bool decodeMovieTimeline(std::string_view parJson,
                                       MovieTimeline& parTimeline,
                                       std::string& parError);

[[nodiscard]] std::string encodeMovieTimeline(
    const MovieTimeline& parTimeline);

}  // namespace kage::film
