# Asset Pipeline

Runtime assets are GLB/GLTF files. `.blend` files are source assets and grading
evidence, not runtime files.

## Folders

- `assets/models/`: bundled and imported model GLBs.
- `assets/animations/`: imported compatible animation GLBs.
- `assets/source_blender/`: Blender source files.
- `projects/kage_no_kata_assets.kage.json`: tracked asset catalog.

There is no separate `imported` folder. Once accepted, an imported file is a
normal project asset.

## Blender Export Rules

- Apply object transforms before export.
- Export as glTF 2.0 GLB, Y-up.
- Use Principled BSDF material inputs that glTF can export.
- Export armature, skinning, inverse bind matrices, and all actions for rigged
  characters.
- Keep clip names clear; the Samurai currently exports `ReadyIdle` and
  `ArmAction`.

## Engine Import

`Import Model...` validates GLB/GLTF, copies it into `assets/models/`, registers
it in the project catalog, and starts placement.

`Import Animation...` is available for a selected rigged entity. It validates
joint names and hierarchy against the selected skeleton, copies the file into
`assets/animations/`, and adds compatible clips to the Timeline.

## Checks

```bash
ctest --test-dir build --output-on-failure
build/asset_import_check assets/models/samurai.glb --require-rig --min-animation-clips 2 --min-real-clip-duration 1.0 --min-clip-keys 3
```
