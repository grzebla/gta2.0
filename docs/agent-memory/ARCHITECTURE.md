# Architecture

Inspected integration 12281ba. core provides math/types; assets parses game files and owns map/style associations; scene owns GPU-independent geometry and draw lists; render owns SDL_GPU resources; apps compose modules. scene must remain independent of sim, render and SDL.

Current runtime uses 40-byte QuadInstance records and a 2048x2048 tile atlas. Slot 1023 is an opaque white debug slot. Map rendering emits flat lids; quad shaders use constant depth and the pipeline has no world depth buffer. Map draw lists are rebuilt and instance data uploaded each frame.

T0001 supplies five unit-block faces with outward winding and lids at origin.z+1. T0005 maps texture corners for five faces and eight transforms. These are synthetic contracts; original wall orientation and GMP elevation remain unverified.

Next CPU contract: scene owns indexed position/UV meshes with u32 indices. Assemble explicitly selected faces first, then add a deterministic partial unsloped map builder with unsupported-content statistics. GPU ownership, camera projection and caching follow in separate tasks.

T0010 supplies strict local inventory and all 17 map/style associations. Inventory success does not prove geometry completeness or fidelity.

T0008 remains unapproved on its branch. Integration still has the older include-only boundary checker and stale architecture documentation; its passing check does not establish complete dependency enforcement.
