#pragma once

#include "assets/asset_types.hpp"
#include "film/film_frame_state.hpp"
#include "math/transform.hpp"
#include "scene/entity_id.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace kage::film {

using FilmFrame = std::int32_t;
using TargetSequenceId = std::uint64_t;
using SequenceInstanceId = std::uint64_t;
using SequenceClipId = std::uint64_t;

inline constexpr FilmFrame FILM_FPS = 30;
inline constexpr FilmFrame MAX_FILM_FRAMES = 3600;

enum class TimelineTargetKind { RiggedEntity, Camera, PointLight, Sun };

struct TimelineTarget final {
  TimelineTargetKind kind = TimelineTargetKind::RiggedEntity;
  scene::EntityId entity;

  friend bool operator==(const TimelineTarget&, const TimelineTarget&) = default;
};

struct CapturedCameraState final {
  float vertical_fov_degrees = 45.0f;
  float near_plane = 0.1f;
  float far_plane = 1000.0f;
};

struct CapturedPointLightState final {
  bool enabled = true;
  glm::vec3 color{1.0f};
  float intensity = 1.0f;
  float range = 10.0f;
  bool casts_shadows = false;
};

struct CapturedEntityBaseState final {
  math::Transform transform;
  std::optional<CapturedCameraState> camera;
  std::optional<CapturedPointLightState> point_light;
};

struct CapturedSunBaseState final {
  glm::vec3 direction_to_sun{0.0f, -1.0f, 0.0f};
  glm::vec3 color{1.0f};
  float intensity = 1.0f;
};

using CapturedTargetBaseState =
    std::variant<CapturedEntityBaseState, CapturedSunBaseState>;

enum class MovementStartMode { PreviousEndpoint, ExplicitPosition };

struct MovementCurve final {
  glm::vec3 position_control_1{0.0f};
  glm::vec3 position_control_2{0.0f};
  float timing_control_1 = 1.0f / 3.0f;
  float timing_control_2 = 2.0f / 3.0f;
  bool automatic_position_controls = true;
};

struct MovementTransition final {
  bool enabled = false;
  MovementCurve curve;
};

struct MovementClip final {
  MovementStartMode start_mode = MovementStartMode::PreviousEndpoint;
  std::optional<math::Transform> explicit_start;
  math::Transform end;
  MovementCurve curve;
  MovementTransition transition_before;
};

struct RigAnimationClip final {
  assets::AnimationClipId clip_id = 0;
  std::size_t legacy_clip_index = 0;
  float source_in = 0.0f;
  float source_out = 1.0f;
  float speed = 1.0f;
  bool looping = false;
  float blend_in_seconds = 0.0f;
  float blend_out_seconds = 0.0f;
};

enum class PropertyKind {
  CameraFov,
  PointLightIntensity,
  PointLightColor,
  SunDirection,
  SunIntensity,
  SunColor,
  LegacyPointLightEnabled,
  LegacyPointLightRange,
};

struct PropertyClip final {
  PropertyKind kind = PropertyKind::CameraFov;
  glm::vec4 start_value{0.0f};
  glm::vec4 control_1{0.0f};
  glm::vec4 control_2{0.0f};
  glm::vec4 end_value{0.0f};
};

using SequenceClipPayload =
    std::variant<MovementClip, RigAnimationClip, PropertyClip>;

[[nodiscard]] int laneFor(const SequenceClipPayload& parPayload);

struct SequenceClip final {
  SequenceClipId id = 0;
  FilmFrame start_frame = 0;
  FilmFrame end_frame = 1;
  SequenceClipPayload payload = MovementClip{};
};

struct TargetSequence final {
  TargetSequenceId id = 0;
  std::string name;
  TimelineTarget target;
  CapturedTargetBaseState captured_base = CapturedEntityBaseState{};
  std::vector<SequenceClip> clips;

  [[nodiscard]] FilmFrame durationFrames() const;
};

struct SequenceInstance final {
  SequenceInstanceId id = 0;
  TargetSequenceId sequence_id = 0;
  FilmFrame start_frame = 0;
};

enum class CameraGapMode { HoldLastCameraState, Black };

