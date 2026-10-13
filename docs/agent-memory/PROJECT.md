# Project

GTA2.0 is a native C++20/CMake/SDL3/SDL_GPU reimplementation of GTA 2. Original-game fidelity must be demonstrated by automated comparisons. The project goal is fixed; only the human can declare completion.

Architecture review inspected integration 07ce6db1a5f3c3dc898b1e3b062e0fdd97de9e07. T0001–T0005 remain approved. Runtime rendering still consists of flat textured lids and prototype entity quads. CPU block faces and texture-corner mapping are implemented but not connected to the renderer.

M1 remains active. No original-game reference run or world-geometry comparison has been established. Existing district captures prove nonblank engine output only.

Freeze existing prototype simulation behavior. No new AI, vehicle physics or missions before Astra accepts M1. Claude implements bounded tasks; Astra reviews code and evidence. Only Gatekeeper merges through the orchestrator. Developers do not push.

Original assets and screenshots stay local and outside repository/CI artifacts. No aws commands, including mock invocations. EC2 access is exclusively through validated gfx_requests. Protected paths remain unchanged. Memory updates are applied by the orchestrator.
