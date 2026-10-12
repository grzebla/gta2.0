# Roadmap

M1 is active. T0001 unit-block geometry, T0002 conventions and evidence inventory, and T0003 external capture handling are complete.

## Ready work

1. M1-004: isolate offline shell-test outputs and cleanup; resolve the confirmed Gatekeeper data-loss finding without cloud execution or protected-path edits.
2. M1-005: CPU texture-corner mapping, 40 synthetic face/rotation/flip combinations and eight regressions against current lid behavior.

## Following M1 dependencies

Obtain local original-game references and establish projection, elevation, face selection and texture transforms. Keep unresolved assumptions explicit. Then schedule session-sized work for unsloped CPU mesh assembly, projection, GPU mesh drawing, depth and transparent-texel occlusion. Split slope families into separate tested increments, including diagonal and three-sided geometry.

Follow with explicit geometry validation on all 17 original maps, CPU/GPU caching and invalidation, reviewed local district captures, fixed-view benchmarks and enforced regression thresholds. Finish with required native gates and synthetic rendering tests on Metal, Direct3D 12 and Vulkan. Parser success and non-gating smoke tests do not satisfy these gates.

## After M1 acceptance

Sprites and animated textures; world objects; player controls and collisions; vehicles and physics; pedestrians and AI; police, gangs, weapons and damage; missions and scripts; UI, audio and saves; remaining mechanics; optimisation and platform parity; full-gameplay regression and original-game behavioral comparisons; stable-release review. Only the human declares completion.
