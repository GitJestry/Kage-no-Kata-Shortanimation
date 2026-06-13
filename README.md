# Kage no Kata - Der letzte Schnitt

University of Bonn computer graphics animation project by Julian Meyer and Faouzi Homsani.

`Kage no Kata - Der letzte Schnitt` is a short interactive CG film about the last heir of a Shinobi sword school. The scene begins in a quiet mountain landscape: a small warm hut, green terrain, distant snowy peaks, and a character carrying the weight of family memory. The interaction centers on a bamboo cutting exercise. User input controls the cut, and the result drives animation selection, bamboo response, particles, sound, and visual feedback.

![Storyboard sketch](assets/storyboard/storyboard-1.png)

## Technical Direction

The project is written in C++23 and built with CMake. Rendering and window/application setup use the university-provided OpenGL adapter framework from `julcst/gltemplate` version `1.7b` through CMake `FetchContent`.

The project must run on macOS and Windows. Source code should stay platform independent unless a platform-specific branch is clearly isolated behind a small interface. Assets are produced in Blender, then imported into the runtime format chosen for the scene pipeline. Expected authored assets include the hut, bamboo, character, sword, terrain elements, and supporting props.

## Core Interaction

The exact input model is still open. Two candidates are documented in [docs/INPUT_DECISION.md](docs/INPUT_DECISION.md):

- Angle input: the user selects cut angle and optional target height.
- Mouse-drawn lines: the user draws one or more cuts over the bamboo target.

Both variants should feed the same internal cut request: target height, direction, angle, strike validity, and strength estimate. This keeps the animation, collision, and bamboo response systems independent from the final UI decision.

## Initial Build

```bash
cmake -S . -B build
cmake --build build
```

On Windows, use a generator matching the installed compiler, for example Visual Studio or Ninja. On macOS, Xcode or Ninja are both suitable.

## Repository Layout

```text
assets/
  audio/            Sound effects, ambience, and mix notes.
  models/           Runtime-ready exported meshes.
  reference/        Concept PDFs and non-runtime references.
  source_blender/   Blender source files.
  storyboard/       Rendered storyboard images for documentation.
  textures/         Runtime-ready texture assets.
docs/
  ASSET_PIPELINE.md
  CONCEPT.md
  INPUT_DECISION.md
  TECHNICAL_PLAN.md
  TODO.md
src/
  main.cpp
```

## Current State

This is the initial repository structure. It contains the concept documentation, build skeleton, reference assets, and implementation roadmap. No scene systems are implemented yet.
