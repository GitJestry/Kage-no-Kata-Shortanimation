# Development Log

This log connects implementation changes to visible, integrated project
results. Add one entry when a milestone is completed or when a platform finding
changes the implementation contract.

## Entry Template

### YYYY-MM-DD: Milestone name

- Commit:
- Responsible developer:
- Configuration and compiler:
- Integrated result:
- Verification procedure:
- Evidence and metrics:
- macOS status:
- Windows status:
- Known limitations:
- Next integrated step:

## Milestones

The authoritative weekly milestones and acceptance criteria live in
[TODO.md](TODO.md). This document records evidence and findings after those
tasks are attempted or completed.

## Entries

### 2026-07-02: KageEngine skinning, lighting, and editor persistence pass

- Commit: Pending
- Responsible developer: Julian Meyer
- Configuration and compiler: CMake 4.2.1, C++23, AppleClang 21.0.0,
  arm64 macOS, Debug
- Integrated result: The application is named KageEngine. The editor stores
  tracked film-world data in `projects/kage_no_kata_world.kage.json` and local
  scratch scenes in `.kage_local/editor_session.json`. GLB library entries are
  registered by path and loaded lazily when the scene or editor needs them.
  Skinned mesh attributes, clip playback, pose blending support, and
  per-primitive joint matrix palettes are integrated into the existing mesh
  path. Lighting uses a single entity `LightComponent` for sun and point lights.
- Verification procedure: Configured and built `build/kage_engine`, ran
  `ctest --test-dir build --output-on-failure`, and checked the rig import path.
- Evidence and metrics: `asset_import_samurai_rig` passed, including finite CPU
  sampled skinned bounds. Direct import check reported `samurai.glb`: 84
  primitives, 126,600 vertices, 1 skin, 49 joints, and 1 animation.
- macOS status: Build, static GLB imports, samurai rig validation, shared
  project file loading, local scratch-scene persistence, lazy samurai asset
  loading, Dark Studio default, sun-light default, and CTest pass.
- Windows status: Configure, build, runtime launch, and LFS hydration remain
  pending on a Windows machine.
- Known limitations: The current samurai GLB has one clip, so it proves rigging,
  skinning, and clip playback but not final authored clip-to-clip blending. IK,
  strike planning, bamboo physics, particles, audio, Windows verification, and
  final film camera timing remain future tasks.
- Next integrated step: Export a two-action samurai GLB and demonstrate a real
  cross-fade through the Animation inspector.

### 2026-06-24: GLB runtime import and scene foundation

- Commit: `9c47682`, `86fa392`, `fff8f71`, `f27e31c`, `19703cf`,
  `508508c`
- Responsible developer: Julian Meyer
- Configuration and compiler: CMake 4.2.1, C++23, AppleClang 21.0.0
  (`clang-2100.1.1.101`), arm64 macOS, Debug and Release
- Integrated result: The runtime loads `sword.glb` and `torii_gate.glb` from
  copied executable-relative assets, imports GLB document data for nodes, static
  primitives, materials, textures, skins, inverse bind matrices, animation
  channels, and Blender marker nodes, then creates selectable scene entities
  rendered through project-owned OpenGL RAII objects.
- Verification procedure: Configured and built Debug and Release in `build`,
  launched the executable from its build output directory, confirmed copied
  model assets, verified the app-owned render loop delegates to the runtime
  scene manager, world, camera, lighting, and editor interface, and searched
  source code for forbidden DSA calls with
  `rg -n "glNamed|glVertexArray|glProgramUniform" src`.
- Evidence and metrics:

  | Asset | Scene | Meshes | Primitives | Vertices | Indices | Triangles | Materials | Bounds size |
  | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
  | `sword.glb` | `Scene` | 9 | 9 | 4,356 | 12,108 | 4,036 | 6 | 0.076 x 0.078 x 1.163 |
  | `torii_gate.glb` | `Scene` | 6 | 6 | 1,244 | 2,232 | 744 | 4 | 11.044 x 7.538 x 1.354 |

  Runtime startup reported Apple M4, OpenGL 4.1 Metal 90.5, and GLSL 4.10.
- macOS status: Configure, Debug build, Release build, executable-relative
  asset copy, GLB document import, static mesh upload, scene entity creation,
  staged entity placement, scene selection, quaternion fly/orbit camera control,
  mouse-look navigation, left-panel editor controls, floor/grid visualization,
  entity light controls, material texture rendering, lighting uniforms, and
  Release startup pass.
- Windows status: Configure, build, and runtime verification remain pending on
  a Windows machine.
- Known limitations: This milestone imports rigging and animation document data,
  but bind-pose evaluation, clip playback, GPU skinning, cross-fades, IK, and the
  two-action character asset remain separate Week 1 tasks.
- Samurai branch finding: `origin/test-samurai-rig` contains Git LFS pointer
  files for `samurai.glb` and `samurai.blend`; the actual binary payload is not
  available until Git LFS is installed and the files are hydrated.
- Next integrated step: Evaluate imported node hierarchies into skeleton poses,
  upload skinning matrices, and render the two-action character GLB in bind pose
  and one sampled clip.

### 2026-06-22: Portable application baseline

- Commit: Pending
- Responsible developer: Julian Meyer
- Configuration and compiler: CMake 4.2.1, C++23, AppleClang 21.0.0,
  arm64 macOS, Debug and Release
- Integrated result: The framework-backed application opens a 1280 x 720
  window, runs a VSync render loop, clears the framebuffer, displays runtime
  diagnostics through ImGui, and handles Escape as a close request.
- Verification procedure: Configured fresh Debug and Release build trees,
  downloaded every declared dependency with SHA-256 verification, built both
  configurations, and ran the Debug executable for a sustained smoke test.
- Evidence and metrics: Both builds completed without compiler diagnostics. The
  runtime reported Apple M4, OpenGL 4.1 Metal 90.5, and GLSL 4.10.
- macOS status: Configure, Debug build, Release build, context creation, and
  render-loop smoke test pass.
- Windows status: Configure, build, and runtime verification remain pending.
- Known limitations: The baseline renders only a clear frame and diagnostics;
  Windows runtime evidence requires the target machine.
- Next integrated step: Export the two-action GLB test character and define its
  import validation data before implementing static vertex loading.

### 2026-06-21: Engineering standards baseline

- Commit: Pending
- Responsible developer: Julian Meyer
- Configuration and compiler: Repository-level configuration; runtime build not
  changed
- Integrated result: Code style, OpenGL 4.1 compatibility rules, portable CMake
  defaults, runtime asset packaging, and the two-platform verification matrix
  now form one documented development contract.
- Verification procedure: Checked changed files for whitespace errors, reviewed
  documentation links, and inspected platform branches and target-local compiler
  options.
- Evidence and metrics: Three engineering documents, shared formatter and editor
  automation, and portable build/package rules.
- macOS status: Static checks pass. Configure/build verification remains pending
  because the installed CMake 4.2.1 executable hangs before producing output,
  including for `cmake --version`.
- Windows status: Pending first Windows configure/build.
- Known limitations: No OpenGL runtime milestone exists yet; `clang-format` is
  not installed on the current macOS development machine.
- Next integrated step: Configure the framework, open an OpenGL 4.1 window, and
  record the first Week 1 runtime milestone on both target platforms.
