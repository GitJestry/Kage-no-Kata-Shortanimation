# Editor Workflow

## Navigation

- Right-drag: look around.
- `WASD`: move.
- `Space` / `Shift`: move up/down.
- Mouse wheel: camera speed.
- Double-click an entity in the hierarchy: frame it.
- `Z`: cycle Bounds, Solid, Material, and Final viewport modes.

The compact viewport strip offers the same four modes. Material is the default;
Solid forces the lowest generated LOD and skips material texture sampling,
while Final keeps full geometry detail.

## World Editing

- Select an asset or `Light Source` in the Create list to start placement.
- Move the ghost over the floor and left-click the viewport to place it.
- `Esc` cancels placement or active gizmo work.
- Select entities from the hierarchy or by double-clicking mesh geometry.
- Delete from the hierarchy, Inspector, or Delete key; all paths use the same
  confirmation dialog.

## Animation

Select a rigged entity and open Timeline.

- `Play` starts looping playback.
- `Stop` resets the clip to the first frame.
- `Speed` changes playback rate.
- Imported compatible animation clips appear beside embedded model clips.

## Lighting

The Lighting panel separates:

- `Sun`: directional light controlled by direction, color, and intensity.
- `Environment`: ambient diffuse/specular and exposure.
- `Light Source`: placeable point light entities.

Dark Void starts with no ambient contribution. Shadows are not implemented yet.
