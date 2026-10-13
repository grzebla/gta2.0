# Architecture

Reviewed integration a2e5232.

## Ownership

- core: math, types, cameras and diagnostics.
- assets: game-file decoding and map/style associations.
- scene: GPU-independent face geometry, texture mapping, draw lists and future CPU world meshes. Depends on core/assets, never sim/render/SDL.
- render: SDL_GPU shaders, buffers, textures, pipelines, depth targets and resource lifetime.
- platform: SDL window/input and image output.
- apps: module composition, scene replacement and camera control.
- testing: simulation traces/comparison. Existing simulation goldens are prototype regressions, not original-game evidence.

Actual CMake links follow this general split. Integration's checker only examines selected include patterns. T0008's stronger checker and corrected documentation remain unapproved. Avoid turning its custom CMake lexer into an unlimited interpreter; supported syntax and rejection behavior must be explicit and tested.

## Current rendering

QuadInstance is 40 bytes. The atlas is 2048x2048; slot 1023 is opaque white for debug quads and must not render GMP marker 1023. App build_frame rebuilds map/entity quads each frame; render uploads all instances. Buffer-capacity reuse is not static geometry caching.

Shaders use world z=0. The pipeline blends color, disables culling and has no depth target. Camera2D is orthographic and ignores elevation. Offscreen rendering allocates and downloads a target per call, so its timings include costs absent from window rendering.

T0001 supplies five outward-wound unit-block faces with lids at origin.z+1. T0005 supplies 40 face/rotation/flip contracts. Neither proves GMP elevation or original texture orientation.

## Approved direction

T0013/T0014 establish indexed CPU meshes and an explicitly partial map builder. Renderer-owned indexed resources consume that contract; scene receives no GPU handles. Preserve the quad path while a diagnostic geometry-only path gains configurable world projection, depth and binary-alpha cutout.

Discard transparent world texels before depth writes. Test walls, roofs and stacked surfaces in both submission orders. Projection parameters remain provisional until original references establish them. Camera2D culling cannot be reused for elevated geometry without proof that bounds are conservative.

Use explicit map/style/policy revisions for cache invalidation. Camera motion must only change view data or visibility selection. Test equal-sized content replacement, not only changed counts. Measure rebuilds and static uploads separately from dynamic quads.

Add strict asset-free GPU tests with backend identity checks. Required Vulkan CI is one backend's evidence; Metal and D3D12 execution remains necessary. All-17-map geometry audits must distinguish partial coverage from completeness.

## Debt

renderer.cpp has unchecked transfer mappings and acquisition paths, plus unchecked GPU-size narrowing/multiplication. Harden these before extending buffers. Existing public architecture text incorrectly says QuadInstance is 24 bytes and lists the atlas as future work; resolve through existing T0008.
