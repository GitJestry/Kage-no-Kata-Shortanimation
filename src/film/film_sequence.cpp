#include "film/film_sequence.hpp"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace {

template <typename Value>
[[nodiscard]] Value cubicBezier(const Value& parStart,
                                const Value& parControl1,
                                const Value& parControl2,
                                const Value& parEnd, float parT) {
  const float inverse = 1.0f - parT;
  return inverse * inverse * inverse * parStart +
         3.0f * inverse * inverse * parT * parControl1 +
         3.0f * inverse * parT * parT * parControl2 +
         parT * parT * parT * parEnd;
}

[[nodiscard]] float clipT(const kage::film::FilmClip& parClip,
                          double parFrame) {
  return std::clamp(static_cast<float>(
                        (parFrame - parClip.start_frame) /
                        static_cast<double>(parClip.end_frame -
                                            parClip.start_frame)),
                    0.0f, 1.0f);
}

[[nodiscard]] bool isActive(const kage::film::FilmClip& parClip,
                            double parFrame) {
  return parFrame >= parClip.start_frame && parFrame < parClip.end_frame;
}

[[nodiscard]] int laneFor(const kage::film::FilmClipPayload& parPayload) {
  if (std::holds_alternative<kage::film::FilmMovement>(parPayload)) {
    return 0;
  }
  if (std::holds_alternative<kage::film::RigAnimation>(parPayload)) {
    return 1;
  }
  return 2 + static_cast<int>(
                 std::get<kage::film::FilmProperty>(parPayload).kind);
}

}  // namespace

