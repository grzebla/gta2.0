# Architecture

engine/scene now provides face_texture_corners alongside unit-block geometry. It returns A-D labels at p0-p3 for five faces, four rotations and both flip states, with horizontal mirroring before clockwise rotation. It has no GPU dependencies or runtime integration. Lid expectations encode current-engine regression behavior; wall expectations remain hypotheses. QuadInstance, geometry, shaders, camera and atlas behavior are unchanged. Original-data render smoke captures remain in validated external directories and establish nonblank output only.
