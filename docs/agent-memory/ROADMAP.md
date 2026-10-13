# Roadmap

M1 remains active. Keep its acceptance criteria unchanged.

## Current queue

Preserve T0013 indexed CPU face assembly, followed by T0014 partial unsloped map building. Do not duplicate T0008 or T0011. Owner/orchestrator intervention should restore T0008 review at priority 1 and resolve T0011 integration reconciliation at priority 0.

New bounded tasks, each intended for one developer session:

1. Check existing GPU transfer sizes and resource failures.
2. Add a strict asset-free offscreen pixel-test target.
3. Add GPU-independent configurable world projection, separate from original calibration.
4. Audit T0014 geometry across all 17 local maps; incomplete support must remain explicit.
5. Produce a 64-code slope evidence table and family-by-family implementation proposal.
6. Render T0013 indexed faces through GLSL/MSL/HLSL with synthetic texture assertions.
7. Add depth, transparent-texel rejection and order-independent occlusion tests.
8. Connect the partial map builder to an explicit diagnostic geometry view.
9. Gate synthetic world rendering on software Vulkan; retain separate Metal/D3D12 requirements.
10. Test CPU/GPU cache invalidation, camera movement and equal-sized map/style replacement.
11. Record reproducible construction, memory, upload and median/p95 frame measurements.

Dependencies are recorded on the task board. CPU work need not wait for original-game capture repair.

## Remaining M1 work to issue after evidence review

- Implement one sourced slope family per task, including each orientation and independently derived corner/triangle expectations. Separate inclined, diagonal and three-sided families; do not guess undocumented codes.
- Integrate each family into map assembly and update unsupported counts and texture tests.
- Resolve flat-face, wall-selection and reserved-tile semantics using documented evidence.
- Obtain usable original WORLD references only after the protected runner prerequisite. Preserve provenance, camera settings and local-only storage.
- Add local comparison tooling with synthetic success/failure tests, then review projection/elevation calibration and texture-transform comparisons in separate tasks. Tolerances require evidence and Astra review.
- Promote the all-17-map audit to a complete strict geometry gate only when every required family is handled.
- Capture and review Downtown, Residential and Industrial views showing walls, slopes and heights.
- Review performance baselines, then add threshold enforcement and failure tests. Optimise measured bottlenecks in separate steps; retain correctness comparisons.
- Collect synthetic GPU execution on Metal/D3D12/Vulkan and all deterministic gates on the final integration revision.

No M1 acceptance from inventory, synthetic tests or attractive screenshots alone. No new AI, vehicle physics or missions before acceptance.

## After M1

Sprites and animated textures; world objects; player controls and world collisions; vehicles and physics; pedestrians/AI/police; gangs; weapons/damage; missions/scripts; UI/HUD; audio; save state; remaining mechanics; optimisation/platform parity; full-gameplay regression and original-game behavioral comparisons; stable-release candidate. Only the human declares the project finished.
