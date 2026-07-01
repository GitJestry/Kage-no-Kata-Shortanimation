# Editor Workflow

## Shared Project State

```text
projects/kage_no_kata_world.kage.json
```

Tracked. Save it after changes to scenes, entities, transforms, lights, sky,
floor settings, or material debug mode.

```text
.kage_local/editor_session.json
.kage_local/imgui.ini
```

Ignored. Stores selection, camera speed, panel layout, and local test scenes.

## Asset Handoff

Runtime GLB files: `assets/models/`

Blender sources: `assets/source_blender/`

Large assets use Git LFS:

```bash
git lfs install
git lfs pull
```

Import checks:

```bash
ctest --test-dir build --output-on-failure
```

The samurai test requires skin data, joints, normalized weights, inverse bind
matrices, and one animation clip. A full rigging-and-blending demonstration
requires at least two exported clips so KageEngine can cross-fade between them.

## Editor Controls

- Right mouse drag: look around with the editor camera.
- `W`, `A`, `S`, `D`: fly camera movement.
- `Space` / `Shift`: move camera up/down.
- Mouse wheel: adjust fly speed.
- Asset/library selection: arms placement without creating an entity id.
- Left click in the viewport: place the pending entity.
- `Esc`: cancel placement or active gizmo operation.
- Outliner single click: select entity.
- Outliner double click: frame selected entity.
- `+ Local Test Scene`: create an ignored scratch scene for experiments.

Selection is local state.

## Save Model

Shared scenes are written to `projects/kage_no_kata_world.kage.json` by
`Save Project`.

Local test scenes are written to `.kage_local/editor_session.json`
automatically. They are useful for testing placement, lighting, and animation
without committing experimental world changes.

## Collaboration

1. Pull branch and LFS assets.
2. Open the editor from the project root with `build/kage_engine`.
3. Edit the world.
4. Press `Save Project`.
5. Review `projects/kage_no_kata_world.kage.json`.
6. Run build/tests before pushing.
