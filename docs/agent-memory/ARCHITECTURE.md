# ARCHITECTURE

## Existing implementation

- Native C++20 engine using SDL3 and SDL_GPU. Simulation stays headless, deterministic and fixed point.
- Module boundaries are enforced by scripts/ci/check-module-boundaries.sh. Core, assets, sim, scene and testing cannot include SDL/render/platform. Core/assets cannot depend on higher modules; sim cannot depend on scene/testing.
- Scene owns CPU draw descriptions and tile-atlas generation. QuadInstance is a 40-byte GPU-facing layout. Preserve it while introducing world geometry separately.
- Current shaders use two-dimensional quad positions with z=0. Rendering blends atlas samples and has no depth attachment.
- MSL is compiled at runtime; GLSL has committed SPIR-V with freshness checks; Windows builds HLSL with dxc/fxc. Every world-render change must cover all three shader paths.
- Pinned dependencies and warnings-as-errors remain in force. The headless CMake preset disables the SDL application and builds scene unit tests.

## M1 direction

Start with pure CPU face geometry in engine/scene. Its unit block occupies [x,x+1] × [y,y+1] × [z,z+1], with outward triangle winding. This is a helper contract, not yet proof of original-game elevation semantics.

Subsequent work adds independently tested UV transforms, indexed world meshes, an evidence-backed map-to-world elevation convention, projection and depth, then slopes and caching. Keep GPU resource ownership in render. Establish original face semantics before applying neighbour-face removal or interpreting collision bits as visibility.

## Format evidence and limits

- DMAP column index is y*256+x; column records contain height-offset indices beginning at offset.
- Documented side axes are left -X, right +X, top -Y, bottom +Y.
- Existing tests and docs say legacy CMAP differs from DMAP. The contrary parse_gmp header comment is stale.
- Lid tile 1023 usually marks three-sided diagonal slopes, but exceptions exist. Tiles 992–1022 occur in bonus maps. Do not sample these as ordinary style tiles or turn marker geometry into white atlas faces.
- Existing docs report visual validation of lid rotation/flip on Downtown. Wall orientation and slope mapping still require evidence and automated comparisons.
- Lid lighting factors remain unknown and unapplied.
- GTA2_RE may inform behavioural specifications; its code must never be copied. Label source facts, measured facts and hypotheses separately.
