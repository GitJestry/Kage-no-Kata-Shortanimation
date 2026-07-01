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

## Feature Plan

| Owner | Feature | Category | Points |
| --- | --- | ---: | ---: |
| Julian | Physical bamboo simulation | 0 | 50 |
| Julian | Interactive mouse cut | 4 | 20 |
| Julian | Sound | 4 | 20 |
| Faouzi | Procedural terrain | 0 | 30 |
| Faouzi | Particle system | 0 | 40 |
| Faouzi | Temporal motion blur | 1 | 20 |
| Faouzi | Film grain | 1 | 10 |
| Shared | Rigging and blending | 0 | 50 |

The project covers 240 points across categories 0, 1, and 4. Shared rigging contributes 25 points to each member, resulting in 115 points for Julian and 125 points for Faouzi.

## Assessment Evidence

The report contains architecture diagrams, debug screenshots, performance data, a concept-to-result comparison, resolved technical problems, and individual reflections. The presentation demonstrates each feature separately and then within the complete film.

## Rigging and Blending Evidence

Current engine support:

- imports GLB skins, joints, inverse bind matrices, joint indices, and weights;
- validates normalized weights and finite bind/sample matrices through CTest;
- samples translation, rotation, and scale channels;
- blends two sampled poses when at least two clips are available;
- uploads joint indices and weights with mesh vertices;
- renders skinned meshes with a 128-joint matrix palette.

Current samurai asset evidence:

- `samurai.glb` imports as 84 primitives, 126,600 vertices, 1 skin, 49 joints,
  and 1 animation clip;
- `asset_import_samurai_rig` validates the rig data and sampled joint matrices.

Open asset requirement:

- the current samurai export has one clip, so it proves rigging, skinning, and
  clip playback, but not authored clip-to-clip blending by itself;
- the final grading proof needs a samurai GLB with at least two named actions,
  for example `Idle` and `Ready` or `Draw`, so the inspector blend control can
  demonstrate a real cross-fade.

## Sources

- [Official project brief](../assets/reference/cgintro-animation-project-info.pdf)
- [Official assessment sheet](../assets/reference/cgintro-bewertungsbogen.pdf)
