# Decisions

Append-only log written by the orchestrator from Astra's recorded decisions.

## 2026-10-12 — Owner decisions (seed)

- Native C++20/SDL3/SDL_GPU engine; OpenGTA2 (C#) direction abandoned.
- Project renamed to GTA2.0 (`grzebla/gta2.0`, namespace `gta20`).
- Development supervised by Astra (gpt-6-astra via Codex CLI), implemented by Claude Code, run by the orchestrator. First milestone: M1 world geometry.

## 2026-10-12T21:25:28Z — Astra plan

Verified HEAD 8f820b7806946cbc5a4663e5d4a24eab60e69c1c. Scene generation emits flat quads, vertex shaders use z=0, and the renderer has no depth attachment. The headless preset already builds scene doctests without SDL. Module-boundary checks passed; no build directory or test logs were available to rerun the reported baseline. Create M1 and issue one small READY task for the owner's trial iteration. The roadmap below orders the remaining milestone work for later promotion. Also record that the current Vulkan smoke job is non-gating and that the GmpMap header's CMAP-equivalence comment contradicts existing tests and format documentation.

New tasks: T0001

## 2026-10-12T21:31:19Z — T0001 BLOCKED

Task T0001 "Add GPU-independent unit-block face geometry" at 76ef0295f

Code inspection found no implementation defect or task-commit policy violation. Approval requires verification currently blocked by the execution environment: no build outputs or formatter are available in this read-only checkout, and the developer reports denied formatter installation and environment removal. The orchestrator's successful Debug checks do not establish the required asset-free headless and formatting results.

- [x] Add and register the GPU-independent face geometry API. — Verified block_geometry.hpp defines all five faces, Position3f, four positions and six indices. block_geometry.cpp implements a pure table-based function; engine/scene/CMakeLists.txt registers it.
- [x] Document the complete mathematical contract and elevation limitation. — block_geometry.hpp:11–24 documents unit bounds, map axes, upward z, all face planes and normals, y-facing top/bottom walls, and the original-elevation disclaimer.
- [x] Produce two non-degenerate outward-facing triangles covering each unit square. — Checked all five corner-table entries and indices {0,1,2,0,2,3}. Each pair splits its square along a diagonal, has area 1/2 per triangle, and yields the required outward normals.
- [x] Add five named [world-geometry] cases covering all five faces. — scene_tests.cpp:135,152,188,204,244 defines the five requested cases. Each covers all five faces; the seam case handles the lid separately.
- [x] Use independent expectations, unit-area assertions, specified origins and adjacent-block seams. — Expected corners are generated from independently specified face planes. Tests cover all three required origins, index bounds, winding, total area, horizontal neighbours and vertically stacked wall edges.
- [ ] Pass headless configure/build/complete CTest without game data or GPU, preserve existing tests, and supply execution evidence. — The two existing scene tests are unchanged. Developer reports 56/56 headless CTest tests and 5 geometry cases with 588 assertions, but these remain uncorroborated claims and the geometry run retained GTA20_GAME_DIR. Supplied independent verification covers Debug only. This checkout has no build directory or test logs.
- [ ] Complete required formatting, boundary and whitespace checks. — Independently ran module boundaries, git diff --check and git diff HEAD^ HEAD --check successfully. CI pins clang-format 23.1.3; its required check remains unexecuted and no formatter was found.
- [x] Respect scope, architecture and protected-path constraints. — Verified the task commit changes exactly the four permitted files. No renderer integration, new dependency, restricted include or QuadInstance/DrawList change. Memory changes relative to the older baseline belong to the preceding planning commit.

## 2026-10-12T21:36:48Z — T0001 APPROVED

Task T0001 "Add GPU-independent unit-block face geometry" at 76ef0295f

Inspected commit 76ef0295fdb4fd4a8d33bb8e28bf64e1be9e7ce0. Geometry, tests and architecture meet the task requirements, with no identified defect or policy violation. Local boundary and whitespace checks pass. Supplied independent orchestrator verification establishes successful asset-free headless configure/build/tests, pinned formatting and data-enabled Debug verification, resolving the previous blocker. Build and formatter results were not rerun in this read-only checkout, which lacks those tools and outputs.

- [x] Add and register the GPU-independent face geometry API. — Verified block_geometry.hpp defines all five BlockFace values, Position3f and FaceGeometry with four positions and six indices. block_geometry.cpp implements a pure function using constant corner offsets. engine/scene/CMakeLists.txt registers the source.
- [x] Document the mathematical contract and original-elevation limitation. — block_geometry.hpp:11–24 specifies unit bounds, map-column x, map-row y, upward z, all five face planes and outward normals. It explicitly identifies top/bottom as walls and disclaims establishing original-game elevation semantics.
- [x] Produce two non-degenerate triangles covering each unit square with outward winding and valid indices. — Inspected all five corner tables and indices {0,1,2,0,2,3}. Each pair splits its square along a diagonal into two area-0.5 triangles. Cross products give left -X, right +X, top -Y, bottom +Y and lid +Z; every index is below four.
- [x] Add five named [world-geometry] doctest cases parameterized across all faces. — scene_tests.cpp:135,152,188,204,244 contains the five required cases. All cover the five faces; the seam case handles lids separately from stacked walls. tests/unit/CMakeLists.txt includes scene_tests.cpp and discovers doctests.
- [x] Use independent expected corners, unit-area assertions, specified origins and adjacent-block seams. — Expected corners are generated from independently specified planes, without reading implementation tables. Tests use (0,0,0), (3,5,2), (255,255,7), assert index bounds and total area one, compare opposing neighbour walls, and check vertically stacked wall edges and their coincidence with lid edges.
- [x] Pass asset-free headless configure/build/complete CTest, preserve existing scene tests, and supply commands, statuses and counts. — Verified the two original scene test bodies are unchanged and the headless preset disables the SDL application. Independent orchestrator results report exit 0 for asset-free headless configure, build and tests. Developer supplies exact commands with GTA20_GAME_DIR removed, 56/56 CTest tests and five filtered geometry cases with 588/588 assertions. Those detailed counts are developer-reported; orchestrator pass results corroborate suite success.
- [x] Complete formatting, module-boundary and whitespace checks. — Locally ran bash scripts/ci/check-module-boundaries.sh, git diff --check and git diff HEAD^ HEAD --check; all exited 0. Verified CI pins clang-format 23.1.3. Independent orchestrator verification reports that formatter check exited 0.
- [x] Respect task scope, architecture and fixed policy. — The task commit changes exactly the four permitted files. No renderer integration, new dependency, prohibited include, QuadInstance/DrawList change or weakened check appears. Memory changes since the reviewed baseline belong to the preceding Astra planning commit. Orchestrator checks also confirm protected paths untouched and no committed original assets or secrets.
