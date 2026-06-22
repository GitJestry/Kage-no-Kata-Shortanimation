# Interaction Design

## Decision

The presentation accepts one mouse-drawn cut at a time and generates its strike
procedurally. An ImGui panel exposes angle and target height for repeatable tests.

## Input Flow

1. `AwaitCutInput` activates the bamboo target.
2. Mouse press, movement, and release record screen positions and timestamps.
3. A smoothing pass produces stable start and end points.
4. Camera rays project both points onto a bamboo-local interaction plane.
5. Target bounds, line length, and character reach determine validity; rejected
   lines appear red while input remains active.
6. `ProceduralStrikePlanner` generates wind-up, contact, follow-through, and
   recovery transforms from the valid `CutRequest`.
7. Runtime IK drives the torso, arms, hands, and sword while the feet remain
   planted.
8. The contact transform emits one synchronized impact event.
9. Recovery returns the character to `AwaitCutInput`.

```cpp
struct CutRequest {
  glm::vec2 screen_start;
  glm::vec2 screen_end;
  glm::vec3 local_start;
  glm::vec3 local_end;
  float angle_radians;
  float target_height;
  float draw_speed;
  float strength;
  int direction;
  bool valid;
};
```

Cut position and angle define the contact transform. Direction selects the
wind-up and follow-through sides. Drawing speed and line length shape timing and
strength. Parametric curves generate the sword path, and runtime IK converts its
transforms into skeleton poses. Bamboo physics, particles, and audio consume the
generated contact event on the same simulation step.

## Acceptance

- Cut position, angle, and direction continuously affect the generated motion.
- Left-to-right and right-to-left cuts approach from opposite sides.
- The debug overlay matches the generated angle, height, and strength.
- Every accepted cut produces one impact and returns to the ready pose.
- Repeated input produces repeatable animation and physics parameters.
