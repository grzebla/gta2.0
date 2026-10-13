# Project

GTA2.0 is a native C++20/CMake/SDL3/SDL_GPU reimplementation of GTA 2. Fidelity requires automated comparisons with the original. The goal is fixed; only the human can declare completion.

Planning review inspected integration 12281baf3eecab74977cc905a7890ed019987656. T0001-T0007, T0009 and T0010 are approved. T0008 and T0011 remain blocked. Runtime rendering still uses flat textured lids and prototype entity quads. CPU face geometry and texture-corner mapping are not connected to the renderer.

M1 remains active. Original-game capture job 1 completed, but world entry is unverified. The inspected first and last frames show the main menu. No usable world reference or geometry comparison is established.

Next work is an offline capture audit, indexed CPU face assembly, then a partial unsloped map mesh. Freeze prototype simulation behavior. No new AI, vehicle physics or missions before Astra accepts M1.

Claude implements bounded tasks; Astra reviews code and evidence. Only Gatekeeper merges through the orchestrator. Developers do not push. Original assets and screenshots stay local and outside repository/CI artifacts. No aws commands, including mocks. Host access is exclusively through gfx_requests. Protected paths remain unchanged; the orchestrator applies memory updates.
