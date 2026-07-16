# Project Scope and Evidence

## 1. Purpose of this document

This file is the authoritative manifest for feature ownership, implementation
status, claim boundaries, and evidence. A catalog feature is ready for a
submission claim when all four conditions are satisfied:

1. its implementation is present in the submission commit;
2. the animation visibly or audibly uses it;
3. source locations and validation evidence are recorded here;
4. it can be demonstrated during the presentation.

Implementation status and film-use evidence are tracked separately so that an
implemented engine contribution is not confused with the production evidence
required for the claim. Supporting infrastructure is documented, but it does
not automatically become an additional catalog feature.

The official brief requires at least 100 feature points per participant and, for
a two-person team, coverage of at least three catalog categories. Submission
deliverables include buildable code with dependencies and input data, an MPEG4
film at 2160p30 or higher, and an illustrated HTML/PDF report.

## 2. Implementation status

| Status | Meaning |
| --- | --- |
| **Implementation evidenced** | Present in the current branch with source and validation evidence below |
| **Contributor implementation pending** | Reserved ownership awaiting merged implementation and evidence |
| **Not claimed** | Supporting functionality or intentionally excluded scope |

These labels describe the engine implementation. Film-use and presentation
evidence is recorded separately in the feature sections and submission
checklist.

## 3. Feature ownership

The official catalog assigns **Rigging and Blending** 50 points as one feature.
The 25/25 split below is the team’s responsibility split and requires examiner
acceptance; it does not create two independent 50-point claims.

| Owner | Feature | Catalog category | Planned points | Implementation status |
| --- | --- | ---: | ---: | --- |
| Julian Meyer | Cinematic Engine | 4 | 50 | **Implementation evidenced** |
| Julian Meyer | Rigging and Blending contribution | 0 | 25 of shared 50 | **Implementation evidenced** |
| Julian Meyer | Shadow Mapping for Point lights | 2 | 30 | **Implementation evidenced** |
| Faouzi Homsani | Rigging and Blending contribution | 0 | 25 of shared 50 | **Implementation evidenced** |
| Faouzi Homsani | Motion Blur | 1 | 20 | **Contributor implementation pending** |
| Faouzi Homsani | Film Grain | 1 | 10 | **Contributor implementation pending** |
| Faouzi Homsani | Sound | 4 | 20 | **Contributor implementation pending** |
| Faouzi Homsani | Procedural Geometry | 0 | 30 | **Contributor implementation pending** |

Julian’s evidenced implementation subtotal is 105 planned points across
categories 0, 2, and 4. Faouzi’s Blender rigging and animation contribution is
implementation-evidenced as the other half of the shared Rigging and Blending
feature; the 25/25 split still requires examiner acceptance. His remaining
allocations enter the claimed subtotal only when their implementation and
required evidence are present.

## 4. Julian Meyer — implementation evidence

Internal mechanics and subsystem relationships are documented in
[Architecture](ARCHITECTURE.md). This section records only the claim boundary
and its evidence.

### 4.1 Cinematic Engine — Category 4, 50 points

#### Claim boundary

The contribution covers the tracked Asset Catalog, asynchronous asset loading,
persistent World authoring, frame-exact Movie authoring, reusable Target
Sequences and Sequence Instances, movement paths, typed camera/light/Sun
animation, preview, validation, save/reload, and deterministic 2160p30 Bake.

#### Primary source evidence

| Responsibility | Primary files |
| --- | --- |
| Asset catalog and stable IDs | `src/assets/project_asset_catalog.*`, `src/assets/asset_registry.*` |
| Worker-based asset loading | `src/assets/asset_streamer.*`, `src/assets/gltf_asset_loader.*` |
| Scene and World ownership | `src/scene/scene_manager.*`, `src/scene/world.*` |
| World Editor | `src/editor/editor_ui.*`, `src/editor/world_editor.*`, controllers in `src/editor/` |
| Movie model, evaluation, and edits | `src/film/movie_timeline.*`, `src/film/film_frame_state.hpp`, `src/film/timeline_edit_service.*` |
| Movie UI and persistence | `src/editor/movie_*`, `src/editor/*timeline*`, `src/engine/project_serializer.*`, `src/film/movie_timeline_serializer.*` |
| Bake output | `src/film/film_exporter.*`, `src/film/film_output_format.hpp` |

#### Evidence record

| Evidence type | Record |
| --- | --- |
| Automated validation | `film_domain_check`, `film_persistence_check`, `movie_workflow_check` |
| Editor demonstration | World Edit; sequence and camera authoring; instance reuse; invalid-overlap validation; preview; save/reload; Bake |
| Film use | Record the scenes created with the cinematic workflow and the resulting output |

![](/docs/images/movie-editor.png)

### 4.2 Rigging and Blending contribution — Category 0, shared feature

#### Claim boundary

Julian’s contribution covers GLTF skins and animation import, stable clip IDs,
TRS sampling, bind-pose construction, pose blending infrastructure,
per-primitive palettes, GPU skinning, and Movie-controlled trim, speed, loop,
and final-pose hold.

The Movie evaluator selects one authored rig clip at a time and blends from the
bind pose to the sampled pose. Automatic cross-fading between adjacent Movie
clips is outside this claim.

#### Primary source evidence

