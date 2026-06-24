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

### 2026-06-24: Static GLB import and rendering

- Commit: `9c47682`, `86fa392`, `fff8f71`, `f27e31c`, `19703cf`,
  `508508c`
- Responsible developer: Julian Meyer
- Configuration and compiler: CMake 4.2.1, C++23, AppleClang 21.0.0
  (`clang-2100.1.1.101`), arm64 macOS, Debug and Release
- Integrated result: The runtime loads `sword.glb` and `torii_gate.glb` from
  copied executable-relative assets, reports import diagnostics in ImGui,
  uploads static primitives through project-owned OpenGL RAII objects, and
  renders a selectable untextured GLB mesh with depth testing.
- Verification procedure: Configured and built Debug in `build-week1-debug`,
  configured and built Release in `build-week1-release-patched`, launched the
  Release executable from its build output directory, confirmed copied model
  assets, and searched source code for forbidden DSA calls with
  `rg -n "glNamed|glVertexArray|glProgramUniform" src`.
- Evidence and metrics:

  | Asset | Scene | Meshes | Primitives | Vertices | Indices | Triangles | Materials | Bounds size |
  | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
  | `sword.glb` | `Scene` | 9 | 9 | 4,356 | 12,108 | 4,036 | 6 | 0.076 x 0.078 x 1.163 |
  | `torii_gate.glb` | `Scene` | 6 | 6 | 1,244 | 2,232 | 744 | 4 | 11.044 x 7.538 x 1.354 |

  Runtime startup reported Apple M4, OpenGL 4.1 Metal 90.5, and GLSL 4.10.
- macOS status: Configure, Debug build, Release build, executable-relative
  asset copy, GLB import, static mesh upload, and Release startup pass.
- Windows status: Configure, build, and runtime verification remain pending on
  a Windows machine.
- Known limitations: This milestone covers static GLB rendering. Texture
  sampling, skinned vertices, inverse bind matrices, animation clips, and the
  two-action character asset remain separate Week 1 tasks.
- Next integrated step: Extend the same loader and render path to the
  two-action character GLB once the asset is available, starting with bind-pose
  skeleton and inverse bind matrix validation.

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
