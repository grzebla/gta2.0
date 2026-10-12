# Known issues

- Confirmed Gatekeeper blocker at integration 4b6a42a: tests/scripts/ec2-gpu-session.test.sh:98 deletes checkout gfx-results/local-* directories, potentially removing operator results. M1-004 must isolate all outputs and cleanup. Do not run the current suite.
- The full legacy offline suite invokes an aws mock and protected operator scripts. Agent execution remains prohibited by fixed policy; the queued isolation task requires an AWS-free self-test.
- Original-game geometry comparisons remain unestablished. Projection, elevation, wall selection, wall texture orientation and flat/bullet-wall behavior are unresolved. Existing engine captures are not original-game references.
- A usable local original-game capture environment and reproducible scene positioning remain unestablished.
- draw_list.cpp cites sty.md for rotation despite that document containing no rotation evidence. M1-005 includes the citation correction.
- Runtime geometry still lacks walls, slopes, depth testing and static world-mesh caching.
- Original-data capture destination handling is resolved by T0003. Its district screenshots remain local and external.
