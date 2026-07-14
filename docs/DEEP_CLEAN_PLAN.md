# Deep Clean Plan

## Goal

Reduce production and test code substantially without removing submitted
features or changing saved project behavior.

Target: at least 4000 net deleted lines.

## Rules

- Every cleanup phase must have a net negative line count.
- Remove dead, test-only and out-of-scope production APIs.
- Do not preserve features solely because an old implementation plan mentioned
  them.
- Do not introduce abstractions with only one caller.
- Tests protect observable behavior, not deleted implementation details.
- Renderer, asset loading and export remain frozen unless a proven defect
  requires a change.
- One cleanup concern per commit.

## Phase 1 — Dead and out-of-scope code

Remove:
- write-only state;
- test-only production APIs;
- duplicate UI state;
- unused input structures;
- unsupported Movie lanes and controls;
- unused forwarding wrappers.

Consolidate duplicated lane classification.

## Phase 2 — Compatibility and migration

Decide which historical project formats must still load.

Remove compatibility code and fixtures that are not required for the submitted
project.

## Phase 3 — Movie Editor UI

Remove repeated labels, duplicated actions, duplicate diagnostics and repeated
timeline infrastructure.

Keep each action in one obvious location.

## Phase 3B result

The Master and Sequence timelines already share the meaningful reusable
mechanics. Further consolidation would introduce abstractions or couple distinct
interaction behavior for an estimated reduction below 30 lines.

No implementation change was made.

## Phase 4 — Film domain and controller

Reduce duplicate state, forwarding methods and validation paths.

Retain one canonical implementation for each invariant.

## Phase 5 — Tests

Consolidate repeated setup using small builders.

Keep tests for:
- World-state immutability;
- evaluation;
- current-schema save/load;
- Bake validation;
- preview state;
- selection;
- preview/export consistency.

## Completion criteria

- Full build succeeds.
- All retained tests pass.
- Manual Movie workflow passes.
- Current project saves, reloads, previews and bakes.
- No submitted feature is lost.
- Net reduction is at least 4000 lines.