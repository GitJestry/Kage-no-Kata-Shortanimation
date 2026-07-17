# KageEngine

KageEngine is the C++23/OpenGL editor and runtime used to produce **Kage no
Kata – The Final Cut**, a University of Bonn computer-animation project by
Julian Meyer and Faouzi Homsani.

The repository contains the authored world, asset catalog, Movie Editor,
skeletal-animation pipeline, lighting and shadow pipeline, and deterministic
2160p30 film export. The submitted feature claims are deliberately narrower
than the complete engine: only features listed in
[Project Scope and Evidence](docs/PROJECT_SCOPE_AND_EVIDENCE.md) are claimed for
assessment.

![](/docs/images/final.png)

## Quick start

Large runtime assets are stored with Git LFS:

```bash
git lfs install
git lfs pull
```

Configure, build, and run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DKAGE_ENABLE_COVERAGE=OFF
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/kage_engine
```

On multi-configuration Windows generators, use `--config Release` and run
`build/Release/kage_engine.exe`.

The first CMake configure downloads pinned framework dependencies. Final MPEG4
encoding requires `ffmpeg` on `PATH`.

## Authoritative documentation

- [Project Scope and Evidence](docs/PROJECT_SCOPE_AND_EVIDENCE.md) — exact
  assessment claims, ownership, implementation evidence, and non-claims.
- [Architecture](docs/ARCHITECTURE.md) — system boundaries, data flow, ownership,
  rendering, animation, persistence, and export.
- [Build and Validation](docs/BUILD_AND_VALIDATION.md) — prerequisites,
  reproducible build commands, automated checks, and the manual submission
  checklist.
- [Project Report](docs/PROJECT_REPORT.md) — results, challenges, reflection,
  concept comparison, and lessons learned.
- [Asset Pipeline](docs/ASSET_PIPELINE.md)
- [Editor Workflow](docs/EDITOR_WORKFLOW.md)
- [Film Workflow](docs/FILM_WORKFLOW.md)
- [Code Style](docs/CODE_STYLE.md)

## Submission data

- `projects/kage_no_kata_assets.kage.json` — Asset Catalog schema v2.
- `projects/kage_no_kata_world.kage.json` — World schema v6 containing Film
  schema v2.
- `output/` — generated PNG frame sequences and MPEG4 files; generated output is
  not source data.

The full submission package must additionally contain all runtime assets,
licenses, the final MPEG4 film, and the illustrated report required by the
course brief.
