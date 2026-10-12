# Progress

Append-only log of approved work (written by the orchestrator).

## 2026-10-12 — Baseline before the orchestrator

Engine skeleton, CI on 3 platforms, GMP/STY parsers verified on original data, textured lids of the three cities on Metal, rename to GTA2.0, orchestrator.

## 2026-10-12T21:36:48Z — T0001 Add GPU-independent unit-block face geometry

Approved T0001 at 76ef029: added GPU-independent unit-block lid and wall geometry with an explicit local coordinate contract and outward winding. Five synthetic geometry tests cover corners, area, translation, seams and repeatability. Independent orchestrator verification passed asset-free headless tests, pinned formatting and data-enabled Debug tests. M1 remains incomplete.

## 2026-10-12T22:46:42Z — T0002 Document unsloped world-geometry conventions and evidence gaps

Approved T0002 at 78fa72c: documented unsloped geometry conventions, evidence limits, five-face texture proposals, projection measurements and nine future local comparison scenarios; corrected the CMAP test-scope comment. Previous review findings are resolved. Runtime behavior is unchanged, local diagnostic checks pass, and independent orchestrator verification passes. Original-game geometry fidelity and M1 remain unproven.
