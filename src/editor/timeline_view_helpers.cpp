#include "editor/timeline_view_helpers.hpp"

#include "editor/editor_session.hpp"
#include "editor/movie_editor_controller.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <variant>

namespace kage::editor {
namespace {

constexpr float TIMELINE_RULER_HEIGHT = 24.0f;
constexpr float TIMELINE_LABEL_WIDTH = 176.0f;
constexpr float MINIMUM_MAX_TIMELINE_PIXELS_PER_FRAME = 96.0f;
constexpr float PLAYHEAD_GRAB_WIDTH = 8.0f;

[[nodiscard]] float timelineMaximumPixelsPerFrame(float parContentWidth,
                                                   float parLabelWidth) {
  return std::max(MINIMUM_MAX_TIMELINE_PIXELS_PER_FRAME,
                  std::max(1.0f, parContentWidth - parLabelWidth));
}

[[nodiscard]] film::FilmFrame rulerTickStep(float parPixelsPerFrame) {
  constexpr std::array<film::FilmFrame, 11> steps{
      1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 1200};
  for (const film::FilmFrame step : steps) {
    if (static_cast<float>(step) * parPixelsPerFrame >= 72.0f) {
      return step;
    }
  }
  return steps.back();
}

}  // namespace

void pushMovieWidgetId(MovieWidgetIdKind parKind, std::uint64_t parId) {
  std::array<char, 40> identity{};
  const auto kind = std::to_chars(
      identity.data(), identity.data() + identity.size(),
      static_cast<std::uint64_t>(parKind), 16);
  char* cursor = kind.ptr;
  *cursor++ = ':';
  const auto id = std::to_chars(cursor, identity.data() + identity.size(),
                                parId, 16);
  ImGui::PushID(identity.data(), id.ptr);
}

std::string movieTargetLabel(const engine::EngineCore& parEngine,
                             const film::TimelineTarget& parTarget) {
  if (parTarget.kind == film::TimelineTargetKind::Sun) {
    return "Sun";
  }
  const scene::EntityRecord* entity =
      parEngine.getWorld().findEntity(parTarget.entity);
  if (entity == nullptr) {
    return "Missing target";
  }
  return entity->name.name;
}

std::optional<film::CapturedTargetBaseState> captureMovieTargetBase(
    const engine::EngineCore& parEngine, const film::TimelineTarget& parTarget) {
  if (parTarget.kind == film::TimelineTargetKind::Sun) {
    const scene::SunLightSettings& sun = parEngine.getSunLightSettings();
    return film::CapturedSunBaseState{sun.direction_to_sun, sun.color,
                                      sun.intensity};
  }

  const scene::EntityRecord* entity =
      parEngine.getWorld().findEntity(parTarget.entity);
  if (entity == nullptr || movieTargetForEntity(*entity) != parTarget) {
    return std::nullopt;
  }
  film::CapturedEntityBaseState base;
  base.transform = entity->transform.transform;
  if (parTarget.kind == film::TimelineTargetKind::Camera) {
    const scene::CameraComponent& camera = *entity->camera;
    base.camera = film::CapturedCameraState{camera.vertical_fov_degrees,
                                             camera.near_plane,
                                             camera.far_plane};
  } else if (parTarget.kind == film::TimelineTargetKind::PointLight) {
    const scene::LightComponent& light = *entity->light;
    base.point_light = film::CapturedPointLightState{
        light.enabled, light.color, light.intensity, light.range,
        light.casts_shadows};
  }
  return base;
}

const assets::ModelAsset* movieAnimationAsset(
    const engine::EngineCore& parEngine, const film::TargetSequence& parSequence) {
  const scene::EntityRecord* entity =
      parEngine.getWorld().findEntity(parSequence.target.entity);
  if (entity == nullptr || !entity->static_mesh.has_value() ||
      entity->static_mesh->asset_library_index ==
          scene::INVALID_ASSET_LIBRARY_INDEX) {
    return nullptr;
  }
  const assets::AssetRegistry::AssetLibraryEntry* asset =
      parEngine.getAssetLibraryEntry(entity->static_mesh->asset_library_index);
  return asset != nullptr && asset->document.has_value() ? &*asset->document
                                                           : nullptr;
}

const char* movieClipLabel(const film::SequenceClipPayload& parPayload) {
  if (std::holds_alternative<film::MovementClip>(parPayload)) {
    return "Movement";
  }
  if (std::holds_alternative<film::RigAnimationClip>(parPayload)) {
    return "Animation";
  }
  switch (std::get<film::PropertyClip>(parPayload).kind) {
    case film::PropertyKind::CameraFov:
      return "FOV";
    case film::PropertyKind::PointLightIntensity:
    case film::PropertyKind::SunIntensity:
      return "Intensity";
    case film::PropertyKind::PointLightColor:
    case film::PropertyKind::SunColor:
      return "Color";
    case film::PropertyKind::SunDirection:
      return "Direction";
  }
  return "Property";
}

float timelineFrameX(const ImVec2& parOrigin, float parLabelWidth,
                     film::FilmFrame parFrame, float parPixelsPerFrame) {
  return parOrigin.x + parLabelWidth +
         static_cast<float>(parFrame) * parPixelsPerFrame;
}

float TimelineCanvas::frameX(film::FilmFrame parFrame) const {
  return timelineFrameX(origin, label_width, parFrame, pixels_per_frame);
}

film::FilmFrame timelineFrameDelta(float parMouseX, float parGestureMouseX,
                                   float parPixelsPerFrame) {
  return static_cast<film::FilmFrame>(std::lround(
      (parMouseX - parGestureMouseX) / parPixelsPerFrame));
}

void drawTimelinePlayhead(ImDrawList& parDrawList, float parX, float parTop,
                          float parBottom) {
  parDrawList.AddLine(ImVec2(parX, parTop), ImVec2(parX, parBottom),
                       IM_COL32(242, 189, 70, 255), 2.0f);
}

float timelineRulerHeight() { return TIMELINE_RULER_HEIGHT; }

void drawTimelineRuler(ImDrawList& parDrawList, const ImVec2& parOrigin,
                       float parLabelWidth, float parPixelsPerFrame,
                       float parContentWidth) {
  const float ruler_left = parOrigin.x + parLabelWidth;
  const float ruler_right = parOrigin.x + parContentWidth;
  parDrawList.AddRectFilled(ImVec2(parOrigin.x, parOrigin.y),
                             ImVec2(ruler_right,
                                    parOrigin.y + TIMELINE_RULER_HEIGHT),
                             IM_COL32(37, 44, 48, 255));
  parDrawList.AddLine(ImVec2(parOrigin.x, parOrigin.y + TIMELINE_RULER_HEIGHT),
                       ImVec2(ruler_right, parOrigin.y + TIMELINE_RULER_HEIGHT),
                       IM_COL32(92, 104, 110, 255));

  const film::FilmFrame step = rulerTickStep(parPixelsPerFrame);
  for (film::FilmFrame frame = 0; frame <= film::MAX_FILM_FRAMES;
       frame += step) {
    const float x = ruler_left + static_cast<float>(frame) * parPixelsPerFrame;
    if (x < ruler_left || x > ruler_right) {
      continue;
    }
    parDrawList.AddLine(ImVec2(x, parOrigin.y + 12.0f),
                         ImVec2(x, parOrigin.y + TIMELINE_RULER_HEIGHT),
                         IM_COL32(156, 168, 174, 255));
    char label[16]{};
    std::snprintf(label, sizeof(label), "%02d:%02d:%02d", frame / 1800,
                  (frame / 30) % 60, frame % 30);
    parDrawList.AddText(ImVec2(x + 3.0f, parOrigin.y + 1.0f),
                         IM_COL32(190, 198, 202, 255), label);
  }
}

TimelineCanvas prepareTimelineCanvas(EditorSession& parSession,
                                     float parCanvasHeight,
                                     std::size_t parTrackCount,
                                     float parTrackHeight,
                                     film::FilmFrame parFitRange, bool parFit,
                                     int parZoomDirection) {
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const float canvas_width = ImGui::GetContentRegionAvail().x;
  const float available_width =
      std::max(1.0f, canvas_width - TIMELINE_LABEL_WIDTH);
  const float minimum_scale =
      available_width / static_cast<float>(film::MAX_FILM_FRAMES);
  const float maximum_scale =
      timelineMaximumPixelsPerFrame(canvas_width, TIMELINE_LABEL_WIDTH);
  float requested_scale = parSession.target_sequence_pixels_per_frame;
  if (parFit) {
    const film::FilmFrame range =
        parFitRange > 0 ? parFitRange : film::MAX_FILM_FRAMES;
    requested_scale = available_width / static_cast<float>(range);
  } else if (parZoomDirection != 0) {
    requested_scale *= parZoomDirection > 0 ? 1.2f : 1.0f / 1.2f;
  }
  parSession.target_sequence_pixels_per_frame =
      std::clamp(requested_scale, minimum_scale, maximum_scale);
  if (parFit) {
    ImGui::SetScrollX(0.0f);
  }

  const bool wheel_zoom =
      ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
      ImGui::GetIO().KeyCtrl && ImGui::GetIO().MouseWheel != 0.0f;
  if (wheel_zoom) {
    const float old_scale = parSession.target_sequence_pixels_per_frame;
    const float new_scale = std::clamp(
        old_scale * std::pow(1.2f, ImGui::GetIO().MouseWheel), minimum_scale,
        maximum_scale);
    if (new_scale != old_scale) {
      const float frame_under_cursor = std::max(
          0.0f, (ImGui::GetIO().MousePos.x - origin.x - TIMELINE_LABEL_WIDTH) /
                    old_scale);
      parSession.target_sequence_pixels_per_frame = new_scale;
      ImGui::SetScrollX(ImGui::GetScrollX() +
                        frame_under_cursor * (new_scale - old_scale));
    }
  }

  const float pixels_per_frame = parSession.target_sequence_pixels_per_frame;
  const float content_width = std::max(
      canvas_width, TIMELINE_LABEL_WIDTH +
                        static_cast<float>(film::MAX_FILM_FRAMES) *
                            pixels_per_frame);
  const float content_height = std::max(
      parCanvasHeight - ImGui::GetStyle().WindowPadding.y * 2.0f,
      timelineRulerHeight() +
          static_cast<float>(parTrackCount) * parTrackHeight);
  return {origin, TIMELINE_LABEL_WIDTH, pixels_per_frame, content_width,
          content_height, ImGui::GetWindowDrawList()};
}

bool scrubTimelineRuler(const char* parId, const ImVec2& parOrigin,
                         float parLabelWidth, float parContentWidth,
                         float parPixelsPerFrame,
                         EditorSession& parSession,
                         film::FilmFrame parDuration,
                         film::FilmPlayback& parPlayback) {
  ImGui::SetCursorScreenPos(ImVec2(parOrigin.x + parLabelWidth, parOrigin.y));
  ImGui::InvisibleButton(
      parId,
      ImVec2(std::max(1.0f, parContentWidth - parLabelWidth),
             TIMELINE_RULER_HEIGHT));
  if (!ImGui::IsItemActive() && !ImGui::IsItemClicked()) {
    return false;
  }
  const film::FilmFrame frame = clampMovieAuthoringCursor(
      static_cast<film::FilmFrame>(std::lround(
          (ImGui::GetIO().MousePos.x - parOrigin.x - parLabelWidth) /
          parPixelsPerFrame)));
  setMovieAuthoringCursor(parSession, parPlayback, parDuration, frame);
  return true;
}

bool dragTimelinePlayhead(const char* parId, const ImVec2& parOrigin,
                          float parLabelWidth, float parContentHeight,
                          float parPixelsPerFrame,
                          EditorSession& parSession,
                          film::FilmPlayback& parPlayback,
                          film::FilmFrame parDuration) {
  const float x = timelineFrameX(parOrigin, parLabelWidth,
                                 parSession.authoring_cursor_frame,
                                 parPixelsPerFrame) -
                  PLAYHEAD_GRAB_WIDTH * 0.5f;
  ImGui::SetCursorScreenPos(ImVec2(x, parOrigin.y));
  ImGui::InvisibleButton(parId, ImVec2(PLAYHEAD_GRAB_WIDTH,
                                       std::max(1.0f, parContentHeight)));
  if (!ImGui::IsItemActive() && !ImGui::IsItemClicked()) {
    return false;
  }
  const film::FilmFrame frame = clampMovieAuthoringCursor(
      static_cast<film::FilmFrame>(std::lround(
          (ImGui::GetIO().MousePos.x - parOrigin.x - parLabelWidth) /
          parPixelsPerFrame)));
  setMovieAuthoringCursor(parSession, parPlayback, parDuration, frame);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Drag authoring cursor (0-%d)", film::MAX_FILM_FRAMES);
  }
  return true;
}

}  // namespace kage::editor
