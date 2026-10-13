# Project

GTA2.0 is a native C++20/CMake/SDL3/SDL_GPU reimplementation of GTA 2. Fidelity requires automated comparison with the original. The goal is fixed; only the human can declare completion.

Architecture review inspected integration a2e5232497124cc7784a80019ee42cdab725b14f. T0001–T0007, T0009, T0010 and T0012 are approved. T0008 and T0011 remain blocked. Preserve ready T0013 indexed CPU assembly and T0014 partial unsloped map geometry.

M1 remains active. Runtime still draws flat lids and prototype quads without world depth or static mesh caching. CPU face geometry and texture transforms are not connected to runtime rendering. The module split is suitable; dependency enforcement and architecture documentation remain incomplete pending T0008.

Original capture job 1 supplied 20 MENU frames and no usable WORLD reference. T0012 approved the investigation, not fidelity. Further capture requests await the protected runner handoff.

Next work adds renderer error handling, strict synthetic GPU assertions, configurable world projection, indexed rendering, depth/cutout tests, partial-map auditing and cache regressions. Slope families require reviewed evidence before implementation. Freeze prototype simulation behavior; no new AI, vehicle physics or missions before M1 acceptance.

Claude implements bounded tasks. Astra reviews actual code and evidence. Only Gatekeeper merges through the orchestrator. No developer pushes, AWS commands or protected-path edits. Original data and screenshots stay local and outside repository/CI artifacts. Memory updates are applied by the orchestrator.
