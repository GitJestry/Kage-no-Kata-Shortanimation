# Asset Pipeline

## Runtime and source assets

Runtime models and compatible animation packs are GLB/GLTF files.

Expected layout:

- `assets/models/` — model GLBs;
- `assets/animations/` — compatible external animation GLBs;
- `assets/textures/environments/` — HDR/LDR panoramas;
- `projects/kage_no_kata_assets.kage.json` — tracked Asset Catalog v2.

Imported files become normal project assets; there is no separate runtime
“imported” format.

## Blender export

- Apply intended object transforms before export.
- Export glTF 2.0 GLB with Y-up coordinates.
- Use Principled BSDF inputs supported by glTF.
- Export armatures, skins, inverse bind matrices, and the intended actions for
  rigged characters.
- Use clear action names. The submitted Samurai base model provides
  `ReadyIdle` and `ArmAction`.

## Catalog and stable identity

The Asset Catalog assigns every model and panorama a stable `AssetId`. World
entities persist the stable ID. During load, the registry resolves it to a
compact asset-library index used by streaming, GPU caches, rendering, and
picking.

The current loader accepts Catalog schema v2 only. A missing catalog may trigger
the intended project-directory scan; an existing malformed or incompatible
catalog is an error.

## Model import

`Import Model…`:

1. validates a GLB/GLTF source;
2. decodes it once to reject empty/non-renderable data;
3. copies it into `assets/models/`;
4. registers it with a stable ID in the catalog;
5. starts placement and normal asynchronous loading.

## Animation packs

Compatible external animation files are declared by `animation_packs` on a
catalog model entry. When the base model finishes CPU loading, the runtime:

1. decodes each pack GLB/GLTF;
2. selects the requested clip;
3. validates skeleton compatibility;
4. remaps channels by joint name;
5. appends the clip with a stable animation ID.

Embedded GLTF animations continue to load directly with their model. The Movie
Editor selects already loaded clips; it does not import animations interactively.

## Asynchronous loading and GPU ownership

`AssetStreamer` uses a bounded worker pool and priority queue. Workers perform
file IO and GLTF decoding only. The main thread:

- commits load state;
- applies animation packs;
- uploads buffers and source-resolution textures;
- attaches loaded bounds/rig data to existing instances;
- releases transient CPU geometry and decoded pixels.

`MeshResourceCache` stores one full-fidelity GPU mesh per asset-library index.
Textures are shared through `TextureResourceCache`. Bounds mode is the only
geometry-free viewport mode; Solid, Material, and Final use the authored mesh.

Panorama decoding is also asynchronous. GPU panorama dimensions are bounded for
predictable memory usage while the source asset remains unchanged.

## Failure behavior

Load state is explicit: MetadataReady, Queued, CpuLoading, GpuUploading, Ready,
or Error. Errors remain visible in the editor. Invalid data is not silently
converted into a different asset, and OpenGL creation is never performed by a
worker thread.

![](/docs/images/asset-pipeline.png)
