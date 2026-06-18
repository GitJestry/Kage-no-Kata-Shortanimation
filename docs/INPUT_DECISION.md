# Interaction Design

## Decision

The presentation uses a mouse-drawn cut. An ImGui panel exposes angle and target height for repeatable tests.

## Input Flow

1. `AwaitCutInput` activates the bamboo target.
2. Mouse press, movement, and release record screen positions and timestamps.
3. A smoothing pass produces stable start and end points.
4. Camera rays project both points onto a bamboo-local interaction plane.
5. Target bounds, line length, and reachable angle determine validity.
6. A valid `CutRequest` starts the strike; an invalid line appears red and returns control to `AwaitCutInput`.

```cpp
struct CutRequest {
    glm::vec2 screenStart;
    glm::vec2 screenEnd;
    glm::vec3 localStart;
    glm::vec3 localEnd;
    float angleRadians;
    float targetHeight;
    float drawSpeed;
    float strength;
    int direction;
    bool valid;
};
```

The angle blends `StrikeLeft`, `StrikeHorizontal`, and `StrikeRight`. Target height selects a bamboo joint. Drawing speed and line length determine strength. Direction controls the strike impulse.

Every strike clip contains a normalized impact marker. Animator, bamboo physics, particle emitters, and audio consume the marker on the same simulation step.

## Acceptance

- Three distinct lines produce three recognizable strike motions.
- The debug overlay matches the generated angle, height, and strength.
- Repeated input produces repeatable animation and physics parameters.
