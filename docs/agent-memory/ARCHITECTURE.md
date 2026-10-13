# Architecture

T0013's scene-owned WorldMesh is approved. It stores position/atlas-UV vertices and u32 indices and reuses the existing geometry, texture-corner and atlas helpers. It performs explicit synthetic face assembly only, with no map-face selection or rendering integration.
