# PROJECT — GTA2.0 (current state)

Goal (fixed by the owner): faithful, native, cross-platform reimplementation of GTA 2 in C++20
(CMake/Ninja, SDL3, SDL_GPU: Metal / D3D12 / Vulkan). Repository `grzebla/gta2.0` (private).

State verified 2026-10-12 (commit on `feature/orchestrator`):
- Engine modules: core (Q16.16 fixed point, angles, RNG, fixed-step clock, handles), assets
  (GMP and STY parsers, game install detection), sim (deterministic, prototype rules), scene
  (draw lists, tile atlas), render (SDL_GPU, instanced quads + atlas), platform (SDL3), testing
  (scenarios, traces, comparison). Apps: `gta2` (window/offscreen), `gta20-headless`.
- Real data: `.gmp` (17 maps) and `.sty` (4 styles) parse and are verified; lids of
  Downtown/Residential/Industrial render with original textures on Metal (top-down only).
- NOT yet: 3D walls, slopes, lighting, animated tiles, sprites, objects, any gameplay mechanic.
- Tests: CTest 54/54 with original data (macOS ARM64 Debug); CI `native-ci` green on macOS,
  Windows, Linux for PR #3 head. Original data lives only locally (`GTA20_GAME_DIR`).
- Open PRs (owner merges, never agents): #2 engine -> main, #3 original assets -> #2,
  #4 rename to GTA2.0 -> #3, orchestrator PR -> #4.

Current milestone: M1 — full GTA 2 world geometry (see ROADMAP.md). Not started.
