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

Ignored. Stores selection, camera speed, and panel layout.

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
matrices, and one animation clip.

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

Selection is local state.

## Collaboration

1. Pull branch and LFS assets.
2. Open the editor from the project root with `build/kage_no_kata`.
3. Edit the world.
4. Press `Save Project`.
5. Review `projects/kage_no_kata_world.kage.json`.
6. Run build/tests before pushing.
