#include "editor/movie_editor_layout.hpp"

#include <cmath>
#include <iostream>

namespace {

[[nodiscard]] bool close(float parLeft, float parRight) {
  return std::abs(parLeft - parRight) < 0.001f;
}

[[nodiscard]] bool hasFilmAspect(const kage::editor::UiPanelRect& parRect) {
  const float width = parRect.max.x - parRect.min.x;
  const float height = parRect.max.y - parRect.min.y;
  return height > 0.0f &&
         close(width / height, kage::film::FILM_OUTPUT_ASPECT_RATIO);
}

int fail(const char* parMessage) {
  std::cerr << parMessage << '\n';
  return 1;
}

}  // namespace

int main() {
  using namespace kage::editor;

  const MovieEditorLayout layout = computeMovieEditorLayout(
      {10.0f, 20.0f}, {1600.0f, 1000.0f}, 360.0f);
  if (!close(layout.timeline.min.x, 10.0f) ||
      !close(layout.timeline.max.x, 1610.0f) ||
      !close(layout.left_panel.max.y, layout.timeline.min.y) ||
      !close(layout.right_panel.max.y, layout.timeline.min.y) ||
      !close(layout.viewport.max.y, layout.timeline.min.y) ||
      !close(layout.viewport.min.x, layout.left_panel.max.x) ||
      !close(layout.viewport.max.x, layout.right_panel.min.x) ||
      !hasFilmAspect(layout.film_preview)) {
    return fail("Movie Editor layout rectangles do not share their boundaries");
  }
  if (!close(layout.left_panel.max.x - layout.left_panel.min.x,
             MOVIE_TARGET_LIST_WIDTH) ||
      !close(layout.right_panel.max.x - layout.right_panel.min.x,
             MOVIE_INSPECTOR_WIDTH) ||
      layout.viewport.max.x - layout.viewport.min.x <
          MOVIE_MIN_VIEWPORT_WIDTH ||
      layout.viewport.max.x - layout.viewport.min.x <= 800.0f) {
    return fail("Movie side panels did not leave the viewport the majority of the workspace");
  }
  if (!close(layout.timeline_min_height, MOVIE_MIN_TIMELINE_HEIGHT) ||
      !close(layout.timeline_max_height,
             1000.0f - MOVIE_MIN_UPPER_WORKSPACE_HEIGHT) ||
      !close(layout.timeline.max.y, 1020.0f)) {
    return fail("Movie timeline resize limits are not sensible");
  }

  const MovieEditorLayout taller = computeMovieEditorLayout(
      {10.0f, 20.0f}, {1600.0f, 1000.0f}, 620.0f);
  if (!(taller.timeline.min.y < layout.timeline.min.y) ||
      !(taller.viewport.max.y < layout.viewport.max.y) ||
      !close(taller.left_panel.max.y, taller.timeline.min.y) ||
      !close(taller.right_panel.max.y, taller.timeline.min.y)) {
    return fail("increasing timeline height did not reduce the upper workspace");
  }

  const MovieEditorLayout clamped = computeMovieEditorLayout(
      {0.0f, 0.0f}, {1200.0f, 800.0f}, 9999.0f);
  const float clamped_height = clamped.timeline.max.y - clamped.timeline.min.y;
  if (!close(clamped_height, clamped.timeline_max_height) ||
      clamped.viewport.max.y - clamped.viewport.min.y <
          MOVIE_MIN_UPPER_WORKSPACE_HEIGHT - 0.001f) {
    return fail("timeline resize did not clamp before consuming the viewport");
  }

  const MovieEditorLayout narrow = computeMovieEditorLayout(
      {30.0f, 40.0f}, {900.0f, 700.0f}, 260.0f);
  const float narrow_viewport_width =
      narrow.viewport.max.x - narrow.viewport.min.x;
  if (!close(narrow_viewport_width, MOVIE_MIN_VIEWPORT_WIDTH) ||
      narrow.left_panel.max.x > narrow.viewport.min.x ||
      narrow.right_panel.min.x < narrow.viewport.max.x ||
      narrow.left_panel.max.x - narrow.left_panel.min.x >=
          MOVIE_TARGET_LIST_WIDTH ||
      narrow.right_panel.max.x - narrow.right_panel.min.x >=
          MOVIE_INSPECTOR_WIDTH) {
    return fail("Movie layout did not clamp side panels before reducing the viewport");
  }

  const UiPanelRect wide_frame = fitFilmPreviewRect(
      {{10.0f, 20.0f}, {810.0f, 320.0f}});
  if (!hasFilmAspect(wide_frame) || !close(wide_frame.min.y, 20.0f) ||
      !close(wide_frame.max.y, 320.0f) || !close(wide_frame.min.x, 143.3333f) ||
      !close(wide_frame.max.x, 676.6667f)) {
    return fail("wide Movie preview was not centered in a 16:9 frame");
  }

  const UiPanelRect tall_frame = fitFilmPreviewRect(
      {{10.0f, 20.0f}, {410.0f, 820.0f}});
  if (!hasFilmAspect(tall_frame) || !close(tall_frame.min.x, 10.0f) ||
      !close(tall_frame.max.x, 410.0f) || !close(tall_frame.min.y, 307.5f) ||
      !close(tall_frame.max.y, 532.5f)) {
    return fail("tall Movie preview was not centered in a 16:9 frame");
  }

  const UiPanelRect native_frame = fitFilmPreviewRect(
      {{10.0f, 20.0f}, {650.0f, 380.0f}});
  if (!hasFilmAspect(native_frame) || !close(native_frame.min.x, 10.0f) ||
      !close(native_frame.min.y, 20.0f) || !close(native_frame.max.x, 650.0f) ||
      !close(native_frame.max.y, 380.0f)) {
    return fail("native 16:9 Movie preview was resized");
  }
  return 0;
}
