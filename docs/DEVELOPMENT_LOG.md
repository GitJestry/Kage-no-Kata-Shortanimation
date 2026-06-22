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
