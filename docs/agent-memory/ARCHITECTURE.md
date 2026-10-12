# Architecture

engine/scene provides GPU-independent unit-block geometry for the lid and four vertical walls. Its local mathematical contract does not establish original GTA 2 elevation semantics. docs/specs/world-geometry.md separates implementation facts, hypotheses and original-game evidence.

The runtime still uses 2D QuadInstance data and flat lid rendering. Quad shaders set z=0; renderer.cpp supplies no depth attachment and uploads instances per frame. CPU face texture-corner mapping is the next bounded addition, without runtime integration.

Original-data render smoke tests use GTA20_RENDER_CAPTURE_DIR with a runtime override and per-user cache default. CMake rejects checkout/build destinations through existing symlinks and creates separate timestamp/district/random run directories. These tests establish nonblank output, not fidelity.