struct MovieTimeline final {
  std::string name = "Kage no Kata";
  CameraGapMode camera_gap_mode = CameraGapMode::HoldLastCameraState;
  std::vector<TargetSequence> sequences;
  std::vector<SequenceInstance> instances;
  TargetSequenceId next_sequence_id = 1;
  SequenceInstanceId next_instance_id = 1;
  SequenceClipId next_clip_id = 1;

  [[nodiscard]] FilmFrame durationFrames() const;
  [[nodiscard]] TargetSequence* findSequence(TargetSequenceId parId);
  [[nodiscard]] const TargetSequence* findSequence(TargetSequenceId parId) const;
  [[nodiscard]] SequenceClip* findClip(SequenceClipId parId);
  [[nodiscard]] const SequenceClip* findClip(SequenceClipId parId) const;
};

struct MovementPathSegment final {
  glm::vec3 start{0.0f};
  glm::vec3 control_1{0.0f};
  glm::vec3 control_2{0.0f};
  glm::vec3 end{0.0f};
};

struct ResolvedMovementPath final {
  MovementPathSegment movement;
  std::optional<MovementPathSegment> transition_before;
};

// Resolves the spatial path exactly as sequence evaluation derives movement
// starts. Timing controls affect traversal but do not change this geometry.
[[nodiscard]] std::optional<ResolvedMovementPath> resolveMovementPath(
    const TargetSequence& parSequence, SequenceClipId parClipId);

enum class MovieTimelineOrigin { NewProject, LoadedProject };

[[nodiscard]] bool requiresInitialFilmCamera(
    MovieTimelineOrigin parOrigin, const MovieTimeline& parTimeline);

// A default project creates its first film camera from the editor view at
// frame zero.  Loaded projects and projects with any film data do not.
[[nodiscard]] std::optional<FilmFrame> initialFilmCameraCreationFrame(
    MovieTimelineOrigin parOrigin, const MovieTimeline& parTimeline);

struct FilmPlayback final {
  double playhead_frame = 0.0;
  bool playing = false;
  bool previewing = false;
  bool looping = true;

  void update(float parDeltaSeconds, const MovieTimeline& parTimeline);
  void update(float parDeltaSeconds, FilmFrame parDuration);
};

// Preview and export both consume the immutable movie state even while normal
// playback is stopped.  This keeps camera preview independent from playback.
[[nodiscard]] inline bool requiresFilmFrameState(bool parMovieWorkspace,
                                                 bool parShotPreview,
                                                 double parFilmFrame,
                                                 const FilmPlayback& parPlayback) {
  return parMovieWorkspace &&
         (parFilmFrame >= 0.0 || parShotPreview || parPlayback.previewing);
}

struct TimelineDiagnostic final {
  enum class Severity { Warning, Error };

  Severity severity = Severity::Error;
  std::string message;
};

struct TimelineValidation final {
  std::vector<TimelineDiagnostic> diagnostics;

  [[nodiscard]] bool hasErrors() const;
};

// The evaluator is pure: it reads only captured sequence data and never reads
// or mutates World state.  The output remains a FilmFrameState so runtime
// integration can happen in the later cutover milestone.
void evaluateTargetSequence(const TargetSequence& parSequence,
                            FilmFrame parLocalFrame,
                            FilmFrameState& parState);
[[nodiscard]] std::optional<FilmFrameState> evaluateTargetSequencePreview(
    const MovieTimeline& parTimeline, TargetSequenceId parSequenceId,
    FilmFrame parFrame);
[[nodiscard]] FilmFrameState evaluateMovieTimeline(const MovieTimeline& parTimeline,
                                                    FilmFrame parFrame);
[[nodiscard]] TimelineValidation validateMovieTimeline(
    const MovieTimeline& parTimeline, bool parForBake = false);

[[nodiscard]] bool isCameraSequence(const TargetSequence& parSequence);
[[nodiscard]] bool isValidTimelineTarget(const TimelineTarget& parTarget);
[[nodiscard]] bool isPayloadCompatibleWithTarget(
    TimelineTargetKind parTargetKind, const SequenceClipPayload& parPayload);
[[nodiscard]] bool isAuthorablePayloadForTarget(
    TimelineTargetKind parTargetKind, const SequenceClipPayload& parPayload);

}  // namespace kage::film
