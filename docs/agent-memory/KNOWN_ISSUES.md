# Known issues

- T0011 remains blocked. The recorded review reproduced an integration/main conflict in tests/scripts/ec2-gpu-session.test.sh and confirmed developer merge permission was denied. Orchestrator reconciliation must preserve test isolation and neutralize inherited GFX_RESULTS_ROOT within the isolated suite. New integration CI is required.
- T0008 remains blocked after three change requests. Last reviewed branch 9e445282 accepts escaped target names, locally defined aliases and an external include path that evade enforcement. Its checker/docs changes are not in integration. Continue the existing task after orchestrator requeue; do not duplicate it.
- Capture job 1, local-20261013-084136, reports FAIL_GAME_DIALOG. Inspected frames at 6 and 118 seconds show the GTA 2 main menu, while result.json has dialogs=null. Three ENTER inputs each report two events, which does not prove the game consumed them. Root cause remains unresolved.
- Capture images are 2304x800 desktop captures, while the recorded game window is 640x480. Future measurements need a verified game-region mapping. No world localization or projection calibration is possible from the inspected menu frames.
- Original wall selection, flat-face behavior, elevation, projection, texture orientation and slope topology still lack reviewed comparison evidence.
- Runtime rendering has no world mesh, slopes, depth buffer or static geometry cache. The existing 17-map inventory validates input data only. Current architecture documentation remains stale pending T0008.
- Existing district render smoke tests establish nonblank engine output only. They do not establish M1 fidelity or gating GPU coverage across all backends.
