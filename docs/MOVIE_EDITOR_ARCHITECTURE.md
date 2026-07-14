# Movie Editor Architecture

## Scope

The Movie Editor supports:

- rigged entities: movement and animation;
- cameras: movement and FOV;
- point lights: movement, intensity and color;
- sun: direction, intensity and color.

No additional authoring functionality is part of the submitted scope.

## Core model

- MovieTimeline owns TargetSequences and SequenceInstances.
- A TargetSequence contains target-local clips beginning at frame zero.
- A SequenceInstance places one complete sequence on the Movie Timeline.
- Time uses integer frames at 30 fps.
- Maximum film end is frame 3600.
- Clip IDs are unique within one MovieTimeline.

## Runtime boundary

- World Edit owns persistent authored World state.
- Movie evaluation never mutates World state.
- FilmFrameState is the only output consumed by rendering, animation, camera
  and lighting.
- Stopping preview stops consumption of FilmFrameState; no restoration pass is
  required.

## Editing

- TimelineEditService owns timeline mutation rules.
- ImGui draws state and submits edit operations.
- UI code must not duplicate overlap, duration, continuity or frame-limit rules.
- The active timeline view is derived from Movie selection, not stored
  independently.

## Evaluation

- Sequence evaluation starts from captured base state.
- Empty intervals hold the previous evaluated value.
- Rig animation uses bind pose before its first clip and holds the last pose
  after a completed clip.
- Camera gaps use the selected Hold or Black policy.
- Same-target non-camera instances cannot overlap.
- Camera overlap is editable but invalid for Bake.

## Persistence

- The current project and film schemas are the only authoring format.
- Compatibility code exists only where explicitly retained.
- Loader-only compatibility structures must not become a second runtime model.

## UI responsibilities

- Animation Targets selects valid Movie targets.
- Movie Inspector edits the active target, sequence, clip or instance.
- Target Sequence Timeline edits local clips.
- Movie Timeline places reusable sequence instances.
- Both timelines share common layout, zoom, playhead and coordinate helpers.

## Non-goals

- No general-purpose nonlinear editor.
- No speculative authoring features.
- No new dependency.
- No hidden functionality preserved only for possible future use.