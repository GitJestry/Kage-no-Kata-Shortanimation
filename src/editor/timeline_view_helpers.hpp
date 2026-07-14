#pragma once

#include "engine/engine_core.hpp"
#include "film/movie_timeline.hpp"

#include <imgui.h>

#include <cstdint>
#include <optional>
#include <string>

namespace kage::editor {

struct EditorSession;

enum class MovieWidgetIdKind : std::uint64_t {
  TimelineTarget = 1,
  TargetSequence = 2,
  SequenceClip = 3,
  SequenceInstance = 4,
  AnimationAssetClip = 5,
  TimelineLane = 6,
  TimelineTargetRow = 7,
};

void pushMovieWidgetId(MovieWidgetIdKind parKind, std::uint64_t parId);

[[nodiscard]] const char* movieTargetKindLabel(film::TimelineTargetKind parKind);
[[nodiscard]] std::string movieTargetLabel(const engine::EngineCore& parEngine,
                                           const film::TimelineTarget& parTarget);
[[nodiscard]] std::optional<film::CapturedTargetBaseState> captureMovieTargetBase(
    const engine::EngineCore& parEngine, const film::TimelineTarget& parTarget);
[[nodiscard]] const assets::ModelAsset* movieAnimationAsset(
    const engine::EngineCore& parEngine, const film::TargetSequence& parSequence);

void drawCapturedMovieBaseSummary(const film::CapturedTargetBaseState& parBase);
[[nodiscard]] const char* movieClipLabel(const film::SequenceClipPayload& parPayload);

[[nodiscard]] float timelineFrameX(const ImVec2& parOrigin, float parLabelWidth,
                                   film::FilmFrame parFrame,
                                   float parPixelsPerFrame);
[[nodiscard]] film::FilmFrame timelineFrameDelta(float parMouseX,
                                                  float parGestureMouseX,
                                                  float parPixelsPerFrame);
void drawTimelinePlayhead(ImDrawList& parDrawList, float parX, float parTop,
                          float parBottom);
[[nodiscard]] float timelineRulerHeight();
void drawTimelineRuler(ImDrawList& parDrawList, const ImVec2& parOrigin,
                       float parLabelWidth, float parPixelsPerFrame,
                       float parContentWidth);
[[nodiscard]] float timelineMinimumPixelsPerFrame(float parContentWidth,
                                                   float parLabelWidth);
[[nodiscard]] float timelineFitPixelsPerFrame(float parContentWidth,
                                               float parLabelWidth,
                                               film::FilmFrame parRangeFrames);
void setTimelinePixelsPerFrame(EditorSession& parSession,
                               float parPixelsPerFrame,
                               float parContentWidth, float parLabelWidth);
// Ctrl + wheel zooms around the cursor while plain wheel/trackpad gestures are
// left to ImGui scrolling. Timeline data is never changed by this view operation.
bool updateTimelineZoom(EditorSession& parSession, const ImVec2& parOrigin,
                        float parContentWidth, float parLabelWidth);
bool scrubTimelineRuler(const char* parId, const ImVec2& parOrigin,
                         float parLabelWidth, float parContentWidth,
                         float parPixelsPerFrame,
                         EditorSession& parSession,
                         film::FilmFrame parDuration,
                         film::FilmPlayback& parPlayback);
bool dragTimelinePlayhead(const char* parId, const ImVec2& parOrigin,
                          float parLabelWidth, float parContentHeight,
                          float parPixelsPerFrame,
                          EditorSession& parSession,
                          film::FilmPlayback& parPlayback,
                          film::FilmFrame parDuration);

}  // namespace kage::editor
