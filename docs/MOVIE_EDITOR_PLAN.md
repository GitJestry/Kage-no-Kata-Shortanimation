# Final Movie Editor Plan

## 1. Scope

Complete only the Movie Editor required for the university film project.

Do not add unrelated editor features, new feature claims, speculative systems,
general-purpose NLE functionality, or production dependencies.

| Target | Authorable lanes |
|---|---|
| Rigged entity | Movement, Animation |
| Camera | Movement, FOV |
| Point light | Movement, Intensity, Color |
| Sun | Direction, Intensity, Color |

Terminology:

- `TargetSequence`: reusable, target-specific local animation beginning at frame 0.
- `SequenceInstance`: placement of a `TargetSequence` on the final movie timeline.

Legacy `LightEnabled` and `LightRange` data remain loadable and evaluable, but
cannot be newly authored.

---

## 2. Invariants

- Film time uses integer frames at 30 fps.
- All ranges are half-open: `[start_frame, end_frame)`.
- Maximum exclusive film end: frame `3600`.
- World Edit owns persistent authored World state.
- Movie evaluation never mutates World transforms, cameras, lights, rigs, or sun.
- Stopping preview only stops consuming `FilmFrameState`; no restoration pass is needed.
- `FilmFrameState` remains the runtime boundary.
- ImGui contains no timeline invariants or editing algorithms.
- Existing projects remain loadable.
- Existing animation blending remains supported internally, but Movie clips do not auto-blend.
- After migration, only one runtime film model exists.
- Zero-duration movies are valid editor states; Play and Bake are disabled until
  at least one valid instance exists.

---

## 3. Core types

```cpp
using FilmFrame = std::int32_t;
using TargetSequenceId = std::uint64_t;
using SequenceInstanceId = std::uint64_t;
using SequenceClipId = std::uint64_t;

inline constexpr FilmFrame FILM_FPS = 30;
inline constexpr FilmFrame MAX_FILM_FRAMES = 3600;
```

IDs are stable aliases, not new wrapper types. `SequenceClipId` values are
globally unique inside `MovieTimeline`; `MovieTimeline` owns `next_clip_id`.

```cpp
enum class TimelineTargetKind { Entity, Sun };

struct TimelineTarget {
    TimelineTargetKind kind = TimelineTargetKind::Entity;
    scene::EntityId entity;
};
```

Valid entity targets are rigged entities, cameras, and point lights. The sun is
explicit and never receives a fake entity ID.

### Captured base state

```cpp
struct CapturedCameraState {
    float vertical_fov_degrees;
    float near_plane;
    float far_plane;
};

struct CapturedPointLightState {
    bool enabled;
    glm::vec3 color;
    float intensity;
    float range;
    bool casts_shadows;
};

struct CapturedEntityBaseState {
    math::Transform transform;
    std::optional<CapturedCameraState> camera;
    std::optional<CapturedPointLightState> point_light;
};

struct CapturedSunBaseState {
    glm::vec3 direction_to_sun;
    glm::vec3 color;
    float intensity = 1.0f;
};

using CapturedTargetBaseState =
    std::variant<CapturedEntityBaseState, CapturedSunBaseState>;
```

A sequence captures its target state when created. Later World edits do not
change it. Only `recaptureBaseState` may replace it. Active sequence evaluation
does not read mutable World camera, transform, light, or sun values. Rigged
entities use bind pose before their first animation clip.

---

## 4. Movie model

```cpp
struct TargetSequence {
    TargetSequenceId id = 0;
    std::string name;
    TimelineTarget target;
    CapturedTargetBaseState captured_base;
    std::vector<SequenceClip> clips;

    [[nodiscard]] FilmFrame durationFrames() const;
};

struct SequenceInstance {
    SequenceInstanceId id = 0;
    TargetSequenceId sequence_id = 0;
    FilmFrame start_frame = 0;
};

enum class CameraGapMode { HoldLastCameraState, Black };

struct MovieTimeline {
    std::string name = "Kage no Kata";
    CameraGapMode camera_gap_mode = CameraGapMode::HoldLastCameraState;
    std::vector<TargetSequence> sequences;
    std::vector<SequenceInstance> instances;
    TargetSequenceId next_sequence_id = 1;
    SequenceInstanceId next_instance_id = 1;
    SequenceClipId next_clip_id = 1;

    [[nodiscard]] FilmFrame durationFrames() const;
};
```

Rules:

- A sequence belongs to one target, starts at local frame 0, and is reusable.
- Sequence duration is the furthest clip end; an empty sequence has duration 0
  and cannot be placed.
- An instance always uses the complete sequence. It has no trim, speed,
  time-scaling, or independent duration.
- Movie duration is the furthest `instance.start_frame + sequence.durationFrames()`.

