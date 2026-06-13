# Asset Pipeline

## Source Assets

Blender source files belong in `assets/source_blender/`. Runtime exports belong in `assets/models/`, `assets/textures/`, and related folders.

Recommended naming:

- `character_shinobi.blend`
- `character_shinobi_rigged.blend`
- `prop_bamboo_segmented.blend`
- `prop_sword.blend`
- `environment_hut.blend`
- `environment_mountains.blend`

## Export Guidelines

- Use meters or a documented shared unit scale.
- Apply transforms before export when the asset is final.
- Keep forward/up axes consistent across all exports.
- Use separate materials for surfaces that need distinct shader treatment.
- Keep collision or cut-helper geometry clearly named.
- Store texture sources separately from compressed runtime textures if compression is added later.

## Bamboo Requirements

The bamboo model should expose cut-relevant structure:

- Segment boundaries.
- Preferred break zones.
- Optional helper markers for target height.
- Separate pieces if deterministic fracture is used.

## Character Requirements

The character should prioritize a small set of expressive poses over a large unfinished rig:

- Standing near hut.
- Looking at family picture.
- Kneeling.
- Sword draw.
- Wind-up.
- Strike.
- End pose.

The yellow ribbon on the sword sheath should remain visible in close-up framing.

## Runtime Policy

Keep large source files out of build output. The application should load only exported runtime assets.
