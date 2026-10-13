# Known Issues

T0011 is blocked at ba732b0. Independently reproduced the integration/main conflict in tests/scripts/ec2-gpu-session.test.sh. Run settings deny git merge, and the transcript confirms denial. Orchestrator intervention is required. Main's GFX_RESULTS_ROOT override must also be neutralized in the isolated test suite when resolving the conflict. Retain existing issues.
