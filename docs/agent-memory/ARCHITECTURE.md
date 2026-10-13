# Architecture

Retain the approved WorldMesh and T0015 renderer contracts. T0014 adds scene::build_unsloped_map_mesh with deterministic full-map traversal and fixed-size coverage statistics. Slopes are skipped whole; unsloped faces use exclusive flat/blank/reserved/out-of-style/emitted classification. Coverage is partial or provisional, never complete M1. No renderer integration was added.
