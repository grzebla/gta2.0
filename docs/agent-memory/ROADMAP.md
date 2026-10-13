# Roadmap

M1 remains the only active implementation milestone. Existing approvals stand; no task is cancelled.

Immediate queue:
1. T0020/T0021 repair the independently identified Windows and Linux CI failures.
2. T0022 fixes mesh allocation growth; T0016 supplies strict GPU execution.
3. T0023 reviews existing world references. T0024 validates debug scenarios and prepares one stationary Downtown scenario. Request a paid run only after review if existing frames are insufficient.
4. T0025 researches only codes 41-44. T0017 establishes configurable synthetic projection mathematics.
5. T0018 audits partial CPU geometry across all 17 maps after the allocation fix.
6. T0026 adds persistent GPU storage; T0027 adds opaque indexed world depth rendering; T0028 tests transforms and cutout depth; T0029 makes software Vulkan gating.

Further M1 work must remain separate developer sessions:
- Calibrate projection using local original references, independent holdout measurements and reviewed residual tolerances.
- Resolve unsloped elevation, wall selection, flat flags and original texture orientation; update contracts only from evidence.
- Add application CPU mesh caching and explicit map/style invalidation, then connect the tested GPU path in a separate integration task. Require zero rebuilds/uploads over 120 unchanged or camera-only frames; successful map/style replacement invalidates exactly the affected state and failed replacement preserves it.
- Research and implement each slope family separately: S1 41-44, S2 1-8, S3 9-40, S4 diagonal walls 45-48, S4b their lids, S5 three-sided 49-52, S6 four-sided 49-52, S7 strips 53-56, S8 corners 57-60, S9 center 61. Each implementation remains blocked by world-slopes.md section 7.2; U1 also gates textured rendering. Code 63 needs its own evidence task; code 62 needs explicit unsupported handling.
- Integrate one verified family at a time and rerun the strict 17-map audit. A partial audit never becomes complete by relabeling.
- Add local comparison infrastructure with provenance checks, independent references, negative controls and fixed reviewed tolerances. Review Downtown, Residential and Industrial separately at recorded cameras; keep all original pixels outside git and CI artifacts.
- Establish strict Metal and Direct3D 12 execution on existing available environments in separate backend tasks. Do not provision resources or infer parity from shader compilation.
- Record geometry construction, counts, allocated bytes, uploads and median/p95 frame time with fixed hardware/build/views and warm-up/sample settings. Enforce reviewed thresholds; add chunking or other optimization only from measurements.
- Reverify the entire integrated revision: native matrix, conformance, sanitizers, format/tidy, shaders, boundaries, three GPU backends, original comparisons and performance gates. Astra then decides M1 acceptance.

After M1: sprites/animated textures; objects; player/controls; collisions; vehicles/physics; pedestrians/AI/police; gangs; weapons/damage; missions/scripts; UI/HUD; audio; saves; remaining mechanics; optimization/platform parity; gameplay and original-behavior regression; stable release candidate. Only the human declares the project finished.
