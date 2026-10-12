# PROJECT

Fixed owner goal: faithfully reimplement GTA 2 as a native cross-platform C++20 application using CMake/Ninja, SDL3 and SDL_GPU, with fidelity measured by automated comparisons. Backends are Metal, Direct3D 12 and Vulkan.

## Reviewed baseline

Source inspected at 8f820b7806946cbc5a4663e5d4a24eab60e69c1c on 2026-10-12.

- Modules: core, assets, sim, scene, render, platform and testing. Apps: gta2 and gta20-headless.
- GMP/STY parsing, synthetic conformance tests, a tile atlas and an instanced-quad renderer exist.
- Textured map rendering emits lids at a common GPU z=0. There are no geometric walls, slopes or depth attachments.
- Simulation rules are prototypes, not verified original-game behaviour.
- Historical memory reports successful original-data tests and district lid renders. Those results were not rerun in this read-only review.

## Current work

M1: full GTA 2 world geometry, active and not yet implemented. M1-001 is the sole initial READY task, a 30–45 minute GPU-independent block-face builder. ROADMAP contains the complete ordered M1 plan; later entries are not yet board tasks.

No AI, vehicle physics or missions before M1 acceptance. Original assets and screenshots stay local. No AWS commands, cloud changes, developer pushes or agent merges. Protected paths remain untouched by the developer; the orchestrator applies memory updates.

PRs supplied by the orchestrator: #2 engine, #3 original assets, #4 rename, #5 orchestrator. Humans merge PRs. No integration-branch CI run was supplied.
