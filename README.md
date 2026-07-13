# KageEngine

KageEngine is the C++ OpenGL editor/runtime for **Kage no Kata - The Final
Cut**, a University of Bonn short animation project by Julian Meyer and Faouzi
Homsani.

Current scope: load Blender GLB assets, edit the film world, validate and render
the samurai rig, play authored animation clips, import model/animation GLBs, and
keep shared scene data in
`projects/kage_no_kata_world.kage.json`.

## Build

Large assets use Git LFS:

```bash
git lfs install
git lfs pull
```

```bash
cmake -S . -B build
cmake --build build --config Release
```

Final MPEG4 export requires `ffmpeg` on `PATH` (for example
`brew install ffmpeg` on macOS).

CMake fetches the pinned framework version. Run from the project root:

```bash
build/kage_engine
```

Import checks:

```bash
ctest --test-dir build --output-on-failure
```

## Where To Look

- [Architecture](docs/ARCHITECTURE.md)
- [Performance baseline](docs/PERFORMANCE.md)
- [Engineering contract](docs/ENGINEERING_CONTRACT.md)
- [Asset pipeline](docs/ASSET_PIPELINE.md)
- [Requirements and evidence](docs/PROJECT_REQUIREMENTS.md)
- [Editor workflow](docs/EDITOR_WORKFLOW.md)
- [Film workflow](docs/FILM_WORKFLOW.md)
- [Milestone checklist](docs/TODO.md)

## References

- [Project concept](assets/reference/Kage_no_Kata_Konzept.pdf)
- [Storyboard PDF](assets/reference/Kage_no_Kata_Storyboard.pdf)
- [Official project brief](assets/reference/cgintro-animation-project-info.pdf)
- [Official assessment sheet](assets/reference/cgintro-bewertungsbogen.pdf)

Local editor state and local test scenes stay in `.kage_local/`.
