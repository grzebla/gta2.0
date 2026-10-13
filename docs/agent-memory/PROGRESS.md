# Progress

Append-only log of approved work (written by the orchestrator).

## 2026-10-12 — Baseline before the orchestrator

Engine skeleton, CI on 3 platforms, GMP/STY parsers verified on original data, textured lids of the three cities on Metal, rename to GTA2.0, orchestrator.

## 2026-10-12T21:36:48Z — T0001 Add GPU-independent unit-block face geometry

Approved T0001 at 76ef029: added GPU-independent unit-block lid and wall geometry with an explicit local coordinate contract and outward winding. Five synthetic geometry tests cover corners, area, translation, seams and repeatability. Independent orchestrator verification passed asset-free headless tests, pinned formatting and data-enabled Debug tests. M1 remains incomplete.

## 2026-10-12T22:46:42Z — T0002 Document unsloped world-geometry conventions and evidence gaps

Approved T0002 at 78fa72c: documented unsloped geometry conventions, evidence limits, five-face texture proposals, projection measurements and nine future local comparison scenarios; corrected the CMAP test-scope comment. Previous review findings are resolved. Runtime behavior is unchanged, local diagnostic checks pass, and independent orchestrator verification passes. Original-game geometry fidelity and M1 remain unproven.

## 2026-10-12T22:57:49Z — T0003 Keep original-data render captures outside the checkout

Approved T0003 at a16ba67: district render smoke tests now retain original-data screenshots in validated external directories with independent run names. Added 12 asset-free path tests and documented local capture use. Inspected logs confirm 68/68 headless and 71/71 data-enabled Debug tests passed; all three district captures exist outside the checkout and CI artifact paths. M1 fidelity remains unproven.

## 2026-10-13T05:54:14Z — T0004 Isolate offline shell-test outputs and cleanup

Approved T0004 at d78585c: offline shell tests now use disposable repository copies with validated external temporary paths and guarded cleanup. All 33 original assertions remain unchanged. Inspected execution logs confirm 60/60 isolation self-tests, 68/68 headless tests and 71/71 Debug tests passed. The full mock suite was not run under fixed policy. M1 remains incomplete.

## 2026-10-13T06:01:47Z — T0005 Add CPU face texture-corner mapping

Approved T0005 at e41b6ae: added GPU-independent texture-corner mapping for five block faces, with 40 explicit synthetic expectations and eight regressions against existing lid drawing. Inspected logs confirm 72/72 headless and 75/75 Debug tests passing, including external district captures. Runtime rendering is unchanged; wall orientation and original-game fidelity remain unverified. M1 remains incomplete.
