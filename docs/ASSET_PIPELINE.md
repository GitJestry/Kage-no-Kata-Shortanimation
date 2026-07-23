# Asset Pipeline

## Runtime and source assets

Runtime models and compatible animation packs are GLB/GLTF files.

Expected layout:

- `assets/models/` — model GLBs;
- `assets/animations/` — compatible external animation GLBs;
- `source_assets/blender/` — authoritative Blender masters;
- `assets/textures/environments/` — HDR/LDR panoramas;
- `projects/kage_no_kata_assets.kage.json` — tracked Asset Catalog.

Imported files become normal project assets; there is no separate runtime
“imported” format.

## Blender export

- Apply intended object transforms before export.
- Export glTF 2.0 GLB with Y-up coordinates.
- Use Principled BSDF inputs supported by glTF.
- Export armatures, skins, inverse bind matrices, and the intended actions for
  rigged characters.
- Use clear action names for every exported animation.

The renderer deforms skinned primitives only. A prop that must follow a rigged
animation must therefore be weighted to a deform bone; object parenting or
loose-object keyframes are not a runtime attachment mechanism. A rigid prop
uses one vertex group at weight `1.0`.

## Samurai master and export

`source_assets/blender/samurai_master.blend` is the authoritative Samurai
source. It contains one 51-bone skin, including the `prop_katana` and
`prop_saya` deform bones, and these embedded actions in stable order:

1. `ArmAction`;
2. `ReadyIdle`;
3. `samurai_bow`;
4. `samurai_cut`;
5. `samurai_stand_to_sit`;
6. `samurai_walk_no_sword`;
7. `samurai_walk_sword`.

The bind pose, idle actions, and bow carry the katana fully sheathed at one
canonical left-hip socket. Cut and sword-walk animate the katana in the hand
while keeping the empty saya on a shared close left-hip socket with a modest
downward angle, clear of the waist armor without floating away from the body.
Stand-to-sit restores the complete 13.33-second performance: the sheathed
katana/saya assembly begins in front and is counter-animated against the hips
so it remains planted on the ground until hand contact. Its original 24 fps
prop track is imported once into the 30 fps master (without a second timing
conversion), follows the hands through the pickup, settles into the canonical
hip socket during the rise, and continues through the full rise to a standing
pose. The 2.1-second no-sword walk restores the original `mixamo.com.001` gait,
hides both
weapon props, and removes net root displacement while retaining the gait's
vertical motion. Sword-walk body and katana motion remain unchanged. Both walks
are in place so world translation remains under the Movie Editor's Movement
lane.

The master preserves the approved very-dark armor textures and metallic
response. Red and gold armor colors are removed; `Laces_Rope` and the waist
details provide the contrasting gold garment accent.

Export from the repository root with Blender 4.5 LTS:

```sh
/Applications/Blender-4.5-LTS.app/Contents/MacOS/Blender \
  --background --factory-startup \
  --python tools/blender/export_samurai.py
```

The helper force-samples at 30 fps, writes `assets/models/samurai.glb`, and
fails unless the source and output retain the canonical action order, one
51-joint skin, rigid prop weights, supported sampler interpolation, canonical
hip attachment, animated sheathed pickup, hidden no-sword props, in-place
walks, a world-planted pre-contact sword, pickup motion in the hand-contact
window, exact restored durations, and all seven stable clip IDs. All seven
actions live in this single runtime GLB; the Samurai catalog entry does not use
external animation packs.

## Tameshigiri master and export

`source_assets/blender/tameshigiri_master.blend` is the authoritative
Tameshigiri source. It retains the four static platform/support meshes and uses
one `bamboo_root` plus four deform bones for the bamboo sections. Each section
is rigidly weighted at `1.0` to its matching bone. The embedded
`tameshigiri_cut` action reproduces the original object animation at 30 fps for
exactly 7.5 seconds; the planted base stays fixed while the three severed
sections move.

Export from the repository root with Blender 4.5 LTS:

```sh
/Applications/Blender-4.5-LTS.app/Contents/MacOS/Blender \
  --background --factory-startup \
  --python tools/blender/export_tameshigiri.py
```

The helper writes `assets/models/tameshigiri.glb` and rejects incorrect timing,
joint hierarchy, loose section meshes, non-rigid weights, animated supports, or
unsupported interpolation. The Asset Catalog keeps stable asset ID
`8253487953852997897`, so existing placed entities load the new skin and become
Movie rig targets without changing their world transforms.

## Catalog and stable identity

The Asset Catalog assigns every model and panorama a stable `AssetId`. World
entities persist the stable ID. During load, the registry resolves it to a
compact asset-library index used by streaming, GPU caches, rendering, and
picking.

A missing catalog may trigger the intended project-directory scan; an existing
malformed or incompatible catalog is an error.

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
