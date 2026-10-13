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

## 2026-10-13T06:16:04Z — T0006 Reject failed or stale conformance executions

Approved T0006 at 3b9d895. The conformance wrapper now validates both execution statuses, rejects unsupported modes and clears prior outputs. Six synthetic wrapper tests cover failure, stale-output, missing-output and success cases. Inspected logs confirm 12/12 focused conformance, 78/78 headless and 81/81 Debug passes. Existing goldens remain unchanged; M1 remains incomplete.

## 2026-10-13T06:23:20Z — T0007 Validate trace integrity before comparison

Approved T0007 at 2fca90f. Trace parsing and comparison now reject empty or structurally invalid traces and unknown provenance with explicit diagnostics. Regression tests reproduce the former false passes and verify rejection while preserving sparse schedules, cross-adapter comparisons, tolerances and goldens. Inspected logs confirm 83/83 headless, 86/86 Debug and required ASan/UBSan testing unit passes. M1 remains incomplete.

## 2026-10-13T06:34:54Z — T0009 Prepare the first original-game geometry capture scenario

Approved T0009 at 519e9aa: committed the first bounded original-game discovery scenario, companion interpretation and camera-localization procedure, and offline validation with 37 synthetic cases. Local checks passed; inspected logs confirm 121/121 headless and 124/124 Debug passes. Requested one discovery capture. M1 remains incomplete.

## 2026-10-13T07:44:19Z — T0010 Add a strict local map inventory gate

Approved T0010 at c4c0c8b. The strict local inventory validates all 17 maps and their sourced style associations, reports bounded aggregates and rejects missing data, invalid arguments and case collisions. Verified 150/150 headless and 153/153 Debug passes plus a direct successful original-data inventory. M1 remains incomplete.

## 2026-10-13T09:24:47Z — T0012 Classify the discovery capture and document the blocked world entry

Approved T0012 at 1584314. Independently confirmed all 20 discovery frames show an unchanged MENU region, with no usable WORLD reference. The report separates observed pixels, runner values and unresolved input behavior, and hands protected runner diagnostics to the owner. Required checks and inspected 150/150 headless and 153/153 Debug results pass. No further capture requested; M1 remains incomplete.
