 Developer Contract

## Applicability

This contract applies to every contributor working on this repository,
including automated engineering tools.

## Binding sources

For Movie Editor work, the following documents are authoritative:

1. docs/PROJECT_REQUIREMENTS.md
2. docs/MOVIE_EDITOR_ARCHITECTURE.md
3. this developer contract

When these documents conflict, project requirements take precedence.

## Scope

- Implement only the approved Movie Editor requirements.
- Do not introduce additional feature claims.
- Do not add speculative systems or unrelated refactors.
- Do not add a production dependency without explicit approval.
- Do not add tool or provider references to source code, UI, commits, or
  project documentation unless required by university policy.

## Architecture

- World Edit owns persistent authored World state.
- Movie evaluation never mutates authored World state.
- Film time uses integer frames at 30 fps.
- Valid film ranges are half-open.
- The maximum exclusive film end is frame 3600.
- FilmFrameState is the runtime boundary.
- Timeline invariants and editing operations do not belong in ImGui.
- Existing project data remains loadable.
- Existing animation blending support remains available, but Movie clips do
  not receive automatic cross-fades.

## Implementation workflow

- Implement exactly one approved milestone per task.
- Keep every milestone buildable and testable.
- Prefer adapting existing working code over replacing it.
- Stop before making an architectural decision not covered by the approved
  plan.
- Stop before introducing a dependency.
- Do not implement later milestones opportunistically.

## Verification

- Run the smallest relevant tests first.
- Run all affected CTest targets before completing a milestone.
- Add regression tests for every corrected timeline invariant.
- Never claim a test was run when it was not run.

## Completion report

Report only:

- changed files;
- tests and commands;
- remaining risks.
