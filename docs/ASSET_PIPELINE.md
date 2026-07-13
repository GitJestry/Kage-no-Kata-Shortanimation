# Asset Pipeline

Runtime assets are GLB/GLTF files. `.blend` files are source assets and grading
evidence, not runtime files.

## Folders

- `assets/models/`: bundled and imported model GLBs.
- `assets/animations/`: imported compatible animation GLBs.
- `assets/textures/environments/`: catalogued HDR/LDR panoramas.
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

Runtime GLBs remain authoritative. Imports decode on a bounded worker queue.
Every mesh mode uses one complete authored index buffer and source-resolution
texture set; the engine does not generate LODs or proxy textures. GPU upload
releases transient CPU geometry and pixels. Panorama decode also runs
off-thread, limits the GPU panorama to 8192×4096 while retaining the source,
and uploads only on the main thread. `.kage_cache/` is reserved
for reproducible generated data and is never committed.
