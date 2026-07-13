# Editor Workflow

Every launch starts in **World Edit** with **Solid** shading. Use the centered
top switch to enter Movie; shading controls remain on the right.

## Navigation

- Right-drag looks around; `WASD` moves, `Space` moves up, and `Shift` moves
  down. Fly speed is available in the Editor Camera inspector, and the wheel
  adjusts it within its validated range.
- Single-click selects. Double-click selects and frames once.
- `Frame World` frames all visible models.
- `Z` cycles Bounds, Solid, Material, and Final. Every mesh mode uses original
  geometry and source-resolution textures.

## World Edit

The left Editor is fixed to the screen edge and fills the space above the status
strip. Its right edge can be resized. It contains the scene selector, Save,
hierarchy, one collapsed Add section, and collapsed World settings. Model,
camera, and point-light placement exists only here. Delete Selected, the Delete
key, and the hierarchy context menu share the same confirmation flow. Closing
it leaves a fixed Editor button at the top-left to reopen it.

The Inspector is fixed above the status strip in the bottom-right corner. With
no selection it exposes the Editor Camera, including fly speed and navigation
instructions; with an entity selected it edits visibility, transform, mesh
diagnostics, camera properties, and light properties. Per-entity opacity does
not exist. Closing it leaves a fixed Inspector button above the status strip to
reopen it.

World environment settings select a catalogued panorama or import `.hdr`,
`.png`, `.jpg`, or `.jpeg`. Decode occurs on a worker, upload on the main thread,
and Loading/Error/Ready is shown inline. Invalid assets use the fallback sky.

## Movie

Movie has no hierarchy, World Inspector, placement, deletion, or world gizmos.
The editor camera remains available for navigation and selection. Shot Preview
is explicitly enabled in FilmEditor and is non-interactive.

The bottom strip contains only the bottom-right Diagnostics button. Runtime
Diagnostics contains timing, draw/triangle, culling, texture-memory, and
streaming data.