---

## 5. Sequence clips

```cpp
struct SequenceClip {
    SequenceClipId id = 0;
    FilmFrame start_frame = 0;
    FilmFrame end_frame = 1;
    SequenceClipPayload payload;
};

using SequenceClipPayload = std::variant<
    MovementClip,
    RigAnimationClip,
    PropertyClip
>;
```

Different lanes may overlap. Clips on the same lane may not overlap.

### Movement

```cpp
enum class MovementStartMode { PreviousEndpoint, ExplicitPosition };

struct MovementCurve {
    glm::vec3 position_control_1;
    glm::vec3 position_control_2;
    float timing_control_1 = 1.0f / 3.0f;
    float timing_control_2 = 2.0f / 3.0f;
    bool automatic_position_controls = true;
};

struct MovementTransition {
    bool enabled = false;
    MovementCurve curve;
};

struct MovementClip {
    MovementStartMode start_mode = MovementStartMode::PreviousEndpoint;
    std::optional<math::Transform> explicit_start;
    math::Transform end;
    MovementCurve curve;
    MovementTransition transition_before;
};
```

- `PreviousEndpoint`: start derives from the previous movement endpoint, or from
  captured base state when no predecessor exists. Continuity propagates forward.
- `ExplicitPosition`: preserves its authored start. With transition enabled, the
  preceding gap becomes a violet transition; otherwise the previous state is
  held until an intentional jump.
- A transition belongs to the following movement clip and derives its range from
  previous movement end to current movement start. It is never a top-level clip.
- Spline geometry and clip duration are authoritative. Derived average speed is
  display-only. Timing changes come from resizing or timing Bézier controls.

### Rig animation

```cpp
struct RigAnimationClip {
    assets::AnimationClipId clip_id = 0;
    std::size_t legacy_clip_index = 0;
    float source_in = 0.0f;
    float source_out = 1.0f;
    float speed = 1.0f;
    bool looping = false;
    float blend_in_seconds = 0.0f;
    float blend_out_seconds = 0.0f;
};
```

- Stable animation ID is preferred; legacy index remains a fallback.
- Trim is normalized in persistence and may be displayed in seconds.
- Speed changes sampling, not timeline length.
- Looping wraps inside the trimmed source range.
- Non-looping playback clamps to its final trimmed pose.
- Shorter bars cut early; longer looping bars repeat; longer non-looping bars hold.
- Gaps after a clip hold its final pose; before the first clip, use bind pose.
- Adjacent clips switch immediately. Blend fields remain supported and default to 0.

### Properties

```cpp
enum class PropertyKind {
    CameraFov,
    PointLightIntensity,
    PointLightColor,
    SunDirection,
    SunIntensity,
    SunColor,

    // Loader/runtime compatibility only; not authorable.
    LegacyPointLightEnabled,
    LegacyPointLightRange
};
```

---

## 6. Evaluation

Evaluation is:

```text
TargetSequence local evaluation -> MovieTimeline master composition -> FilmFrameState
```

Private film-module helpers perform local evaluation and compose directly into
`FilmFrameState`; no new public intermediate result type is required.

### Local sequence

Start from `captured_base`.

For every lane:

- before the first clip: captured base;
- during a clip: evaluated clip value;
- in gaps or after the last clip: hold the previous final value.

Holds are evaluator behavior and are not stored as synthetic clips. Rig lanes
use bind pose before their first clip and a frozen final-sample directive after
completed clips.

### Master timeline

For each target with instances:

- before the first instance: first sequence's captured base;
- during an instance: evaluate at local time;
- between and after instances: hold the preceding final state;
- when a later instance starts, its captured base supplies properties that its
  sequence does not animate.

Targets without instances use normal World state.

Conflict rules:

- Different targets may overlap.
- Same non-camera target instances may not overlap.
- Camera overlap is allowed while editing, reported as a warning, and blocks Bake.
- Invalid camera-overlap preview resolves deterministically by greatest
  `start_frame`, then greatest `SequenceInstanceId`.

### Camera output

Keep existing transform, animation, and property override collections. Add only:

```cpp
enum class FilmOutputKind { Camera, Black };

struct EvaluatedCameraState {
    scene::EntityId source_entity;
    math::Transform transform;
    float vertical_fov_degrees;
    float near_plane;
    float far_plane;
};

struct FilmCameraOutput {
    FilmOutputKind kind = FilmOutputKind::Black;
    std::optional<EvaluatedCameraState> camera;
};
```

Add a dedicated sun override only if existing property overrides cannot represent
it cleanly.

Camera gaps:

- `HoldLastCameraState`: hold the last camera; output black before the first.
- `Black`: output black during every camera gap.

