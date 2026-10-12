# Architecture

Scene depends on core/assets and remains GPU-independent. BlockFace, Position3f, FaceGeometry and block_face_geometry() provide translated unit-block faces with outward winding. Their lid at origin.z+1 does not establish original-game elevation.

Runtime scene generation still emits QuadInstance records through DrawList. QuadInstance is 40 bytes. Textured map rendering loops through levels but assigns no geometric elevation; shaders use z=0. Camera2D is orthographic. Renderer::encode has no depth attachment and uploads instances each frame.

GMP collision flags, flat flags, drawable-face selection, texture-local axes and level-to-world elevation require an evidence-backed contract before integration. Keep static indexed world geometry separate from the existing dynamic quad path when that design is promoted.

Original-data render tests currently pass a build-tree screenshot path. M1-003 will move these captures outside the checkout without altering image thresholds.
