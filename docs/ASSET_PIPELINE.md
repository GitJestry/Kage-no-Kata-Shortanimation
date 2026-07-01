# Blender and Asset Pipeline

## Handoff

Blender authors geometry, UVs, materials, armatures, skin weights, and animation clips. The C++ runtime creates GPU resources, evaluates skeletons, blends poses, and drives simulation.

- `assets/source_blender/`: `.blend` source files.
- `assets/models/`: runtime `.glb` exports.
- `assets/textures/`: external runtime textures.
- `assets/audio/`: ambience and event samples.
- `assets/reference/`: concepts, storyboard, and course material.

Large `.glb` and `.blend` files are handled through Git LFS. Always run
`git lfs pull` after pulling a branch that changes models or Blender sources.

GLB/glTF 2.0 carries meshes, materials, textures, node hierarchies, skins,
inverse bind matrices, named animations, and named marker nodes exported from
Blender empties.

## Shared Conventions

- Blender unit scale: `1.0`, measured in metres.
- Character reference height: approximately `1.75 m`.
- Axis conversion: Blender glTF export with Y Up.
- File names: lowercase with underscores.
- Skinning: four normalized bone weights per vertex.
- Animation sampling: 30 FPS.
- Materials: Principled BSDF values and image textures.

Use `glTF Binary (.glb)`, animation mode `Actions`, enabled skinning, four bone
influences, and deformation bones only. Constraint or attachment points should
be authored as named empties or marker nodes outside the deforming joint set so
the importer can expose them as runtime markers.

## Character Actions

| Action | Purpose |
| --- | --- |
| `Idle` | Resting pose |
| `LookAtPicture` | Family portrait beat |
| `Kneel` | Move into training position |
| `Draw` | Draw the sword |
| `Ready` | Stable starting and ending pose for procedural strikes |

The runtime generates each strike and recovery from the requested cut using
parametric sword motion and IK. Character, clothing, and yellow ribbon use one
armature.

## Environment Assets

The bamboo export contains named rigid segments and joint points. Runtime
physics assigns mass, inertia, and constraints from imported marker and node
data. The hut, gate, sword, and family portrait load as static GLB assets. C++
generates terrain and places vegetation from height, slope, and seed.

## Export Check

- Scale and axes match the reference character.
- Meshes contain UVs, normals, and complete material assignments.
- Skin weights sum to one and use up to four joints.
- GLB animation names and time ranges match the action table.
- Constraint, attachment, and bamboo joint markers use stable descriptive names.
- The `Ready` pose keeps both hands on the hilt and both feet planted.
- Texture and model licenses are recorded.

## Shared World File

Imported assets become editable entities inside
`projects/kage_no_kata_world.kage.json`. Save this file through the editor when
world placement, scene names, transforms, lights, sky, or floor settings belong
in the shared branch. `.kage_local/` remains machine-local.
