# Film Workflow

FilmEditor is resizable above the status strip in Movie. Film time is
frame-exact at 30 fps; click or drag the ruler and empty lanes to scrub.

- `New Shot From View` atomically creates a film camera, movement/FOV clips,
  and the camera cut beginning at the playhead.
- `Update Camera Key From View` updates the nearest endpoint of the active
  camera movement clip without moving the world camera entity.
- Add Movement creates a cubic-Bezier transform clip with a separate monotonic
  timing curve. Rotation uses shortest-path quaternion slerp.
- Add Camera / Light creates camera cuts, FOV curves, or typed light curves.
- Animation Library imports stable-ID clips. Source In/Out, speed, looping, and
  blend boundaries are edited on the selected rig clip.
- Clips drag to move; their edge handles trim. Same-lane overlaps are rejected.
  Selecting a clip exposes only its relevant compact controls.
  `Play Selected Clip` evaluates only that clip and target; the rest of the
  world stays at authored values.

Film evaluation produces an immutable `FilmFrameState`. Rendering consumes its
transform, pose, active-camera, and light overrides without writing them to the
world. This is also the path used by export.

## Preview and export

Preview Camera uses the camera cut at the playhead and disables navigation,
picking, placement, and gizmos until preview is turned off. Leaving Movie always
clears preview and returns to the unchanged editor camera.

`Bake Movie 2160p30` validates every frame, renders one overlay-free 4×-MSAA
HDR frame at a time, retains PNGs in `output/frames/<sequence-name>/`, and then
creates `output/<sequence-name>.mp4` through ffmpeg.

```bash
ffmpeg -framerate 30 -i output/frames/Kage\ no\ Kata/frame_%06d.png \
  -c:v libx264 -preset slow -pix_fmt yuv420p -crf 14 \
  -movflags +faststart output/Kage\ no\ Kata.mp4
```
