# ARCHITECTURE — decisions in force

1. Own engine in C++20, no OpenGTA2/MonoGame/.NET (owner decision 2026-10-12).
2. SDL3 + SDL_GPU; shaders: MSL source for Metal, GLSL->SPIR-V (glslang, committed, freshness
   checked in CI), HLSL compiled at build time (dxc/fxc). Bindings: vertex uniforms b0/space1
   (set 1), fragment sampler t0/s0 space2 (set 2).
3. Module boundaries enforced by `scripts/ci/check-module-boundaries.sh`: core, assets, sim,
   scene and testing never include SDL, render or platform headers; core and assets never
   include sim/scene/testing; sim never includes scene/testing. `sim` stays headless,
   deterministic, fixed point.
4. Dependencies via FetchContent pinned by URL + SHA-256 (SDL 3.4.18, doctest 2.5.3,
   nlohmann/json 3.12.0). Warnings are errors; clang-format 23.1.3, clang-tidy 22.1.8.
5. Map facts (verified on original data): column index y*256+x; DMAP column = 1+(height-offset)
   dwords; lid tile 1023 marks 3-sided diagonal slopes; tiles 992-1022 reserved; rotation codes
   turn clockwise on screen; flip mirrors in tile space before rotation; lid lighting factors are
   unknown (not applied) - to be measured on the original.
6. Style facts: GBST version 700; palettes stored in dword columns (BGRA); tiles 64x64 in a 4x4
   grid per 256x256 page.
7. GTA2_RE is a behavioural reference only (names of functions in docs), never copied.
8. Development is run by the orchestrator (`tools/orchestrator`): Astra plans/reviews (read-only),
   Claude implements on `agent/<task>` branches, approved work is merged into
   `agents/integration`; humans merge PRs.
