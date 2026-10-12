# KNOWN ISSUES

- Current textured rendering uses flat lid quads at GPU z=0, with no geometric walls, slopes or depth testing.
- Renderer alpha blending currently has no depth interaction. Transparent tile texels must not occlude lower geometry when depth is introduced.
- Existing visual lid-orientation observations do not establish all wall/slope UV conventions or automated original-game fidelity.
- GMP flat-flag rendering semantics, map elevation convention, original projection and full slope topology need explicit evidence before implementation claims.
- Lid 1023 is usually a three-sided-slope marker but has exceptions. Bonus maps use reserved tiles 992–1022. Avoid silently dropping required geometry or sampling atlas slot 1023 as a marker texture.
- parse_gmp's header comment says CMAP and DMAP describe the same city; docs/formats/gmp.md and the existing gamedata test explicitly say they differ. Use DMAP by default and correct the comment during the conventions task.
- Lid lighting factors remain unknown and unapplied.
- Prototype movement/collision rules are not GTA 2 behaviour.
- native-ci's Vulkan render-smoke-linux job has continue-on-error: true. Green CI alone therefore does not establish working Vulkan rendering.
- Interactive Windows/D3D12 and Linux rendering remain unverified in supplied evidence. No original-game comparison results were supplied.
- This checkout has no build directory or test logs available for independent baseline execution. Historical pass counts are reported evidence only.
- Three shader languages require coordinated maintenance. CI minutes are limited; integration and remote runs belong to the orchestrator.
- No AWS commands or cloud changes are authorised. Existing infrastructure documentation grants no exception to owner policy.
