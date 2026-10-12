#!/usr/bin/env bash
# Runs one gfx-compat session from your machine with your AWS CLI credentials.
# Works WITHOUT GitHub OIDC / advanced features. The host is stopped on exit,
# error, Ctrl-C or timeout (trap), on top of the on-instance watchdog and the reaper.
# Usage: scripts/aws/run-gfx-compat-local.sh <probe|compat> [cases=all] [max_minutes=45]
# shellcheck source=common.sh
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

export GFX_MODE="${1:?usage: $0 <probe|compat> [cases] [max_minutes]}"
export GFX_CASES="${2:-all}"
export MAX_MINUTES="${3:-45}"
RUN_ID="local-$(date -u +%Y%m%d-%H%M%S)"
export RUN_ID
export RESULTS_DIR="$REPO_ROOT/gfx-results/$RUN_ID"
export RUNNER_TEMP="${TMPDIR:-/tmp}"
GFX_BUCKET="$(stack_output "$GFX_STACK" ArtifactsBucketName)"
export GFX_BUCKET
[[ -n "$GFX_BUCKET" && "$GFX_BUCKET" != "None" ]] || die "stack $GFX_STACK not found"

session="$REPO_ROOT/scripts/ci/ec2-gpu-session.sh"
confirm "uruchom-ec2" "gfx-compat '$GFX_MODE' (cases: $GFX_CASES), budget $MAX_MINUTES min.
COST: ~0.75 USD/h while the host runs (first bootstrap adds driver install + reboots)."

stopped=0
cleanup() {
  if [[ $stopped -eq 0 ]]; then
    stopped=1
    log "stopping host (trap)"
    bash "$session" stop || log "WARNING: stop failed - check the console; watchdog/reaper will still stop it"
  fi
}
trap cleanup EXIT INT TERM

bash "$session" resolve
bash "$session" start
bash "$session" upload
status=0
bash "$session" run || status=$?
bash "$session" collect || true
cleanup
log "results: $RESULTS_DIR (exit $status)"
exit $status
