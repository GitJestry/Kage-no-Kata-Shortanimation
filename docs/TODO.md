# Four-Week Schedule

## Week 1: Import and Skinning

- [x] Define the shared C++ and OpenGL code style.
- [x] Define the macOS and Windows portability contract.
- [x] Record course approval for parser, audio backend, and bamboo simulation.
- [x] Configure and build Debug and Release on macOS.
- [x] Launch the OpenGL 4.1 application baseline on macOS.
- [ ] Configure, build, and launch the application baseline on Windows.
- [ ] Export a GLB test character with two actions.
- [x] Load and render static GLB mesh data.
- [ ] Load skinned vertex data.
- [ ] Evaluate the joint hierarchy and inverse bind matrices.
- [ ] Render one clip with the skinning shader and bone buffer.
- [ ] Capture the milestone and import metrics for the report.

Acceptance: the test character displays a stable bind pose and one complete clip.

## Week 2: Interaction and Physics

- [ ] Implement keyframe sampling, slerp, and cross-fades.
- [ ] Project the mouse line and produce a debug `CutRequest`.
- [ ] Generate a wind-up, contact, follow-through, and recovery sword path.
- [ ] Apply torso and two-hand IK with planted feet.
- [ ] Implement bamboo bodies, joints, impulse, integration, and ground contact.
- [ ] Synchronize animation and physics through the generated contact event.

Acceptance: each cut generates a matching strike and bamboo response, followed
by recovery and renewed input.

## Week 3: Environment and Effects

- [ ] Generate fBm terrain and normals.
- [ ] Place vegetation from seed, height, and slope.
- [ ] Implement the particle pool and three emitter types.
- [ ] Integrate scene and impact audio.
- [ ] Implement temporal subframe accumulation and film grain.
- [ ] Integrate the final character, clothing, sword, gate, hut, and bamboo.

Acceptance: the complete training scene presents every planned runtime feature.

## Week 4: Delivery

- [ ] Freeze the feature set and resolve prioritized defects.
- [ ] Finalize camera movement and scene timing.
- [ ] Render the 2160p30 PNG sequence with eight subframes.
- [ ] Encode the MPEG4 film.
- [ ] Test packaged Release builds on macOS and Windows.
- [ ] Complete the illustrated report, reflection, making-of, and presentation.

Acceptance: both platform builds start from the submission package, and every deliverable presents clear technical and personal contributions.
