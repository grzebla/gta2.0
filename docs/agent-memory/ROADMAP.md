# Roadmap

M1 is active. T0001–T0005 are complete and remain approved. New task keys are M1-006 through M1-035; the orchestrator assigns task IDs.

## Ordered M1 work

1. M1-006/007: close conformance execution and trace-integrity false-pass paths.
2. M1-008: enforce module boundaries and update stale architecture contracts.
3. M1-009/010: prepare the first original capture scenario and strict 17-map inventory. Astra requests host capture only after scenario review and commit.
4. M1-011/012: indexed CPU mesh and renderer failure handling.
5. M1-013/014: original projection measurements and world-camera implementation.
6. M1-015/016/017: backend shader variants, persistent indexed drawing, depth and cutouts.
7. M1-018/019: unsloped map mesh and application integration.
8. M1-020: sourced catalogue for all 64 slope codes. M1-021–024 implement four batches of at most 16 codes with independent fixtures.
9. M1-025: strict geometry validation on all 17 original maps.
10. M1-026/027: CPU cache and application GPU reuse, with zero rebuild/upload gates.
11. M1-028: fail-closed external-reference comparator. M1-029/030 verify unsloped and slope behavior against the original.
12. M1-031: Astra-reviewed external screenshots of all three districts.
13. M1-032/033: reproducible benchmarks and reviewed numeric regression budgets.
14. M1-034: gating synthetic GPU correctness on Metal, Direct3D 12 and Vulkan.
15. M1-035: all acceptance evidence on one reviewed integration revision.

Tasks are bounded sessions. Missing reference evidence, unresolved slope rows or unavailable required backend execution must be reported, not replaced with assumptions or skips. Substantial discrepancies found during evidence tasks become separate fix tasks.

## After M1 acceptance

Sprites/animated textures; world objects; player and controls; world collisions; vehicles and car physics; pedestrians/AI; police/wanted level; gangs/respect; weapons/damage; missions/scripts; UI/HUD; audio; saves; remaining mechanics; optimisation/platform parity; gameplay and original-behavior regression; stable release candidate. Refine ordering by actual dependencies. No gameplay expansion before the M1 gate.
