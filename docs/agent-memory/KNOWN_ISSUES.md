# Known issues

Reviewed integration a2e5232.

1. M1 runtime gap: only flat lids and prototype quads render. Indexed world geometry, slopes, elevation projection, depth and static caching are absent.
2. Fidelity gap: wall selection, flat flags, GMP elevation, wall UV orientation, slope topology and reserved-tile behavior lack sufficient original-game evidence. T0001/T0005 are synthetic contracts. Lid-lighting factors remain unmeasured.
3. GPU failure handling: renderer.cpp uses unchecked mapped pointers and several unchecked command/pass acquisitions. Upload/readback byte calculations include unchecked u32 multiplication or narrowing. These are inspection findings; device failures were not reproduced.
4. Depth risk: existing shaders blend transparent texels without discard. Reusing them with depth writes would let invisible texels hide lower geometry. Require binary-alpha occlusion tests with reversed submission order.
5. Culling/cache risk: Camera2D bounds ignore elevation; pointer/count cache keys would miss equal-sized map/style replacements. New world paths need explicit contracts and tests.
6. T0008 blocked after three review attempts. Integration still has the old include-only checker and stale architecture text. Last review recorded escaped-target, CMake alias and external-include bypasses on its branch. Preserve existing work; owner/orchestrator must requeue it.
7. T0011 remains an integration conflict/permission blocker. No resolved integration head or current complete CI evidence is established. Do not bypass merge denial or push.
8. T0012 confirmed all 20 discovery frames show an unchanged MENU region. No WORLD reference exists in that run. D1 is a suspected null-dialog verdict defect in inspected origin/main d7b8be7; PowerShell behavior was not executed locally and deployed runner provenance remains unknown. Input consumption, injection-time focus and frontend readiness remain unresolved. Owner-controlled runner review and revision-identified diagnostics are prerequisites; diagnostics cannot prove consumption. No further paid capture requested.
9. Test/performance gap: no indexed-world GPU assertions yet; current Vulkan smoke is non-gating. Optional game-data tests may succeed without data. Inventory does not validate generated geometry. Existing benchmarks measure prototype simulation or general frame statistics, not a reviewed M1 baseline.