Both modes are valid and do not block Bake.

---

## 7. TimelineEditService

Create:

```text
src/film/timeline_edit_service.hpp
src/film/timeline_edit_service.cpp
```

Required behavior:

```text
create / duplicate / delete sequence
recapture base state
append / ripple-insert / move / trim / delete clip
set movement start mode / transition
place / duplicate / move / delete instance
validate authoring / validate for bake
```

Exact C++ signatures may follow repository error conventions. Binding behavior:

- all commands are atomic;
- return created IDs when applicable;
- camera overlaps commit with warnings;
- same-target non-camera overlaps are rejected;
- operations exceeding frame 3600 are rejected;
- `appendClipToLane` inserts after the furthest clip on that lane and returns the
  new clip ID;
- ripple insertion moves only later clips on the same lane and recomputes
  movement continuity and transitions;
- no edit algorithm belongs in ImGui.

---

## 8. Engine, lifecycle, and selection

### New Camera From View

Provide one atomic Engine command:

```cpp
CreateFilmCameraResult EngineCore::createFilmCameraFromView(FilmFrame start_frame);
```

It captures the editor camera, creates the World camera, camera sequence,
constant Movement and FOV clips, and instance. Failure rolls back both World and
movie data.

A genuinely new project with no film data receives exactly once:

- one World-owned film camera;
- one sequence and one instance at frame 0;
- constant Movement and FOV clips from frame 0 to 300.

Do not recreate it when entering Movie mode or loading existing film data.

### Orphans

Deleted target entities do not silently delete sequences or instances.

- Mark the sequence orphaned and show a warning.
- Skip non-camera orphan output during preview.
- Orphan camera output is black.
- Bake is blocked while an orphan sequence has instances.
- Required user action: delete the orphan sequence and its instances.
- Rebinding is out of scope.

### Movie selection

Store Movie selection separately from World Edit selection:

```cpp
struct MovieEditorSelection {
    std::optional<TimelineTarget> target;
    TargetSequenceId sequence_id = 0;
    SequenceClipId clip_id = 0;
    SequenceInstanceId instance_id = 0;
};
```

Movie mode allows selecting rigged entities, cameras, and point lights; static
non-rigged entities are ignored. Double-click frames the entity. Gizmos,
placement, transform editing, and Delete-key entity deletion are disabled.
Clicking empty viewport does not deselect. Only the Movie **Deselect** button
clears Movie selection.

---

## 9. UI

Split the current monolithic timeline UI into:

```text
src/editor/movie_editor_panel.cpp
src/editor/movie_target_list.cpp
src/editor/movie_inspector.cpp
src/editor/sequence_timeline_view.cpp
src/editor/master_timeline_view.cpp
src/editor/timeline_view_helpers.cpp
```

### Left: Animation Targets

Sections: Cameras, Rigged Entities, Point Lights, Sun.

Actions: New Camera From View, Deselect. No other Movie-mode entity creation.

### Right: Animation Inspector

Target view:

- captured base summary;
- sequence list;
- New, Duplicate, Delete Sequence;
- Recapture Start State From World;
- Save Project.

Clip view:

- movement and transition controls;
- animation trim, speed, loop;
- approved camera/light/sun properties.

Camera sequence: Preview Camera Sequence.

### Bottom panel

Modes: Movie Timeline and Target Sequence Timeline.

Shared behavior: vertically resizable upward, shared vertical playhead,
mouse-wheel zoom, horizontal scrolling, frame-exact drag and trim.

Toolbar contains only Play/Stop, Bake, current time, and zoom indicator. Save
belongs to the inspector or existing project controls.

Local timeline uses fixed target-dependent lanes with one Add button per lane.
New clips append to the lane end and become selected. Violet transitions select
their owning movement clip.

Master timeline has one row per target and supports reusable instances, drag,
duplicate, and delete. Camera overlaps show warnings and disable Bake.

---

## 10. Persistence and migration

Advance to the next project schema (expected v6) and film schema v2.

Persist movie targets, captured bases, sequences, clips, stable IDs, movement
start/transition settings, instances, camera gap mode, animation IDs and legacy
indexes, trim, speed, loop, and blend fields.

Do not persist editor-only selection, active view, zoom, scroll, or panel height.
World and Movie data remain in one project save operation.

### Loading order

Legacy film JSON may be parsed first, but World entities must be constructed
before migration finalizes captured base states. Missing targets receive safe
fallback data and remain orphaned; data is never dropped silently.

### Legacy conversion

Legacy DTOs and conversion helpers are loader-only, never a second runtime model.

Non-camera tracks:

1. create one sequence per track;
2. preserve clip IDs and payloads where possible;
3. preserve frame ranges;
4. create one instance at frame 0;
5. preserve gaps through hold evaluation.

