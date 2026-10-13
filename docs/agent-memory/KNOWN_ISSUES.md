# Known issues

- P0 Windows compilation: map_inventory.cpp:45 returns int from the lowercase lambda into std::string. MSVC /WX rejects C4244. Confirmed in native-ci run 38429483238, both Windows configurations. T0020.
- P0 Linux tests: capture rejection diagnostics wrap across lines, but CaptureDirTest.cmake expects literal spaces. Three tests fail in Release and ASan despite nonzero rejection and zero renderer calls. Preserve path protection while fixing assertions. T0021.
- WorldMesh::append_face reserves exact next sizes for both vectors on every face. This can produce quadratic copying. Existing three-city test durations were 62.09 seconds Debug and 294.36 seconds ASan, including validation. T0022 precedes the 17-map audit.
- The renderer still draws flat quads without depth. CPU world meshes are test-only; static CPU/GPU cache behavior and performance thresholds are absent.
- Missing GPU devices produce skips in ordinary renderer tests; current Linux smoke is non-gating. Missing game data can also yield zero-assertion success in ordinary unit tests. Strict GPU and local data-enabled gates are required.
- World entry is no longer universally blocked. Local mcp-20261013-k desktop frame-010s.png visibly contains Downtown geometry, while its window counterpart is black. Capture channel, provenance, scaling and camera localization need review before measurements.
- ValidateGfxScenario.cmake does not accept the owner's supported debug object. T0024 addresses only the unprotected validator, fixtures and scenario files.
- All slope families remain evidence-gated. Flat-face meaning, wall selection, elevation, projection calibration, original UV transforms and code-63 geometry remain unresolved. Map aggregates and synthetic expectations do not establish fidelity.
- GTA2_RE concerns PC 10.5; current original captures use 9.6. Record version applicability rather than assume identical rendering behavior.
- Existing prototype simulation is historical scaffolding. Do not extend AI, vehicle physics or missions before M1 acceptance.
