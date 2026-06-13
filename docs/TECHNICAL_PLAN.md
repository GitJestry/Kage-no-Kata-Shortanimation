# Technical Plan

## Runtime Stack

- Language: C++23.
- Build: CMake 3.26 or newer.
- Graphics foundation: university OpenGL adapter framework from `julcst/gltemplate` v1.7b.
- Asset authoring: Blender.
- Target platforms: macOS and Windows.

## Portability Rules

- Use standard C++ and framework APIs for file paths, timing, input, rendering setup, and resource loading where possible.
- Keep platform branches small and isolated.
- Avoid OS-specific path separators in code.
- Store runtime assets with relative paths from a known project asset root.
- Do not rely on shell scripts for the main build path.
- Verify CMake configure and build on both macOS and Windows before final delivery.

## Planned Systems

### Scene Bootstrap

Create application initialization, camera setup, asset root lookup, render loop, and deterministic scene reset. The first scene should load a simple bamboo proxy before full character work begins.

### Asset Loading

Import Blender-authored models into a runtime-friendly format. Track source `.blend` files separately from exported meshes and textures. Keep scale, orientation, and naming consistent across hut, bamboo, character, sword, and terrain props.

### Input System

Capture cut intent and convert it into a shared cut request. The final UI can be angle-based or mouse-line-based, but downstream systems should receive the same normalized data.

### Animation

Build a compact state flow:

1. Idle near hut.
2. Family-picture pause.
3. Kneel.
4. Draw sword.
5. Wind-up.
6. Cut.
7. End pose.

Use a small number of clean poses and blend between them. The first milestone can use placeholder transforms before rigged character animation is integrated.

### Bamboo Response

Represent bamboo as segmented geometry with predefined cut zones. A valid cut computes depth, break direction, and fall behavior. The first version can use deterministic rules before adding richer physics.

### Particles and Effects

Trigger short-lived particles at the cut point: bamboo fibers, dust, and splinters. Add motion blur or a sword trail only after the core strike is readable.

### Audio

Layer wind ambience, footsteps, sword draw, impact, bamboo fracture, and subtle environmental sound. Audio should respond to cut quality.

## Milestones

1. Build skeleton and empty application.
2. Load terrain, hut, bamboo proxy, and camera.
3. Add debug cut input and visual target overlay.
4. Implement bamboo cut evaluation.
5. Add simple character pose flow.
6. Replace proxies with Blender exports.
7. Add particles, audio, motion blur, and film grain.
8. Polish timing, color, and final presentation.

## Risk Management

- Schwertanimation: keep the move set small and readable.
- Bamboo physics: start with segmented deterministic behavior.
- Input: validate against the bamboo target before triggering animation.
- Particles: cap count and lifetime.
- Scope: protect the MVP before adding cinematic extras.