| Responsibility | Primary files |
| --- | --- |
| GLTF skin and animation import | `src/assets/gltf_asset_loader.*`, `src/assets/asset_types.hpp` |
| Sampling and pose construction | `src/animation/animator.*` |
| Film-directed evaluation | `src/animation/animation_system.*`, `src/film/movie_timeline.*` |
| GPU skinning | `src/render/gpu_mesh.*`, shader code in `src/render/mesh_renderer.cpp` |
| External animation packs | `src/assets/asset_registry.*`, `projects/kage_no_kata_assets.kage.json` |

#### Evidence record

| Evidence type | Record |
| --- | --- |
| Automated validation | Stable-ID resolution, bind pose, trim, speed, loop, and final-pose hold in `film_domain_check`; directive persistence in `film_persistence_check` |
| Editor demonstration | Visual Samurai deformation with embedded and catalog animation clips |
| Film use | Record the scene and time range showing the animated Samurai |

![](/docs/images/rigging-animation.png)

### 4.3 Point-light Shadow Mapping — Category 2, 30 points

#### Claim boundary

The contribution covers omnidirectional depth cubemaps, six depth faces per
selected Point light, deterministic selection of at most two shadow-casting
Point lights, alpha-masked and skinned casters, camera-independent caster
extraction, and fallback bindings for inactive samplers.

Directional Sun shadows, the tighter Final-mode Sun footprint, HDR rendering,
and the small filtering kernel support the renderer but are not separate catalog
claims.

#### Primary source evidence

| Responsibility | Primary files |
| --- | --- |
| Light extraction and deterministic ranking | `src/lighting/lighting_system.*`, `src/lighting/light.hpp` |
| Shadow allocation and rendering | `src/render/shadow_renderer.*` |
| Shadow sampling and material lighting | `src/render/mesh_renderer.*` |
| Alpha-mask and skinned depth submission | `src/render/gpu_mesh.*` |

#### Evidence record

| Evidence type | Record |
| --- | --- |
| Manual validation | Zero, one, two, and more than two shadow-enabled Point lights; static, alpha-masked, double-sided, and skinned casters |
| Editor demonstration | Selected lights cast cubemap shadows; non-selected Point lights remain illuminated without a shadow slot |
| Film use | Record the scene and time range where Point-light shadows are visible |

![](/docs/images/point-light-shadows.png)

## 5. Faouzi Homsani — Blender rigging and animation evidence

### 5.1 Rigging and Blending contribution — Category 0, shared feature

#### Claim boundary

Faouzi created the Blender assets used by the project, rigged the Samurai, and
authored every Samurai animation. His contribution is the character and asset
production side of the shared Rigging and Blending feature. It does not duplicate
Julian's C++ GLTF import, runtime pose evaluation, Movie control, or GPU skinning
work.

#### Evidence record

| Evidence type | Record |
| --- | --- |
| Blender source and export | The version-controlled Blender source is `assets/source_blender/samurai_rig/samurai.blend` and must be included in the submission package. The runtime Samurai GLB is `assets/models/samurai.glb`; the external bow action is registered at `assets/animations/samurai_bow.glb`. |
| Runtime integration | `projects/kage_no_kata_assets.kage.json` registers the Samurai and its animation pack; the runtime imports the rig and makes the authored clips available to the Movie workflow. |
| Validation and demonstration | The rigged Samurai deforms and plays the authored actions in the editor; the final result is illustrated in `docs/images/rigging-animation.png`. |
| Film use and presentation | Record the final film shots/time ranges that use the Samurai animations and demonstrate the rig plus animation workflow during the presentation. |

The internal 25/25 responsibility split remains subject to examiner acceptance.

### 5.2 Remaining integration evidence gate

The following allocations remain pending. Each requires an exact implementation
scope, concrete source and runtime integration points, reproducible validation,
film-use location, known limitations, and contributor reflection:

| Allocated feature | Evidence to attach after integration |
| --- | --- |
| Motion Blur | Temporal method, accumulation stage, sample count, camera/object motion handling, use in film, and measurable limits |
| Film Grain | Noise model, color space and pipeline stage, temporal behavior, controls, and use in film |
| Sound | Playback and encoding path, synchronization method, source licenses, film integration, and reproducible validation |
| Procedural Geometry | Generation algorithm, parameters, generated topology/data, use in film, and evidence that it is algorithmic rather than imported geometry |

## 6. Supporting capabilities that are not separate claims

- PBR-style GLTF materials and texture sampling;
- panorama environments and HDR framebuffer processing;
- directional Sun shadows;
- editor diagnostics and performance counters;
- asynchronous loading and GPU resource caches;
- strict current-version project persistence;
- PNG frame output and ffmpeg integration;
- tests, validation, and cleanup work;
- Movie Editor UI beyond the Cinematic Engine claim.

## 7. Explicit non-claims

Unless a contributor implementation is integrated and this manifest is updated,
the project does not claim:

- automatic animation cross-fades;
- the 80-point filtered/soft-shadow feature;
- cascaded shadow maps;
- physics, particles, displacement, hair, or ray tracing;
- motion blur, film grain, sound, or procedural geometry;
- legacy project-format migration.

## 8. Submission evidence checklist

- [ ] Every implementation-status row matches the submission commit.
- [ ] Every intended claim is visibly or audibly used in the film.
- [ ] Every intended claim has source, validation, film-use, and presentation
      evidence in this document.
- [ ] Faouzi’s remaining pending allocations are updated to evidenced
      implementations or excluded from the claimed subtotal.
- [ ] The internal 25/25 Rigging split is confirmed with the examiners.
- [ ] Asset and audio licenses are included.
- [ ] The illustrated report contains a captioned figure for each claim.
- [ ] The presentation demonstrates each claim independently and includes the
      MPEG4 film.
