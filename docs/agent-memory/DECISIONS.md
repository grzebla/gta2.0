# Decisions

Append-only log written by the orchestrator from Astra's recorded decisions.

## 2026-10-12 — Owner decisions (seed)

- Native C++20/SDL3/SDL_GPU engine; OpenGTA2 (C#) direction abandoned.
- Project renamed to GTA2.0 (`grzebla/gta2.0`, namespace `gta20`).
- Development supervised by Astra (gpt-6-astra via Codex CLI), implemented by Claude Code, run by the orchestrator. First milestone: M1 world geometry.

## 2026-10-12T21:25:28Z — Astra plan

Verified HEAD 8f820b7806946cbc5a4663e5d4a24eab60e69c1c. Scene generation emits flat quads, vertex shaders use z=0, and the renderer has no depth attachment. The headless preset already builds scene doctests without SDL. Module-boundary checks passed; no build directory or test logs were available to rerun the reported baseline. Create M1 and issue one small READY task for the owner's trial iteration. The roadmap below orders the remaining milestone work for later promotion. Also record that the current Vulkan smoke job is non-gating and that the GmpMap header's CMAP-equivalence comment contradicts existing tests and format documentation.

New tasks: T0001
