# Architecture

Reviewed at 4dcbdeb167c147d36303089deaff37d69959ea8c.

Retain the enforced dependency graph: scene depends on core/assets; render depends on core/scene and SDL; sim remains independent of scene/render/platform; apps coordinate. World geometry requires no new module or dependency edge. Avoid an ECS or general resource manager for M1.

Implemented CPU contracts: WorldMesh owns position/UV vertices and u32 indices. build_unsloped_map_mesh traverses 256x256x8 cells in deterministic z/y/x order, emits provisional non-flat unsloped faces, and reports fixed-size partial-coverage statistics. Slopes are skipped whole. Coordinates, face selection and UV conventions are not original-game evidence.

Current application rendering remains Camera2D plus rebuilt DrawList quads. Every frame uploads the instance list; shaders force z=0 and the pipeline has no depth attachment. WorldMesh has no production renderer call site.

Next contracts: scene owns configurable projection mathematics and camera-independent CPU meshes. Renderer owns persistent vertex/index buffers, transactional replacement, world shaders and depth resources. Static uploads happen on replacement, not camera motion. Keep world rendering separate from existing quad blending. Start with offscreen synthetic geometry; integrate maps only after that path is tested.

Use opaque/cutout world depth semantics with explicit tests for transparent texels. Do not infer flat-face meaning, sidedness, partial-alpha behavior or original projection constants from synthetic tests. Delay hidden-face removal and chunking until measured need and supported semantics.

WorldMesh currently reserves the next exact size per face. Replace that with amortized growth before 17-map construction measurements. Document allocation failure behavior accurately.

Testing stays layered: CPU contracts, strict backend-selected synthetic GPU tests, explicit local 17-map audits, and separately calibrated original comparisons. The current optional GPU tests and non-gating Vulkan smoke cannot establish backend acceptance.
