# Performance Baseline

Measured on Apple M4 at the committed world camera, using a RelWithDebInfo
build and a three-to-five second `sample` capture.

| Metric | Before | After render work |
| --- | ---: | ---: |
| Scene mesh draw calls before culling | 1,333 | at most 287 |
| Main-thread samples in world rendering | ~78% | ~12% |
| Process CPU while idle in the world | ~75% | 25–29% |
| Physical footprint after all assets load | 6.2 GB | 6.1 GB |

The CPU improvement comes from disabling per-call OpenGL error polling outside
Debug, merging static primitives by material, frustum culling, and generated
index-only LODs. Asset loading is capped at two concurrent CPU imports, which
reduces transient contention but does not reduce final residency.

The remaining memory work is deliberately separate: replace retained decoded
static geometry with a picking acceleration structure or GPU picking, share
textures across assets, and add viewport/full-resolution texture tiers before
discarding CPU image payloads. The source GLBs remain authoritative throughout.
