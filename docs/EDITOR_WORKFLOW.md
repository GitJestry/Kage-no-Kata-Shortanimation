# Editor Workflow

Every launch starts in **World Edit** with **Solid** shading. The centered
workspace switch enters Movie Edit; viewport shading controls remain available
at the top right.

## Navigation

- Right-drag looks around.
- `W`, `A`, `S`, `D` move horizontally.
- `Space` moves up; `Shift` moves down.
- Mouse wheel adjusts fly speed within its validated range.
- Single-click selects.
- Double-click selects and frames once.
- `Frame World` frames all visible model bounds.
- `Z` cycles Bounds, Solid, Material, and Final.

## World Edit

The left Editor panel contains:

- scene selection and scene operations;
- Save/Reload controls;
- entity hierarchy;
- Add Model, Camera, and Point Light;
- World, Sun, environment, and viewport settings.

The Inspector edits the selected entity:

- name and transform;
- mesh visibility and asset diagnostics;
- camera FOV/near/far;
- Point-light enabled, shadow, color, intensity, and range.

With no entity selected, it exposes the editor camera and fly speed.

![](/docs/images/world-editor.png)

Placement, transform gizmos, and entity deletion exist only in World Edit.
Delete Selected, the Delete key, and the hierarchy context menu use one
confirmation flow. Movie preview never changes these authored transforms.

Environment settings select a catalog panorama or import `.hdr`, `.png`, `.jpg`,
or `.jpeg`. Decode occurs on a worker, upload on the main thread, and state is
shown inline.

## Viewport modes

| Mode | Purpose |
| --- | --- |
| Bounds | fast bounds, selection, and loading diagnostics |
| Solid | complete authored geometry without material texturing |
| Material | materials, environment, lighting, and broad Sun shadow |
| Final | final materials, tighter 4096² Sun shadow, and selected Point shadows |

Editor-only grids, gizmos, paths, selection bounds, and diagnostics are never
written to final film output.

## Movie Edit

Movie Edit replaces the World hierarchy and inspector with:

- **Animation Targets** — rigged entities, cameras, Point lights, and Sun;
- **Movie Inspector** — selected target, sequence, clip, instance, diagnostics,
  and Bake;
- **Target Sequence Timeline** — local clips for one target;
- **Movie Timeline** — complete reusable sequence instances.

World placement, deletion, and transform gizmos are disabled. Movie selection is
independent of World selection. Non-camera sequence preview uses the editor
camera; camera sequence preview uses its evaluated film camera.

![](/docs/images/movie-editor.png)

## Diagnostics

Runtime diagnostics expose frame timing, GPU timing, draw/triangle counts,
visibility, shadow reuse, texture-memory estimates, and streaming work. 
