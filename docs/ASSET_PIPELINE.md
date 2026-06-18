# Blender and Asset Pipeline

## Handoff

Blender authors geometry, UVs, materials, armatures, skin weights, and animation clips. The C++ runtime creates GPU resources, evaluates skeletons, blends poses, and drives simulation.

- `assets/source_blender/`: `.blend` source files.
- `assets/models/`: runtime `.glb` exports.
- `assets/textures/`: external runtime textures.
- `assets/audio/`: ambience and event samples.
- `assets/reference/`: concepts, storyboard, and course material.

GLB/glTF 2.0 carries meshes, materials, textures, node hierarchies, skins, and named animations.

## Shared Conventions

- Blender unit scale: `1.0`, measured in metres.
- Character reference height: approximately `1.75 m`.
- Axis conversion: Blender glTF export with Y Up.
- File names: lowercase with underscores.
- Skinning: four normalized bone weights per vertex.
- Animation sampling: 30 FPS.
- Materials: Principled BSDF values and image textures.

Use `glTF Binary (.glb)`, animation mode `Actions`, enabled skinning, four bone influences, and deformation bones only.

## Character Actions

| Action | Purpose |
| --- | --- |
| `Idle` | Resting pose |
| `LookAtPicture` | Family portrait beat |
| `Kneel` | Move into training position |
| `Draw` | Draw the sword |
| `StrikeLeft` | Left diagonal strike |
| `StrikeHorizontal` | Horizontal strike |
| `StrikeRight` | Right diagonal strike |
| `Recover` | Controlled final pose |

The three strike clips share duration, starting pose, and impact time. This alignment supports runtime angle blending. Character, clothing, and yellow ribbon use one armature.

## Environment Assets

The bamboo export contains named rigid segments and joint points. Runtime physics assigns mass, inertia, and constraints. The hut, gate, sword, and family portrait load as static GLB assets. C++ generates terrain and places vegetation from height, slope, and seed.

## Export Check

- Scale and axes match the reference character.
- Meshes contain UVs, normals, and complete material assignments.
- Skin weights sum to one and use up to four joints.
- GLB animation names and time ranges match the action table.
- Strike impact markers share one normalized time.
- Texture and model licenses are recorded.
