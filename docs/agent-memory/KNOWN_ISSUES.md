# KNOWN ISSUES AND LIMITS

- Rendering is top-down lids only; no walls, slopes, lighting, animated tiles or sprites.
- Lid lighting factors (bits 10-11) unknown; guessed values distorted the image, so none applied.
- Movement/collision rules in `sim` are synthetic prototypes, not GTA 2 behaviour.
- Windows window/D3D12 and Linux windowed mode were built in CI but not run interactively.
- Original-game comparison harness on EC2 is prepared but never run (needs owner approval: cost).
- GitHub OIDC for AWS needs "advanced features" in AWS (SCP blocks iam:*Provider*).
- Shaders are maintained by hand in three languages (consider SDL_shadercross later).
- CI minutes on the private repo are limited (macOS jobs cost 10x): push integration sparingly.