Camera cuts `[cut_start, cut_end)`:

1. create one cut-specific camera sequence;
2. place its instance at `cut_start`;
3. convert intersecting Movement and FOV clips to cut-local ranges;
4. ensure sequence duration is `cut_end - cut_start`;
5. fill uncovered cut ranges with explicit constant camera Movement/FOV clips.

For clip/cut intersections:

```text
intersection_start = max(clip_start, cut_start)
intersection_end   = min(clip_end, cut_end)
local range = [intersection_start - cut_start,
               intersection_end - cut_start)
```

Clipped cubic movement, timing, and property curves must use exact De Casteljau
subdivision. Rotation and scale endpoints are sampled at the corresponding eased
boundaries. Do not approximate.

Create at least one legacy fixture and compare old and migrated evaluation at
every integer frame: camera, transform, FOV, animation identity/source time, and
property values. Save only the new schema.

---

## 11. Validation and tests

Authoring diagnostics:

- orphans;
- camera overlaps;
- missing or incompatible assets;
- invalid source ranges or timing curves;
- frame-limit violations.

Bake blockers:

- camera overlap;
- used orphan sequence;
- missing camera/animation assets;
- invalid source/frame ranges;
- movie end above 3600;
- no playable frames;
- no valid camera output.

Camera gaps are valid.

Tests:

```text
tests/film_sequence_check.cpp
tests/timeline_edit_service_check.cpp
tests/film_serializer_check.cpp
```

Required coverage:

- half-open ranges and frame-3600 limit;
- derived durations and sequence reuse;
- local-time reset per instance;
- captured-base independence and World non-mutation;
- lane and instance conflict rules;
- movement continuity, explicit starts, transitions, ripple insertion;
- animation trim, speed, loop, final-pose hold;
- property and sun holds;
- camera gap modes and overlap warnings;
- atomic edit failure;
- new-schema round trip and legacy frame-equivalence migration;
- stable IDs and legacy animation fallback;
- Movie selection filtering and explicit deselection;
- preview stop immediately returns to normal World rendering.

---

## 12. Acceptance scenario

Without scene-specific hardcoding:

1. Open Movie mode and select the Samurai.
2. Create a Samurai sequence.
3. Add a straight movement from the house to in front of the shrine.
4. Add `ArmAction`, set `0.5x`, disable loop, trim source end by `0.2s`.
5. Create/select a camera sequence and add a curved camera movement.
6. Place both sequences on the Movie Timeline.
7. Preview, save, reload, and verify IDs, transforms, timing, trim, speed,
   camera assignment, and curves.
8. Preview again, stop, and confirm World Edit transforms remain unchanged.
9. Bake only after validation succeeds.

Automate domain and persistence checks; document only unavoidable visual checks.

---

# Implementation milestones

## Milestone 1 — Isolated core model

Implement the new `MovieTimeline`, `TargetSequence`, `SequenceInstance`, local
and master evaluator, `TimelineEditService`, and pure unit tests.

The new model is not connected to EngineCore, serialization, rendering, export,
or ImGui. The existing `FilmTimeline` remains the active runtime model during
this milestone. This is an isolated tested library, not a second runtime
representation.

## Milestone 2 — Atomic runtime cutover

In one buildable cutover:

- make `MovieTimeline` the active EngineCore film model;
- connect `FilmFrameState` to rendering, animation, camera, and lighting;
- support holds, final-pose directives, sun/light output, and camera gap modes;
- update playback and export for derived and zero duration;
- parse legacy data through loader-only DTOs;
- migrate after World loading and write the new schema;
- preserve hidden legacy `LightEnabled` and `LightRange` behavior;
- add only the minimum compatibility UI bridge;
- remove the old runtime `FilmTimeline` model.

At completion, loading, saving, preview, export, and build must work.

## Milestone 3 — Movie Editor UI

Replace the compatibility UI with:

- target list and Movie selection policy;
- inspector and sequence management;
- local and master timelines;
- Add, drag, trim, duplicate, delete;
- violet transitions;
- camera preview, Save Project, vertical resize, and mouse-wheel zoom.

All mutations go through `TimelineEditService`.

## Milestone 4 — Bake and acceptance

Complete Bake validation, overlap/orphan warnings, the acceptance workflow,
migration regression tests, full regression suite, final review, and concise
architecture/workflow documentation. Do not add unrelated features.

---

## Agent execution rule

For each task, read `AGENTS.md`, `docs/DEVELOPER_CONTRACT.md`,
`docs/PROJECT_REQUIREMENTS.md`, and this plan. Implement exactly one requested
milestone, add no dependencies or unrelated refactors, run relevant tests, and
report only changed files, tests run, and remaining risks.
