# Architecture

Original-data render smoke tests use GTA20_RENDER_CAPTURE_DIR, with a runtime environment override and per-user cache default. CMake validates checkout/build exclusion through existing symlinks before rendering and creates separate timestamp/district/random run directories. Renderer architecture and image thresholds are unchanged.
