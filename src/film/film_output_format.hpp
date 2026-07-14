#pragma once

namespace kage::film {

inline constexpr int FILM_OUTPUT_WIDTH = 3840;
inline constexpr int FILM_OUTPUT_HEIGHT = 2160;
inline constexpr float FILM_OUTPUT_ASPECT_RATIO =
    static_cast<float>(FILM_OUTPUT_WIDTH) /
    static_cast<float>(FILM_OUTPUT_HEIGHT);

}  // namespace kage::film
