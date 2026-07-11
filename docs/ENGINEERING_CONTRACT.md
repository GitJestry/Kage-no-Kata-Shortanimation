# KageEngine Engineering Contract

KageEngine is the engine foundation for the film, not a preview app.

## Rules

- UI sends commands. It does not own runtime behavior or mutate scene records
  directly.
- Systems own behavior: animation samples clips, lighting builds lights, render
  owns GPU resources, scene owns entities, assets own parsed source data.
- Assets register by label and path. GLB loading and GPU upload happen lazily
  when a scene or explicit editor action needs the asset.
- Hot paths avoid allocation and repeated linear searches.
- Implemented features and planned features are documented separately.
- Production paths do not contain one-off test scaffolding.
- Large files are stored through Git LFS.
- Generated cache data belongs in `.kage_cache/` and is never source of truth.

## Review Checklist

- Does this code serve the film engine, or only a narrow editor shortcut?
- Is ownership clear from the namespace and class name?
- Can another system reuse this without depending on ImGui or OpenGL?
- Is the expensive work done once, cached, and measured?
- Does the documentation state the current truth without overclaiming?
