# TEST STATUS

| Suite | State (2026-10-12) |
|---|---|
| CTest Debug, macOS ARM64, with original data | 54/54 |
| CTest without data (data tests skip) | 54/54 |
| ASan/UBSan with data | 51/51 |
| `[gamedata]` (17 maps, 4 styles, corruption, 9.6 vs 10.5) | 9 cases, 448 880 assertions |
| Render check wil/ste/bil (offscreen, >=500 colours, coverage >=0.95) | 3/3 |
| native-ci (macOS/Windows/Linux Release+Debug+ASan, format, tidy, shaders, boundaries, lavapipe smoke) | green on PR #3 head |
| Orchestrator unit/simulation/recovery tests | 30/30 |
| Comparison with the original game | none yet |
