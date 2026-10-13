# Known issues

Architecture review at 07ce6db1a5f3c3dc898b1e3b062e0fdd97de9e07.

## Verification defects

- cmake/RunConformance.cmake ignores the second determinism execution status. Reused output can conceal a failed run. M1-006.
- Trace comparison can pass matching header-only inputs and conceal duplicate entity IDs through map overwrite. Unknown trace provenance becomes synthetic. M1-007.
- Optional game-data unit cases return successfully without assets. Their passing count does not establish original-data execution. The 17-map case tests parsing, not geometry. M1-010/025.
- Current GPU smoke is non-gating Vulkan only. Required Metal/D3D12/Vulkan world correctness and current-revision hosted CI remain unverified. M1-034/035.
- Boundary enforcement omits several documented include/link restrictions. M1-008.

## Renderer and fidelity

- Runtime has no walls, slope topology, height-dependent projection or depth buffer. Flat lids use submission order.
- Map geometry is rebuilt and uploaded every frame. Reused buffer capacity does not meet the static-cache requirement.
- GPU transfer mappings, command/pass acquisition and some synchronization results are unchecked. M1-012.
- Wall selection, flat flags, elevation mapping, original texture orientation, all slope families and reserved-tile behavior remain unresolved.
- Atlas slot 1023 is white; GMP marker 1023 requires semantic handling, not atlas sampling.
- No original references or calibrated projection exist. No capture scenario is committed yet. M1-009/013.
- Geometry memory, construction time, upload counts and frame p95 have no reviewed budgets.

## Documentation debt

Architecture documentation still describes a 24-byte instance and future atlas. The geometry specification's level-relabel example has the wrong sign, recommends already-completed T0005 and predates current orchestrator-only host capture policy. M1-008.

T0004's destructive cleanup and temporary-path defects remain resolved. Full offline shell-suite execution remains prohibited for agents because it invokes aws, including its mock. Use only the reviewed AWS-free isolation self-test.
