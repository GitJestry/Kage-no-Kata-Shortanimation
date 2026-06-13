# Input Decision

The input model is intentionally open during the first implementation phase. The runtime should convert every UI variant into a shared `CutRequest` concept so that animation, bamboo simulation, and effects do not depend on the final input surface.

## Candidate A: Angle Input

The user selects a cut angle and optional target height on the bamboo.

Advantages:

- Easier to validate.
- Fast to implement.
- Clear mapping to animation clips and cut outcomes.
- Predictable for grading and debugging.

Risks:

- Less expressive.
- May feel like parameter selection instead of interaction.

## Candidate B: Mouse-Drawn Lines

The user draws one or more cut lines over the bamboo.

Advantages:

- Stronger connection between gesture and sword movement.
- Supports multiple attempted cuts.
- Fits the storyboard drawing of marked bamboo targets.

Risks:

- Requires careful filtering and hit validation.
- Needs robust handling for imprecise or off-target input.
- More UI and math work before the scene feels reliable.

## Shared Internal Parameters

The chosen input should produce:

- Bamboo target identifier.
- Start and end point in screen space or target-local space.
- Cut angle.
- Target height.
- Direction vector.
- Validity score.
- Strength estimate.
- Optional sequence index for multiple cuts.

## Recommended First Step

Implement an angle-and-height debug input first. Keep the data model compatible with mouse-drawn lines. Add mouse input once the bamboo target projection, hit testing, and debug visualization are stable.