namespace kage::film {

math::Transform sampleMovement(const FilmMovement& parMovement, float parT) {
  const float linear_t = std::clamp(parT, 0.0f, 1.0f);
  const float timing_1 = std::clamp(parMovement.timing_control_1, 0.0f, 1.0f);
  const float timing_2 = std::clamp(parMovement.timing_control_2,
                                    timing_1, 1.0f);
  const float t = cubicBezier(0.0f, timing_1, timing_2, 1.0f, linear_t);
  const glm::vec3 delta =
      parMovement.end.translation - parMovement.start.translation;
  const glm::vec3 control_1 =
      parMovement.automatic_position_controls
          ? parMovement.start.translation + delta / 3.0f
          : parMovement.position_control_1;
  const glm::vec3 control_2 =
      parMovement.automatic_position_controls
          ? parMovement.end.translation - delta / 3.0f
          : parMovement.position_control_2;
  math::Transform result;
  result.translation = cubicBezier(parMovement.start.translation, control_1,
                                   control_2, parMovement.end.translation, t);
  glm::quat end_rotation = parMovement.end.rotation;
  if (glm::dot(parMovement.start.rotation, end_rotation) < 0.0f) {
    end_rotation = -end_rotation;
  }
  result.rotation = glm::normalize(
      glm::slerp(parMovement.start.rotation, end_rotation, t));
  result.scale = glm::mix(parMovement.start.scale, parMovement.end.scale, t);
  return result;
}

const CameraCut* FilmTimeline::findCameraCut(double parFrame) const {
  const auto cut = std::find_if(camera_cuts.begin(), camera_cuts.end(),
                                [parFrame](const CameraCut& item) {
                                  return parFrame >= item.start_frame &&
                                         parFrame < item.end_frame;
                                });
  return cut == camera_cuts.end() ? nullptr : &*cut;
}

FilmTrack* FilmTimeline::findTrack(scene::EntityId parTarget) {
  const auto track = std::find_if(tracks.begin(), tracks.end(),
                                  [parTarget](const FilmTrack& item) {
                                    return item.target == parTarget;
                                  });
  return track == tracks.end() ? nullptr : &*track;
}

const FilmTrack* FilmTimeline::findTrack(scene::EntityId parTarget) const {
  const auto track = std::find_if(tracks.begin(), tracks.end(),
                                  [parTarget](const FilmTrack& item) {
                                    return item.target == parTarget;
                                  });
  return track == tracks.end() ? nullptr : &*track;
}

bool FilmTimeline::canPlaceClip(scene::EntityId parTarget, int parStartFrame,
                                int parEndFrame,
                                FilmClipId parIgnoring,
                                const FilmClipPayload* parPayload) const {
  if (!parTarget.isValid() || parStartFrame < 0 ||
      parEndFrame <= parStartFrame || parEndFrame > duration_frames) {
    return false;
  }
  const FilmTrack* track = findTrack(parTarget);
  if (track == nullptr) {
    return true;
  }
  return std::none_of(track->clips.begin(), track->clips.end(),
                      [&](const FilmClip& clip) {
                        return clip.id != parIgnoring &&
                               (parPayload == nullptr ||
                                laneFor(clip.payload) == laneFor(*parPayload)) &&
                               parStartFrame < clip.end_frame &&
                               parEndFrame > clip.start_frame;
                      });
}

FilmClip* FilmTimeline::addClip(scene::EntityId parTarget, int parStartFrame,
                                int parEndFrame,
                                FilmClipPayload parPayload) {
  if (!canPlaceClip(parTarget, parStartFrame, parEndFrame, 0, &parPayload)) {
    return nullptr;
  }
  FilmTrack* track = findTrack(parTarget);
  if (track == nullptr) {
    tracks.push_back({parTarget, {}});
    track = &tracks.back();
  }
  FilmClip clip;
  clip.id = next_clip_id++;
  clip.start_frame = parStartFrame;
  clip.end_frame = parEndFrame;
  clip.payload = std::move(parPayload);
  const auto position = std::lower_bound(
      track->clips.begin(), track->clips.end(), clip.start_frame,
      [](const FilmClip& item, int frame) { return item.start_frame < frame; });
  return &*track->clips.insert(position, std::move(clip));
}

bool FilmTimeline::moveClip(FilmClipId parClip, int parStartFrame,
                            int parEndFrame) {
  for (FilmTrack& track : tracks) {
    const auto clip = std::find_if(track.clips.begin(), track.clips.end(),
                                   [parClip](const FilmClip& item) {
                                     return item.id == parClip;
                                   });
    if (clip == track.clips.end() ||
        !canPlaceClip(track.target, parStartFrame, parEndFrame, parClip,
                      &clip->payload)) {
      continue;
    }
    clip->start_frame = parStartFrame;
    clip->end_frame = parEndFrame;
    std::sort(track.clips.begin(), track.clips.end(),
              [](const FilmClip& left, const FilmClip& right) {
                return left.start_frame < right.start_frame;
              });
    return true;
  }
  return false;
}

bool FilmTimeline::removeClip(FilmClipId parClip) {
  for (FilmTrack& track : tracks) {
    const auto old_size = track.clips.size();
    std::erase_if(track.clips, [parClip](const FilmClip& item) {
      return item.id == parClip;
    });
    if (track.clips.size() != old_size) {
      return true;
    }
  }
  return false;
}

FilmClip* FilmTimeline::findClip(FilmClipId parClip) {
  for (FilmTrack& track : tracks) {
    const auto found = std::find_if(track.clips.begin(), track.clips.end(),
                                    [parClip](const FilmClip& item) {
                                      return item.id == parClip;
                                    });
    if (found != track.clips.end()) {
      return &*found;
    }
  }
  return nullptr;
}

const FilmClip* FilmTimeline::findClip(FilmClipId parClip) const {
  for (const FilmTrack& track : tracks) {
    const auto found = std::find_if(track.clips.begin(), track.clips.end(),
                                    [parClip](const FilmClip& item) {
                                      return item.id == parClip;
                                    });
    if (found != track.clips.end()) {
      return &*found;
    }
  }
  return nullptr;
}

void FilmTimeline::evaluate(double parFrame, FilmFrameState& state) const {
  state.active_camera.reset();
  state.transforms.clear();
  state.properties.clear();
  state.rig_animations.clear();
  if (const CameraCut* cut = findCameraCut(parFrame)) {
    state.active_camera = cut->camera;
  }
  for (const FilmTrack& track : tracks) {
    for (const FilmClip& clip : track.clips) {
      if (!isActive(clip, parFrame)) {
        continue;
      }
      const float t = clipT(clip, parFrame);
      if (const auto* movement = std::get_if<FilmMovement>(&clip.payload)) {
        state.transforms.push_back({track.target, sampleMovement(*movement, t)});
      } else if (const auto* animation =
                     std::get_if<RigAnimation>(&clip.payload)) {
        const float elapsed = static_cast<float>(parFrame - clip.start_frame) /
                              FILM_FRAMES_PER_SECOND;
        float weight = 1.0f;
        if (animation->blend_in_seconds > 0.0f) {
          weight = std::min(weight, elapsed / animation->blend_in_seconds);
        }
        const float remaining =
            static_cast<float>(clip.end_frame - parFrame) /
            FILM_FRAMES_PER_SECOND;
        if (animation->blend_out_seconds > 0.0f) {
          weight = std::min(weight,
                            remaining / animation->blend_out_seconds);
        }
        state.rig_animations.push_back(
            {track.target, *animation,
             elapsed * animation->speed,
             std::clamp(weight, 0.0f, 1.0f)});
      } else if (const auto* property =
                     std::get_if<FilmProperty>(&clip.payload)) {
        state.properties.push_back(
            {track.target, property->kind,
             cubicBezier(property->start_value, property->control_1,
                         property->control_2, property->end_value, t)});
      }
    }
  }
}

void FilmTimeline::evaluateClip(FilmClipId parClip, double parFrame,
                                FilmFrameState& state) const {
  state.active_camera.reset();
  state.transforms.clear();
  state.properties.clear();
  state.rig_animations.clear();
  for (const FilmTrack& track : tracks) {
    for (const FilmClip& clip : track.clips) {
      if (clip.id != parClip || !isActive(clip, parFrame)) {
        continue;
      }
      const float t = clipT(clip, parFrame);
      if (const auto* movement = std::get_if<FilmMovement>(&clip.payload)) {
        state.transforms.push_back({track.target, sampleMovement(*movement, t)});
      } else if (const auto* animation =
                     std::get_if<RigAnimation>(&clip.payload)) {
        const float elapsed = static_cast<float>(parFrame - clip.start_frame) /
                              FILM_FRAMES_PER_SECOND;
        state.rig_animations.push_back(
            {track.target, *animation, elapsed * animation->speed, 1.0f});
      } else if (const auto* property =
                     std::get_if<FilmProperty>(&clip.payload)) {
        state.properties.push_back(
            {track.target, property->kind,
             cubicBezier(property->start_value, property->control_1,
                         property->control_2, property->end_value, t)});
      }
      return;
    }
  }
}

FilmFrameState FilmTimeline::evaluate(double parFrame) const {
  FilmFrameState state;
  evaluate(parFrame, state);
  return state;
}

std::optional<CameraSample> FilmTimeline::evaluateCamera(
    double parFrame) const {
  const CameraCut* cut = findCameraCut(parFrame);
  if (cut == nullptr) {
    return std::nullopt;
  }
  CameraSample sample;
  sample.camera = cut->camera;
  sample.transform = evaluateTransform(sample.camera, parFrame);
  if (const FilmTrack* track = findTrack(sample.camera)) {
    for (const FilmClip& clip : track->clips) {
      const auto* property = std::get_if<FilmProperty>(&clip.payload);
      if (!isActive(clip, parFrame) || property == nullptr ||
          property->kind != FilmPropertyKind::CameraFov) {
        continue;
      }
      sample.vertical_fov_degrees =
          cubicBezier(property->start_value, property->control_1,
                      property->control_2, property->end_value,
                      clipT(clip, parFrame))
              .x;
      break;
    }
  }
  return sample;
}

std::optional<math::Transform> FilmTimeline::evaluateTransform(
    scene::EntityId parEntity, double parFrame) const {
  const FilmTrack* track = findTrack(parEntity);
  if (track == nullptr) {
    return std::nullopt;
  }
  for (const FilmClip& clip : track->clips) {
    const auto* movement = std::get_if<FilmMovement>(&clip.payload);
    if (movement != nullptr && isActive(clip, parFrame)) {
      return sampleMovement(*movement, clipT(clip, parFrame));
    }
  }
  return std::nullopt;
}

std::optional<std::string> FilmTimeline::validate() const {
  if (duration_frames <= 0) {
    return "Film duration must be positive";
  }
  int previous_cut_end = 0;
  for (const CameraCut& cut : camera_cuts) {
    if (!cut.camera.isValid() || cut.start_frame < previous_cut_end ||
        cut.end_frame <= cut.start_frame || cut.end_frame > duration_frames) {
      return "Camera cuts overlap or have an invalid frame range";
    }
    previous_cut_end = cut.end_frame;
  }
  for (const FilmTrack& track : tracks) {
    if (!track.target.isValid()) {
      return "A film track has an invalid target";
    }
    for (const FilmClip& clip : track.clips) {
      if (clip.id == 0 || clip.end_frame <= clip.start_frame ||
          clip.end_frame > duration_frames) {
        return "Film clips overlap or have an invalid frame range";
      }
      if (const auto* movement = std::get_if<FilmMovement>(&clip.payload);
          movement != nullptr &&
          (movement->timing_control_1 < 0.0f ||
           movement->timing_control_2 < movement->timing_control_1 ||
           movement->timing_control_2 > 1.0f)) {
        return "A movement clip has an invalid timing curve";
      }
      if (const auto* animation = std::get_if<RigAnimation>(&clip.payload);
          animation != nullptr &&
          (animation->source_in < 0.0f ||
           animation->source_out <= animation->source_in ||
           animation->source_out > 1.0f || animation->speed < 0.0f)) {
        return "A rig clip has an invalid source range or speed";
      }
      if (!canPlaceClip(track.target, clip.start_frame, clip.end_frame,
                        clip.id, &clip.payload)) {
        return "Film clips overlap on the same lane";
      }
    }
  }
  return std::nullopt;
}

void FilmPlayback::update(float parDeltaSeconds,
                          const FilmTimeline& parTimeline) {
  if (!playing || parTimeline.duration_frames <= 0) {
    return;
  }
  playhead_frame += static_cast<double>(std::max(parDeltaSeconds, 0.0f)) *
                    FILM_FRAMES_PER_SECOND;
  if (playhead_frame < parTimeline.duration_frames) {
    return;
  }
  if (looping) {
    playhead_frame = std::fmod(
        playhead_frame, static_cast<double>(parTimeline.duration_frames));
  } else {
    playhead_frame = static_cast<double>(parTimeline.duration_frames - 1);
    playing = false;
  }
}

}  // namespace kage::film
