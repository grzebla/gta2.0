# Roadmap

M1 remains active. T0001 / M1-001 is done. Only the two tasks below are promoted now. Later entries are ordered planning slots, not READY assignments. Each implementation slot targets 1-2 hours; split further when evidence reveals more work.

## Ready

- M1-002: unsloped geometry contract, evidence gaps, proposed comparison scenarios and CMAP comment correction. Review before implementing dependent conventions.
- M1-003: external local capture destinations and synthetic path-validation tests. Complete before further original-data screenshot runs.

## Ordered M1 backlog

1. Resolve elevation and drawable-face selection gaps through bounded local original-game observations. Record provenance and measurements; do not use engine screenshots as original references.
2. Resolve face-local texture axes and rotation/flip order with asymmetric references. Establish independent corner expectations for five faces and eight transforms.
3. Add a pure unsloped block-to-indexed-mesh builder. Test all walls, lids, selection flags, tile bounds and reserved markers.
4. Assemble synthetic multi-level maps. Assert exact face counts, finite vertices, valid indices, height bounds and seams at map boundaries.
5. Add tested UV assignment for all five faces and eight transforms. Preserve half-texel atlas bounds.
6. Specify original projection and elevation measurements, including reproducible camera settings. Approve numerical parameters before fidelity claims.
7. Implement GPU-independent projection math with measured reference points and clip/depth tests.
8. Add indexed world-mesh rendering with a synthetic scene across GLSL/MSL/HLSL; retain the dynamic quad path and shader freshness checks.
9. Add depth targets and depth testing for window/offscreen paths, including resize and resource lifetime checks.
10. Add offscreen tests for reversed submission order and opaque wall/roof/stack occlusion.
11. Add cutout handling and offscreen tests proving transparent texels reveal lower geometry without writing occluding depth.
12. Connect the reviewed unsloped map mesh to the application. Verify district building heights and document remaining slope limitations.
13. Inventory all 64 encoded slope values against format evidence and local maps. Classify supported, reserved and unresolved codes; define topology and UV expectations independently of implementation.
14. Implement ordinary slope families in separate sessions, each with every orientation, winding, area and seam tests.
15. Implement diagonal slope families with independent topology and texture tests.
16. Implement three-sided families and marker-1023 handling, including observed nonstandard marker combinations.
17. Integrate slope meshes with multi-level rendering and offscreen occlusion tests.
18. Add an explicit local all-17-map geometry gate. Missing data or maps must fail this invocation; validate finite vertices, indices, bounds, supported slopes and reserved tiles. Ordinary asset-free CI remains usable.
19. Cache CPU world geometry. Test unchanged scenes, camera movement and map/style replacement with rebuild counters.
20. Cache GPU static buffers. Assert zero static uploads after warm-up; test replacement and resource lifetime.
21. Add repeatable geometry/frame benchmarks reporting construction time, vertices, indices, memory, uploads and median/p95 frame time with fixed sampling and hardware/build metadata.
22. Review performance baselines, introduce justified thresholds and test regression detection. Optimise only measured bottlenecks.
23. Add local original-reference comparison tooling with reviewed tolerances and negative controls. References and images stay outside the checkout and artifacts.
24. Capture and compare Downtown, Residential and Industrial at recorded views covering walls, slopes and heights; Astra reviews external images and comparison results.
25. Run gating synthetic world-render tests on Metal, Direct3D 12 and Vulkan. Split backend fixes into separate sessions. The existing non-gating Vulkan smoke job is insufficient.
26. Review one integration revision with native builds, unit/conformance tests, sanitizers, format/tidy, shader freshness, module boundaries, 17-map gate, comparisons and performance gates. Accept M1 only when every criterion has evidence.

Evidence collection and independent test infrastructure may proceed alongside implementation. No implementation may silently convert an unresolved hypothesis into a fidelity claim.

## After M1

Sprites and animated textures; world objects; player and controls; world collisions; vehicles and physics; pedestrians and AI; police/wanted level; gangs/respect; weapons/damage; missions/scripts; UI/HUD; audio; save state; remaining mechanics; optimisation/platform parity; full-gameplay regressions and original-behavior comparisons; stable release candidate. Only the human declares completion.
