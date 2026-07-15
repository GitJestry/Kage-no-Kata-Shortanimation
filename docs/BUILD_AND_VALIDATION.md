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
| `film_persistence` | Film-v2 round trip, evaluated equivalence, captured-state independence, World non-mutation, and lighting extraction |
| `movie_workflow` | Movie target classification, selection, preview/playback state, atomic Film Camera creation, viewport behavior, and Bake validation |

The tests are headless system checks. They deliberately do not start the full
OpenGL editor or load the complete submitted asset set, because those checks are
slow, platform-dependent, and better verified through the manual workflow below.