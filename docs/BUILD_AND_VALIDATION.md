# Build and Validation

## 1. Supported submission environment

The project targets C++23 and desktop OpenGL. The maintained submission
platforms are macOS and Windows.

Required tools:

- CMake 3.26 or newer;
- a C++23 compiler;
- Git LFS for runtime assets;
- an OpenGL-capable system supported by the course framework;
- `ffmpeg` on `PATH` for MPEG4 encoding.

The first CMake configure downloads pinned source archives for the framework,
TinyGLTF, and meshoptimizer. For an offline grading package, include an approved
dependency cache or the downloaded dependency sources in addition to the
repository.

## 2. Retrieve runtime data

```bash
git lfs install
git lfs pull
```

Verify that `assets/`, `projects/kage_no_kata_assets.kage.json`, and
`projects/kage_no_kata_world.kage.json` are present before launching.

## 3. Configure and build

### macOS or single-configuration generator

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON
cmake --build build --parallel
```

Run:

```bash
./build/kage_engine
```

### Windows with a multi-configuration generator

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
.\build\Release\kage_engine.exe
```

## 4. Automated checks

Run the complete retained suite:

```bash
ctest --test-dir build --output-on-failure
```

| Test | Boundary protected |
| --- | --- |
| `film_domain` | frame ranges, sequence reuse, movement paths and transitions, rig playback, typed properties, gaps, validation, and atomic edits |
| `film_persistence` | Film round trip, evaluated equivalence, captured-state independence, World non-mutation, and lighting extraction |
| `movie_workflow` | Movie target classification, selection, preview/playback state, atomic Film Camera creation, viewport behavior, and Bake validation |

The tests are headless system checks. OpenGL editor startup and rendering are
enabled separately through the CI presets below.

## 5. CI-equivalent local presets

The presets use warnings-as-errors and keep their build trees separate:

```bash
cmake --preset ci-macos
cmake --build --preset ci-macos
ctest --preset ci-macos
```

Linux CI uses Mesa as a deterministic OpenGL test harness, not as a supported
release platform:

```bash
cmake --preset ci-linux
cmake --build --preset ci-linux
xvfb-run -a ctest --preset ci-linux
```

`ci-linux` requires Git LFS objects, Clang, Xvfb, Mesa, and ffmpeg.
`ci-sanitize` runs the deterministic headless tests under ASan/UBSan.

## 6. Complete regression matrix

| Check | Boundary protected |
| --- | --- |
| Existing Film checks | Film evaluation, edits, persistence, viewport selection, Movie workflow, and Bake validation |
| `math_camera` | transforms, bounds, curves, projection, camera navigation, picking, visibility, and gizmo sizing |
| `scene_editor` | entity and scene ownership plus editor layout and gizmo state |
| `asset_pipeline` | paths, catalog round-trip, stable IDs, registry states, GLTF import, LFS rejection, and worker success/failure |
| `animation_lighting` | bind/sample/blend palettes, Film animation directives, light overrides, and shadow ranking |
| `runtime_paths` | executable-relative project, asset, catalog, and World discovery on every CI platform |
| `render_smoke` | OpenGL 4.1 context, HDR/MSAA framebuffers, tone-mapping probes, 2160p30 PNG/ffmpeg export, and manifest |
| `application_smoke` | application startup, editor rendering, ImGui construction, OpenGL error handling, and shutdown |

GitHub Actions runs `quality`, `linux-regression`, `sanitizers`,
`platform-build (macos-intel)`, and `platform-build (windows-x64)` on every pull
request and push to `main`. These are the required branch-protection checks.
CTest XML, render PNGs, manifests, and the smoke-test MPEG4 are uploaded when
relevant.

## 7. Remaining human acceptance

Automation verifies behavior and deterministic render probes. Before a release,
still inspect the complete film for composition, animation/deformation quality,
exact shadow appearance, audio, platform-specific appearance, and hands-on
editor usability on the target macOS and Windows machines.
