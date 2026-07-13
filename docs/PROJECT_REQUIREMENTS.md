# Requirements, Scoring, and Ownership

## Submission Rules

- Each team member contributes at least 100 feature points.
- A two-person project covers at least three feature categories.
- The submission contains buildable source code, dependencies, and input data.
- Blender assets and third-party resources include their licenses.

| Date | Deliverable |
| --- | --- |
| 21 July 2026, 23:59 | Preview video |
| 23 July 2026, 23:59 | MPEG4 film at 2160p30 or higher |
| 23 July 2026, 23:59 | Source, dependencies, runtime data, and illustrated HTML/PDF report |
| 24 July 2026 | Screening, making-of, and five-minute talk per team member |

## Claimed Feature Scope

| Owner | Feature | Category | Points |
| --- | --- | ---: | ---: |
| Julian | Cinematic Engine | 4 | 50 |
| Julian | Rigging and blending contribution | 0 | 25 |
| Faouzi | Rigging and blending contribution | 0 | 25 |
| Julian | Omnidirectional point-light shadow mapping | 2 | 30 |
| Faouzi | Sound | 0 	| 20 |
| Faouzi | Motionblur | 1 	| 20 |
| Faouzi | Film grain | 1 	| 10 |
| Faouzi | Procedural Generation | 0 | 30 |


Julian's current scope is 105 points across categories 0, 2, and 4. No
additional points are claimed for the editor, PBR materials, panorama, HDR
framebuffer, sun-shadow support, tests, or MPEG4 delivery infrastructure.
The small PCF kernel is not claimed as the separate 80-point filtered-shadow
feature. 

## Assessment Evidence

The report contains architecture diagrams, debug screenshots, performance data, a concept-to-result comparison, resolved technical problems, and individual reflections. The presentation demonstrates each feature separately and then within the complete film.

## Rigging and Blending Evidence

Current engine support:

- imports GLB skins, joints, inverse bind matrices, joint indices, and weights;
- validates normalized weights, finite joint matrices, and sampled skinned
  bounds through CTest;
- samples translation, rotation, and scale channels;
- keeps rigged entities in bind pose until the Timeline starts playback;
- blends two sampled poses when at least two clips are available;
- uploads joint indices and weights with mesh vertices;
- renders skinned meshes with a 128-joint matrix palette.

Current samurai asset evidence:

- `samurai.glb` imports as 84 primitives, 126,600 vertices, 1 skin, 49 joints,
  and two authored actions: `ReadyIdle` and `ArmAction`;
- `asset_fidelity` validates the Samurai's complete rigged source data;
- `animation_playback_samurai` proves `ArmAction` changes joint matrices over
  time;
- the Timeline can play both clips and cross-fade between them.

## Cinematic and Shadow Evidence

- World Edit and Movie share one scene without Movie mutating base transforms.
- FilmTimeline provides camera cuts, stable animation clips, typed property
  lanes, cubic movement timing, shot preview, and deterministic 2160p30 output.
- At most two point lights render six depth faces each; selection is
  deterministic and additional point lights remain illuminated.
- Alpha-masked, double-sided, and off-camera casters participate in shadows.

## Sources

- [Official project brief](../assets/reference/cgintro-animation-project-info.pdf)
- [Official assessment sheet](../assets/reference/cgintro-bewertungsbogen.pdf)
