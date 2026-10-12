# TEST STATUS

## Independently checked in this planning review

Revision: 8f820b7806946cbc5a4663e5d4a24eab60e69c1c.

- scripts/ci/check-module-boundaries.sh: passed.
- git diff --check: exit 0; no tracked changes reported. Git emitted sandbox warnings about creating its temporary xcrun cache.
- Source inspection confirms headless scene tests are supported by the headless preset.
- No build directory or executable test results were available. No configure/build/CTest run was attempted in the read-only checkout.
- native-ci defines native builds, sanitizers and static checks. Its Vulkan render smoke is explicitly non-gating.

## Historical results supplied by project memory

These were not independently rerun on the reviewed HEAD.

| Suite | Reported result |
|---|---|
| macOS ARM64 Debug CTest with original data | 54/54 |
| CTest without original data | 54/54, data tests return early |
| ASan/UBSan with data | 51/51 |
| Original-data tests | 9 cases, 448880 assertions |
| Offscreen district lid checks | 3/3 |
| Native CI | Green on PR #3 head |
| Orchestrator tests | 30/30 |
| Original-game comparisons | None |

No agents/integration CI run was supplied. These historical counts are not M1 acceptance evidence.

## Next verification

M1-001 requires five new [world-geometry] doctests, complete headless CTest, module boundaries, formatting and diff checks. All new cases must execute without game data or a GPU.

M1 later requires explicit 17-map execution, independently expected slope/UV geometry, synthetic depth/alpha rendering checks, local original-game comparisons, district review, cache counters, performance baselines and Metal/D3D12/Vulkan evidence. Missing original data cannot satisfy the local acceptance gate.
