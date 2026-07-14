# Movie workflow

Movie uses frame-exact time at 30 fps. The Movie Timeline places complete,
reusable Target Sequences; a Target Sequence always starts at local frame 0.
World Edit remains the owner of the scene. Movie preview and Bake consume only
the evaluated `FilmFrameState` and never write transforms, cameras, lights, or
rig poses back to the World.

## Authoring

1. Enter Movie and use Animation Targets to select a rigged entity, camera,
   point light, or the Sun. Static meshes are not Movie targets.
2. Create a sequence for the selected target. Its base state is captured at
   creation and is independent of subsequent World Edit changes.
3. Add and edit clips in the Target Sequence Timeline. Clips are frame ranges
   `[start, end)`; same-lane overlaps are rejected. Movement, animation, and
   supported target properties are the only authorable lanes.
4. Deselect the target to return to the Movie Timeline, then place complete
   sequence instances. Edit a sequence to change every reuse of that sequence.
5. Use Play, Stop, scrubbing, Fit, and zoom from the timeline toolbar. A
   camera sequence supplies the preview camera; non-camera sequence preview
   uses the editor camera.

Movie-only selection is independent from World Edit selection. The dedicated
Deselect action returns to the Movie Timeline. The old compatibility timeline,
camera-cut controls, and clip-preview controls are not part of this workflow.

## Validation and Bake

The inspector shows authoring warnings for camera overlaps, orphaned targets,
World-target incompatibilities, unavailable animation data, invalid ranges, and
the frame limit. Camera overlaps can be previewed deterministically but block
Bake. A deleted entity leaves its sequences and instances orphaned; delete the
orphaned sequence and its instances before Bake. Rebinding is intentionally not
provided.

Bake is disabled until validation succeeds. It requires playable frames, valid
camera output, no camera overlap, no used orphan or incompatible target, valid
source/frame ranges, an end at or below frame 3600, and all placed rig-animation
assets to resolve by stable ID. Camera gaps are valid.

`Bake Movie` renders overlay-free 3840×2160 frames with 4× MSAA to
`output/frames/<movie-name>/`, then writes `output/<movie-name>.mp4` at 30 fps.
If GPU setup, frame readback, image writing, or encoding fails, Bake stops and
the Movie Inspector keeps the error visible; the editor remains usable.

## Save, reload, and acceptance check

Save Project writes World schema v6 with Movie film schema v2. Reloading keeps
stable sequence, instance, and clip IDs; captured bases; curve controls; frame
ranges; animation trim, speed, loop, and camera placement.
Editor-only selection, view state, zoom, scroll, and panel height are not saved.

For the project acceptance check, create a Samurai sequence with a movement
from the house to the shrine and an `ArmAction` at 0.5×, non-looping, with the
final 0.2 s trimmed. Create a curved camera sequence, place both on the Movie
Timeline, preview, save, and reload. Confirm the sequence/instance/clip IDs,
movement and camera transforms, timing, trim, speed, camera source, and curve
controls. Preview again, stop, and confirm World Edit transforms are unchanged.
Only then Bake. The domain and persistence portions are covered by regression
tests; viewport appearance and final rendered imagery remain visual checks.
