#pragma once

#include "editor/ui_panel_rect.hpp"
#include "film/film_output_format.hpp"

#include <glm/glm.hpp>

#include <algorithm>

namespace kage::editor {

inline constexpr float MOVIE_TARGET_LIST_WIDTH = 200.0f;
inline constexpr float MOVIE_INSPECTOR_WIDTH = 280.0f;
inline constexpr float MOVIE_MIN_VIEWPORT_WIDTH = 640.0f;
inline constexpr float MOVIE_MIN_TIMELINE_HEIGHT = 220.0f;
inline constexpr float MOVIE_MIN_UPPER_WORKSPACE_HEIGHT = 120.0f;

[[nodiscard]] inline UiPanelRect fitFilmPreviewRect(
    const UiPanelRect& parContainer) {
  const glm::vec2 container_size = glm::max(
      parContainer.max - parContainer.min, glm::vec2(0.0f));
  if (container_size.x <= 0.0f || container_size.y <= 0.0f) {
    return parContainer;
  }

  glm::vec2 frame_size = container_size;
  if (container_size.x / container_size.y >
      film::FILM_OUTPUT_ASPECT_RATIO) {
    frame_size.x = container_size.y * film::FILM_OUTPUT_ASPECT_RATIO;
  } else {
    frame_size.y = container_size.x / film::FILM_OUTPUT_ASPECT_RATIO;
  }
  const glm::vec2 frame_min =
      parContainer.min + (container_size - frame_size) * 0.5f;
  return {frame_min, frame_min + frame_size};
}

struct MovieEditorLayout final {
  UiPanelRect left_panel;
  UiPanelRect right_panel;
  UiPanelRect viewport;
  UiPanelRect film_preview;
  UiPanelRect timeline;
  float timeline_min_height = 1.0f;
  float timeline_max_height = 1.0f;
};

[[nodiscard]] inline MovieEditorLayout computeMovieEditorLayout(
    const glm::vec2& parWorkPosition, const glm::vec2& parWorkSize,
    float parRequestedTimelineHeight) {
  const float width = std::max(parWorkSize.x, 1.0f);
  const float workspace_height = std::max(parWorkSize.y, 1.0f);
  const float requested_side_width =
      MOVIE_TARGET_LIST_WIDTH + MOVIE_INSPECTOR_WIDTH;
  const float side_scale = std::min(
      1.0f, std::max(0.0f, width - MOVIE_MIN_VIEWPORT_WIDTH) /
                requested_side_width);
  const float left_width = MOVIE_TARGET_LIST_WIDTH * side_scale;
  const float right_width = MOVIE_INSPECTOR_WIDTH * side_scale;
  const float timeline_min =
      workspace_height >= MOVIE_MIN_TIMELINE_HEIGHT +
                              MOVIE_MIN_UPPER_WORKSPACE_HEIGHT
          ? MOVIE_MIN_TIMELINE_HEIGHT
          : workspace_height;
  const float timeline_max =
      workspace_height >= MOVIE_MIN_TIMELINE_HEIGHT +
                              MOVIE_MIN_UPPER_WORKSPACE_HEIGHT
          ? workspace_height - MOVIE_MIN_UPPER_WORKSPACE_HEIGHT
          : workspace_height;
  const float timeline_height = std::clamp(parRequestedTimelineHeight,
                                           timeline_min, timeline_max);
  const float upper_height = std::max(workspace_height - timeline_height, 0.0f);
  const float left = parWorkPosition.x;
  const float top = parWorkPosition.y;
  const float right = left + width;
  const float timeline_top = top + upper_height;
  const UiPanelRect viewport{glm::vec2(left + left_width, top),
                             glm::vec2(right - right_width, timeline_top)};
  return {{glm::vec2(left, top), glm::vec2(left + left_width, timeline_top)},
          {glm::vec2(right - right_width, top), glm::vec2(right, timeline_top)},
          viewport,
          fitFilmPreviewRect(viewport),
          {glm::vec2(left, timeline_top), glm::vec2(right, top + workspace_height)},
          timeline_min,
          timeline_max};
}

}  // namespace kage::editor
