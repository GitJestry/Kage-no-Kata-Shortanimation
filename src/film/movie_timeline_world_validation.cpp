#include "film/movie_timeline_world_validation.hpp"

#include "animation/animation_system.hpp"

#include <algorithm>
#include <utility>

namespace kage::film {
namespace {

[[nodiscard]] bool hasPlacedInstance(const MovieTimeline& parTimeline,
                                     TargetSequenceId parSequenceId) {
  return std::any_of(
      parTimeline.instances.begin(), parTimeline.instances.end(),
      [parSequenceId](const SequenceInstance& instance) {
        return instance.sequence_id == parSequenceId;
      });
}

void addWorldDiagnostic(TimelineValidation& parValidation, bool parForBake,
                        bool parPlaced, std::string parWarning,
                        std::string parBakeError) {
  parValidation.diagnostics.push_back(
      {parForBake && parPlaced ? TimelineDiagnostic::Severity::Error
                               : TimelineDiagnostic::Severity::Warning,
       parForBake && parPlaced ? std::move(parBakeError) : std::move(parWarning)});
}

[[nodiscard]] bool targetMatchesEntity(const TimelineTarget& parTarget,
                                       const scene::EntityRecord& parEntity) {
  switch (parTarget.kind) {
    case TimelineTargetKind::RiggedEntity:
      return parEntity.rig.has_value();
    case TimelineTargetKind::Camera:
      return parEntity.camera.has_value();
    case TimelineTargetKind::PointLight:
      return parEntity.light.has_value() &&
             parEntity.light->type == scene::LightType::Point;
    case TimelineTargetKind::Sun:
      return true;
  }
  return false;
}

}  // namespace

TimelineValidation validateMovieTimelineWithWorld(
    const MovieTimeline& parTimeline, const scene::World& parWorld,
    bool parForBake, const assets::AssetRegistry* parAssets) {
  TimelineValidation validation =
      validateMovieTimeline(parTimeline, parForBake);
  for (const TargetSequence& sequence : parTimeline.sequences) {
    if (sequence.target.kind == TimelineTargetKind::Sun) {
      continue;
    }
    const bool placed = hasPlacedInstance(parTimeline, sequence.id);
    const scene::EntityRecord* entity = parWorld.findEntity(sequence.target.entity);
    if (entity == nullptr) {
      addWorldDiagnostic(
          validation, parForBake, placed,
          "A sequence targets an entity that no longer exists",
          "An orphaned sequence has instances and must be deleted before Bake");
      continue;
    }
    if (!targetMatchesEntity(sequence.target, *entity)) {
      addWorldDiagnostic(
          validation, parForBake, placed,
          "A sequence target is no longer compatible with its World entity",
          "A placed sequence target is no longer compatible with its World entity");
      continue;
    }
    if (parAssets == nullptr ||
        sequence.target.kind != TimelineTargetKind::RiggedEntity || !placed) {
      continue;
    }
    const assets::ModelAsset* asset = entity->static_mesh.has_value()
                                          ? parAssets->getLoadedAsset(
                                                entity->static_mesh->asset_library_index)
                                          : nullptr;
    for (const SequenceClip& clip : sequence.clips) {
      const auto* animation = std::get_if<RigAnimationClip>(&clip.payload);
      if (animation == nullptr) {
        continue;
      }
      const RigAnimationPlayback playback{animation->clip_id,
                                          animation->legacy_clip_index,
                                          animation->source_in,
                                          animation->source_out,
                                          animation->speed,
                                          animation->blend_in_seconds,
                                          animation->blend_out_seconds,
                                          animation->looping};
      if (asset != nullptr &&
          animation::resolveAnimationClipIndex(*asset, playback).has_value()) {
        continue;
      }
      addWorldDiagnostic(
          validation, parForBake, true,
          "A placed rig sequence references an unavailable animation asset",
          "A placed rig sequence references a missing or incompatible animation asset");
    }
  }
  return validation;
}

}  // namespace kage::film
