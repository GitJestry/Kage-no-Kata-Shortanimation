# Film Workflow

## Concepts

Movie time is frame-exact at 30 fps. All ranges are half-open
`[start_frame, end_frame)`. The maximum exclusive film end is frame 3600.

- A **Target Sequence** is reusable animation for one rigged entity, camera,
  Point light, or the Sun. It begins at local frame zero.
- A **Sequence Instance** places one complete Target Sequence on the Movie
  Timeline.
- `FilmFrameState` is the evaluated, immutable runtime output consumed by camera,
  animation, lighting, preview, and Bake.

World Edit remains the owner of persistent transforms and components. Movie
evaluation never writes evaluated values back to the World.

## Authoring lanes

| Target | Available lanes |
| --- | --- |
| Rigged entity | Movement, Animation |
| Camera | Movement, FOV |
| Point light | Movement, Intensity, Color |
| Sun | Direction, Intensity, Color |

## Authoring procedure

1. Enter Movie Edit and select a target in **Animation Targets** or the viewport.
2. Create/select a Target Sequence. The current target base state is captured.
3. Add clips in the Target Sequence Timeline.
4. Move, trim, duplicate, or delete clips through the inspector/timeline.
5. Deselect the target to return to the Movie Timeline.
6. Place one or more complete Sequence Instances.
7. Preview, validate, save, reload, and Bake.

![](/docs/images/timelines.png)

Movement clips use cubic spatial control points plus a monotonic timing curve.
A clip can inherit the preceding endpoint or use an explicit start. An optional
transition fills an eligible gap and is owned by the following movement clip.

![](/docs/images/movement-path.png)

Rig Animation clips store a stable clip ID, normalized source trim, speed, and
loop state. Before the first clip the rig uses bind pose. Non-looping clips hold
their final sampled pose. Adjacent Movie clips switch directly.

## Preview

- Play/Pause advances the current Movie or selected Target Sequence.
- Scrubbing evaluates without changing World state.
- Camera sequence preview uses the evaluated sequence camera.
- Non-camera sequence preview keeps the editor camera.
- Movie preview evaluates the complete master timeline.
- Stop ends film-state consumption; no restoration pass is required.
- Camera gaps use Hold Last Camera or Black according to the Movie setting.

## Validation

Authoring diagnostics include:

- invalid frame/source ranges;
- lane/payload incompatibility;
- same-target overlap;
- camera overlap;
- orphaned or incompatible World targets;
- unresolved placed animation clips;
- film length beyond frame 3600.

Camera overlaps are editable and resolve deterministically in preview, but they
block Bake. Camera gaps are valid. Deleting a World target leaves its sequence
orphaned so the user can remove it explicitly rather than lose authored data.

## Save and Bake

Save Project writes World schema vX containing Film schema vY. Persistence
includes stable IDs, captured bases, clips, curves, instances, animation
trim/speed/loop, and camera-gap mode. Editor-only selection, zoom, scroll, and
panel geometry are not Film data.

Bake requires a playable valid timeline and evaluated camera output. It renders
overlay-free 3840×2160 PNG frames at 30 fps, writes a manifest, and encodes an
H.264 MPEG4 through `ffmpeg`.

See [Build and Validation](BUILD_AND_VALIDATION.md) for the complete acceptance
check.
