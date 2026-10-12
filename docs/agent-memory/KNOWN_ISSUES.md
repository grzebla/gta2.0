# Known issues

- Runtime world rendering remains flat: shaders use z=0, no depth attachment exists, and instances are uploaded every frame. T0001 is a geometry foundation only.
- Original elevation, projection, drawable-face rules, flat behavior and exhaustive texture orientation are not established. Existing axis statistics and historical visual observations do not establish complete fidelity.
- gmp_map.hpp incorrectly claims original CMAP and DMAP describe the same city. M1-002 corrects this. The current automated CMAP test checks two files and unique-block counts.
- Original-data unit tests return successfully when data is missing, including through a legacy G2N_GAME_DIR fallback. A separate explicit mandatory 17-map geometry gate is still needed.
- tests/conformance/CMakeLists.txt currently writes original-data screenshots into the build tree. M1-003 must move them outside the checkout before further data-enabled screenshot runs.
- Existing district render tests check colour diversity and coverage and may skip without a GPU. They are not geometry comparisons. Vulkan smoke CI is non-gating.
- Historical documentation mentions EC2 reference capture. Current owner policy prohibits all AWS commands; use available local evidence and report concrete access blockers.
- This review checkout has no build directory or formatter on PATH. Git emits sandbox cache warnings but read-only inspection and whitespace checks complete successfully. No new verification blocker is inferred for the developer or orchestrator.
