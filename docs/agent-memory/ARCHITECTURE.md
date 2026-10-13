# Architecture

Reviewed at 07ce6db1a5f3c3dc898b1e3b062e0fdd97de9e07.

## Existing structure

- core provides numeric utilities and Camera2D; assets decodes GMP/STY.
- scene links core/assets and has no SDL dependency. It owns DrawList, TileAtlas, unit-block face geometry and face texture-corner mapping.
- render links scene/core/SDL3 and owns GPU resources. platform wraps SDL window/input services. app coordinates rendering and prototype simulation.
- testing links sim/assets/core and JSON. Current conformance goldens describe synthetic prototype behavior, not original GTA 2.

The graph is suitable for M1. The boundary script enforces only a subset of documented edges; strengthen it before the modules expand.

## Current renderer

QuadInstance is 40 bytes. append_map_textured emits lids in level order but discards elevation. Camera2D is orthographic; all quad vertex shaders use z=0. renderer.cpp uses alpha blending without a depth attachment. app rebuilds visible map quads every frame and Renderer uploads instances every draw. Buffer capacity reuse is not static geometry caching.

The 2048-square atlas uses nearest sampling and reserves slot 1023 as white. Semantic marker 1023 must never become a real world texture. Reserved tiles and unsupported slopes need explicit diagnostics.

T0001 establishes a mathematical unit-block contract only. T0005 supplies 40 face/rotation/flip mappings; lids preserve current-engine behavior and walls remain hypotheses.

## M1 design direction

Use scene-owned indexed position/UV meshes with 32-bit indices. Assets remains responsible for decoding; scene translates map semantics to geometry; Renderer owns upload, replacement, depth targets and GPU resource lifetime. App owns map/style revisions and coordinates caches. Do not add simulation dependencies or an ECS.

Keep QuadInstance for existing dynamic/overlay drawing. Add a separate GPU-independent world camera with explicit elevation and clip/depth conventions. Derive observable projection parameters from original references.

World geometry uses depth testing and transparent-texel cutouts. Avoid unverified neighbor culling and collision-bit visibility rules. Establish correct geometry before optimisation. CPU cache identity includes map/style/policy revisions; camera changes must not trigger tessellation or static uploads.

Keep test layers distinct: synthetic CPU contracts, synthetic GPU correctness, strict local 17-map geometry, local original-reference comparisons, and fixed-configuration performance gates. Each must fail when its required evidence is absent.
