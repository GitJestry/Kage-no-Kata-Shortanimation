#pragma once

#include "math/transform.hpp"
#include "assets/asset_types.hpp"
#include "scene/entity_id.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace kage::film {

inline constexpr int FILM_FRAMES_PER_SECOND = 30;

using FilmClipId = std::uint64_t;

struct FilmMovement final {
  math::Transform start;
  math::Transform end;
  glm::vec3 position_control_1{0.0f};
  glm::vec3 position_control_2{0.0f};
  float timing_control_1 = 1.0f / 3.0f;
  float timing_control_2 = 2.0f / 3.0f;
  bool automatic_position_controls = true;
};

struct RigAnimation final {
  assets::AnimationClipId clip_id = 0;
  std::size_t legacy_clip_index = 0;
  float source_in = 0.0f;
  float source_out = 1.0f;
  float speed = 1.0f;
  float blend_in_seconds = 0.0f;
  float blend_out_seconds = 0.0f;
  bool looping = false;
};

enum class FilmPropertyKind {
  CameraFov,
  LightEnabled,
  LightIntensity,
  LightColor,
  LightRange,
};

struct FilmProperty final {
  FilmPropertyKind kind = FilmPropertyKind::CameraFov;
  glm::vec4 start_value{0.0f};
  glm::vec4 control_1{0.0f};
  glm::vec4 control_2{0.0f};
  glm::vec4 end_value{0.0f};
};

using FilmClipPayload = std::variant<FilmMovement, RigAnimation, FilmProperty>;

struct FilmClip final {
  FilmClipId id = 0;
  int start_frame = 0;
  int end_frame = 1;
  FilmClipPayload payload = FilmMovement{};
};

struct FilmTrack final {
  scene::EntityId target;
  std::vector<FilmClip> clips;
};

struct CameraCut final {
  std::uint64_t id = 0;
  int start_frame = 0;
  int end_frame = 1;
  scene::EntityId camera;
};

struct TransformOverride final {
  scene::EntityId entity;
  math::Transform transform;
};

struct PropertyOverride final {
  scene::EntityId entity;
  FilmPropertyKind kind = FilmPropertyKind::CameraFov;
  glm::vec4 value{0.0f};
};

struct RigAnimationOverride final {
  scene::EntityId entity;
  RigAnimation animation;
  float local_time_seconds = 0.0f;
  float weight = 1.0f;
};

struct FilmFrameState final {
  std::optional<scene::EntityId> active_camera;
  std::vector<TransformOverride> transforms;
  std::vector<PropertyOverride> properties;
  std::vector<RigAnimationOverride> rig_animations;
};

struct CameraSample final {
  scene::EntityId camera;
  std::optional<math::Transform> transform;
  std::optional<float> vertical_fov_degrees;
};

struct FilmTimeline final {
  std::string name = "Kage no Kata";
  int duration_frames = 300;
  std::vector<CameraCut> camera_cuts;
  std::vector<FilmTrack> tracks;
  FilmClipId next_clip_id = 1;
  std::uint64_t next_cut_id = 1;

  [[nodiscard]] const CameraCut* findCameraCut(double parFrame) const;
  [[nodiscard]] FilmTrack* findTrack(scene::EntityId parTarget);
  [[nodiscard]] const FilmTrack* findTrack(scene::EntityId parTarget) const;
  [[nodiscard]] bool canPlaceClip(scene::EntityId parTarget,
                                  int parStartFrame, int parEndFrame,
                                  FilmClipId parIgnoring = 0,
                                  const FilmClipPayload* parPayload = nullptr) const;
  [[nodiscard]] FilmClip* addClip(scene::EntityId parTarget,
                                  int parStartFrame, int parEndFrame,
                                  FilmClipPayload parPayload);
  [[nodiscard]] bool moveClip(FilmClipId parClip, int parStartFrame,
                              int parEndFrame);
  [[nodiscard]] bool removeClip(FilmClipId parClip);
  [[nodiscard]] FilmClip* findClip(FilmClipId parClip);
  [[nodiscard]] const FilmClip* findClip(FilmClipId parClip) const;
  [[nodiscard]] FilmFrameState evaluate(double parFrame) const;
  void evaluate(double parFrame, FilmFrameState& parState) const;
  void evaluateClip(FilmClipId parClip, double parFrame,
                    FilmFrameState& parState) const;
  [[nodiscard]] std::optional<CameraSample> evaluateCamera(
      double parFrame) const;
  [[nodiscard]] std::optional<math::Transform> evaluateTransform(
      scene::EntityId parEntity, double parFrame) const;
  [[nodiscard]] std::optional<std::string> validate() const;
};

// The exporter and existing project container use this domain name. There is
// only one implementation and one owner: FilmTimeline.
using FilmSequence = FilmTimeline;

struct FilmPlayback final {
  double playhead_frame = 0.0;
  bool playing = false;
  bool looping = true;

  void update(float parDeltaSeconds, const FilmTimeline& parTimeline);
};

[[nodiscard]] math::Transform sampleMovement(const FilmMovement& parMovement,
                                             float parT);

}  // namespace kage::film
