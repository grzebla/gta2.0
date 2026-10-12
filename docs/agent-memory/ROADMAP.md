# ROADMAP

## M1 — Full GTA 2 world geometry (current)
Scope: vertical block walls; wall texturing; slopes (gradient, diagonal, 3-sided); multi-level
geometry; building heights; depth testing / occlusion; correct texture orientation;
geometry buffering/caching and optimisation; tests on all 17 maps; local screenshots of the three
districts; benchmarks and regression tests.
Exit: all 17 maps build geometry without errors, the three districts render with walls and
slopes and correct occlusion (visual review), frame-time benchmark recorded, CI green.
No AI, vehicle physics or missions before M1 is accepted.

## Later (order refined by Astra by real dependencies)
Sprites and animated textures; world objects; player character and controls; world collisions;
vehicles; car physics; pedestrians; AI; police and wanted level; gangs and respect; weapons and
damage; missions and scripts; UI/HUD; audio; save state; remaining mechanics; optimisation and
platform parity; full-gameplay regression tests; behavioural comparison with the original
(EC2 G4dn harness, 4 vCPU quota granted; paid runs need the owner's approval); stable release.
