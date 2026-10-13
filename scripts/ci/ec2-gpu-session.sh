#!/usr/bin/env bash
# GTA20 - drive one GPU test session on the pre-provisioned EC2 G4dn host.
#
# Usage: ec2-gpu-session.sh <resolve|start|upload|run|collect|stop>
#
# Required env: AWS_REGION, GFX_BUCKET, RUN_ID
# Optional env: GFX_MODE (probe|compat|e2e|capture, default compat), GFX_CASES (default all;
#               for capture: one scenario id from tests/gfx/scenarios),
#               MAX_MINUTES (default 45, capped at 60), GFX_INSTANCE_ID (set by `resolve`),
#               RESULTS_DIR (default gfx-results), GFX_SCENARIOS_DIR (default tests/gfx/scenarios)
#
# Every value that ends up inside the remote PowerShell command is validated
# against a strict allow-list first.
set -euo pipefail

die() { echo "::error::$*" >&2; exit 1; }
log() { echo "[$(date -u +%H:%M:%S)] $*"; }

: "${AWS_REGION:?AWS_REGION is required}"
: "${GFX_BUCKET:?GFX_BUCKET is required}"
: "${RUN_ID:?RUN_ID is required}"
GFX_MODE="${GFX_MODE:-compat}"
GFX_CASES="${GFX_CASES:-all}"
MAX_MINUTES="${MAX_MINUTES:-45}"
RESULTS_DIR="${RESULTS_DIR:-gfx-results}"
PROJECT_TAG="${PROJECT_TAG:-GTA20}"

[[ "$RUN_ID" =~ ^[A-Za-z0-9-]{1,64}$ ]] || die "invalid RUN_ID"
[[ "$GFX_BUCKET" =~ ^[a-z0-9][a-z0-9.-]{1,61}[a-z0-9]$ ]] || die "invalid GFX_BUCKET"
[[ "$GFX_MODE" =~ ^(probe|compat|e2e|capture)$ ]] || die "invalid GFX_MODE"
if [[ "$GFX_MODE" == capture ]]; then [[ "$GFX_CASES" =~ ^[a-z0-9-]{1,64}$ ]] || die "capture needs one scenario id in GFX_CASES"; fi
[[ "$GFX_CASES" =~ ^(all|[a-z0-9-]+(,[a-z0-9-]+)*)$ ]] || die "invalid GFX_CASES"
[[ "$MAX_MINUTES" =~ ^[0-9]{1,3}$ ]] || die "invalid MAX_MINUTES"
(( MAX_MINUTES >= 10 && MAX_MINUTES <= 60 )) || die "MAX_MINUTES must be within 10..60"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
STATE_FILE="${RUNNER_TEMP:-/tmp}/gfx-session-${RUN_ID}.env"

save_state() { echo "$1=$2" >> "$STATE_FILE"; [[ -n "${GITHUB_ENV:-}" ]] && echo "$1=$2" >> "$GITHUB_ENV"; return 0; }
# shellcheck source=/dev/null
load_state() { [[ -f "$STATE_FILE" ]] && source "$STATE_FILE"; return 0; }

instance_id() {
  load_state
  if [[ -z "${GFX_INSTANCE_ID:-}" ]]; then
    die "GFX_INSTANCE_ID unknown - run 'resolve' first"
  fi
  [[ "$GFX_INSTANCE_ID" =~ ^i-[0-9a-f]{8,17}$ ]] || die "invalid GFX_INSTANCE_ID"
  echo "$GFX_INSTANCE_ID"
}

cmd_resolve() {
  local ids
  ids=$(aws ec2 describe-instances --region "$AWS_REGION" \
    --filters "Name=tag:Project,Values=${PROJECT_TAG}" "Name=tag:Purpose,Values=gfx-test" \
              "Name=instance-state-name,Values=pending,running,stopping,stopped" \
    --query 'Reservations[].Instances[].InstanceId' --output text)
  local count; count=$(wc -w <<<"$ids")
  [[ "$count" -eq 1 ]] || die "expected exactly one tagged gfx-test instance, found $count"
  ids="$(echo "$ids" | tr -d '[:space:]')"
  save_state GFX_INSTANCE_ID "$ids"
  log "instance: $ids"
}

cmd_start() {
  local id state; id=$(instance_id)
  state=$(aws ec2 describe-instances --region "$AWS_REGION" --instance-ids "$id" \
    --query 'Reservations[0].Instances[0].State.Name' --output text)
  log "state before start: $state"
  case "$state" in
    stopped) aws ec2 start-instances --region "$AWS_REGION" --instance-ids "$id" >/dev/null ;;
    # Workflow-level concurrency (group ec2-gpu-host) guarantees a single session,
    # so a running host is a leftover (e.g. fresh stack) and can be reused.
    running|pending) echo "::warning::instance already $state - reusing it" ;;
    stopping) aws ec2 wait instance-stopped --region "$AWS_REGION" --instance-ids "$id"
              aws ec2 start-instances --region "$AWS_REGION" --instance-ids "$id" >/dev/null ;;
    *) die "unexpected state $state" ;;
  esac
  save_state GFX_STARTED_BY_US 1
  aws ec2 wait instance-running --region "$AWS_REGION" --instance-ids "$id"
  log "running; waiting for SSM agent"
  local deadline=$(( $(date +%s) + 600 ))
  while (( $(date +%s) < deadline )); do
    local ping
    ping=$(aws ssm describe-instance-information --region "$AWS_REGION" \
      --filters "Key=InstanceIds,Values=$id" --query 'InstanceInformationList[0].PingStatus' --output text 2>/dev/null || true)
    if [[ "$ping" == "Online" ]]; then log "SSM online"; return 0; fi
    sleep 15
  done
  die "SSM agent did not come online within 10 minutes"
}

cmd_upload() {
  local dest="s3://${GFX_BUCKET}/runs/${RUN_ID}/scripts/"
  aws s3 cp --region "$AWS_REGION" --recursive --only-show-errors "$REPO_ROOT/tests/gfx/windows/" "$dest"
  aws s3 cp --region "$AWS_REGION" --only-show-errors "$REPO_ROOT/tests/gfx/compat-matrix.json" "$dest"
  # Which harness revision runs on the host (recorded in summary.json).
  local rev
  rev="$(git -C "$REPO_ROOT" rev-parse HEAD 2>/dev/null || echo unknown)"
  if [[ "$rev" != unknown && -n "$(git -C "$REPO_ROOT" status --porcelain -- tests/gfx scripts/ci 2>/dev/null)" ]]; then rev="$rev-dirty"; fi
  printf '%s\n' "$rev" > "$RUNNER_TEMP/harness-revision.txt"
  aws s3 cp --region "$AWS_REGION" --only-show-errors "$RUNNER_TEMP/harness-revision.txt" "$dest"
  # GFX_SCENARIOS_DIR: a directory of already validated scenarios (the orchestrator stages one).
  local scenarios="${GFX_SCENARIOS_DIR:-$REPO_ROOT/tests/gfx/scenarios}"
  if [[ -d "$scenarios" ]]; then
    aws s3 cp --region "$AWS_REGION" --recursive --only-show-errors "$scenarios/" "${dest}scenarios/"
  fi
  log "scripts uploaded to $dest"
}

# Exit code of the host script that asks for a reboot (Invoke-GfxCompat.ps1). The SSM agent's
# own reboot (exit 3010) hangs on this host, so the reboot goes through the EC2 API instead.
REBOOT_EXIT_CODE=194

ssm_last_ping() { # instance -> epoch seconds of the agent's last ping (0 if unknown)
  local raw
  raw=$(aws ssm describe-instance-information --region "$AWS_REGION" \
    --filters "Key=InstanceIds,Values=$1" --query 'InstanceInformationList[0].LastPingDateTime' --output text 2>/dev/null || true)
  if [[ "$raw" =~ ^[0-9]+(\.[0-9]+)?$ ]]; then echo "${raw%.*}"; return; fi
  python3 -c 'import sys,datetime; print(int(datetime.datetime.fromisoformat(sys.argv[1]).timestamp()))' "$raw" 2>/dev/null || echo 0
}

wait_ssm_after() { # instance, epoch: wait until the agent pings again after that moment
  local id="$1" since="$2" deadline=$(( $(date +%s) + ${SSM_BACK_TIMEOUT_S:-900} ))
  while (( $(date +%s) < deadline )); do
    (( $(ssm_last_ping "$id") > since )) && return 0
    sleep "${SSM_POLL_S:-15}"
  done
  return 1
}

send_and_wait() { # instance, budget seconds -> sets STATUS and CODE
  local id="$1" budget_s="$2" ps params cmd_id
  # Remote script: values below were validated above; no user-controlled free text.
  ps=$(cat <<EOF
\$ErrorActionPreference = 'Stop'
\$dir = 'C:\\GTA20\\runs\\${RUN_ID}\\scripts'
New-Item -ItemType Directory -Force -Path \$dir | Out-Null
foreach (\$m in @('AWS.Tools.S3','AWSPowerShell')) { if (Get-Module -ListAvailable -Name \$m) { Import-Module \$m; break } }
Read-S3Object -BucketName '${GFX_BUCKET}' -KeyPrefix 'runs/${RUN_ID}/scripts' -Folder \$dir -Region '${AWS_REGION}' | Out-Null
& "\$dir\\Invoke-GfxCompat.ps1" -Bucket '${GFX_BUCKET}' -Region '${AWS_REGION}' -RunId '${RUN_ID}' -Mode '${GFX_MODE}' -Cases '${GFX_CASES}'
exit \$LASTEXITCODE
EOF
)
  params=$(jq -n --arg c "$ps" --arg t "$(( budget_s - 120 ))" '{commands: [$c], executionTimeout: [$t]}')
  cmd_id=$(aws ssm send-command --region "$AWS_REGION" \
    --document-name AWS-RunPowerShellScript \
    --instance-ids "$id" \
    --comment "GTA20 gfx ${GFX_MODE} ${RUN_ID}" \
    --timeout-seconds 600 \
    --parameters "$params" \
    --output-s3-bucket-name "$GFX_BUCKET" \
    --output-s3-key-prefix "ssm-output/${RUN_ID}" \
    --query 'Command.CommandId' --output text)
  save_state GFX_COMMAND_ID "$cmd_id"
  log "SSM command $cmd_id sent"
  local deadline=$(( $(date +%s) + budget_s - 60 ))
  STATUS="Pending"
  while (( $(date +%s) < deadline )); do
    STATUS=$(aws ssm get-command-invocation --region "$AWS_REGION" --command-id "$cmd_id" --instance-id "$id" \
      --query 'Status' --output text 2>/dev/null || echo "Pending")
    case "$STATUS" in
      Pending|InProgress|Delayed) sleep "${SSM_POLL_S:-20}" ;;
      *) break ;;
    esac
  done
  if [[ "$STATUS" =~ ^(Pending|InProgress|Delayed)$ ]]; then
    aws ssm cancel-command --region "$AWS_REGION" --command-id "$cmd_id" >/dev/null || true
    die "SSM command exceeded the ${MAX_MINUTES}-minute budget and was cancelled"
  fi
  CODE=$(aws ssm get-command-invocation --region "$AWS_REGION" --command-id "$cmd_id" --instance-id "$id" \
    --query 'ResponseCode' --output text 2>/dev/null || echo "")
  aws ssm get-command-invocation --region "$AWS_REGION" --command-id "$cmd_id" --instance-id "$id" \
    --query '{Status:Status,ResponseCode:ResponseCode,Stdout:StandardOutputContent,Stderr:StandardErrorContent}' --output json || true
}

cmd_run() {
  local id; id=$(instance_id)
  local session_end=$(( $(date +%s) + MAX_MINUTES * 60 ))
  local attempt left before
  for attempt in 1 2 3 4; do
    left=$(( session_end - $(date +%s) ))
    (( left > 180 )) || die "session budget exhausted before the command could finish"
    send_and_wait "$id" "$left"
    if [[ "$STATUS" == "Failed" && "$CODE" == "$REBOOT_EXIT_CODE" ]]; then
      (( attempt < 4 )) || die "host still asks for a reboot after 3 reboots"
      before=$(date +%s)
      log "host asked for a reboot (attempt $attempt) - rebooting through the EC2 API"
      aws ec2 reboot-instances --region "$AWS_REGION" --instance-ids "$id"
      sleep "${REBOOT_SETTLE_S:-60}"
      wait_ssm_after "$id" "$before" || die "SSM agent did not come back after the reboot"
      continue
    fi
    [[ "$STATUS" == "Success" ]] || die "SSM command finished with status $STATUS (code $CODE)"
    return 0
  done
}

cmd_collect() {
  mkdir -p "$RESULTS_DIR"
  aws s3 cp --region "$AWS_REGION" --recursive --only-show-errors \
    "s3://${GFX_BUCKET}/runs/${RUN_ID}/results/" "$RESULTS_DIR/" || log "no results uploaded"
  aws s3 cp --region "$AWS_REGION" --recursive --only-show-errors \
    "s3://${GFX_BUCKET}/ssm-output/${RUN_ID}/" "$RESULTS_DIR/ssm-output/" || true
  if [[ -f "$RESULTS_DIR/summary.json" ]]; then
    jq -r '"overall: \(.overall)", (.cases[]? | "  \(.id): \(.verdict)")' "$RESULTS_DIR/summary.json"
  fi
}

cmd_stop() {
  load_state
  local id="${GFX_INSTANCE_ID:-}"
  if [[ -z "$id" ]]; then cmd_resolve; load_state; id="$GFX_INSTANCE_ID"; fi
  [[ "$id" =~ ^i-[0-9a-f]{8,17}$ ]] || die "invalid instance id"
  local state
  state=$(aws ec2 describe-instances --region "$AWS_REGION" --instance-ids "$id" \
    --query 'Reservations[0].Instances[0].State.Name' --output text)
  if [[ "$state" == "running" || "$state" == "pending" ]]; then
    aws ec2 stop-instances --region "$AWS_REGION" --instance-ids "$id" >/dev/null
    log "stop requested for $id"
    # Windows on this host has been seen hanging late in shutdown (after the kernel shutdown
    # transition). After STOP_FORCE_AFTER_S in 'stopping', force the stop.
    local waited=0 limit="${STOP_FORCE_AFTER_S:-240}"
    while (( waited < limit )); do
      state=$(aws ec2 describe-instances --region "$AWS_REGION" --instance-ids "$id" \
        --query 'Reservations[0].Instances[0].State.Name' --output text)
      [[ "$state" == "stopped" ]] && { log "stopped"; return 0; }
      sleep "${STOP_POLL_S:-15}"; waited=$(( waited + ${STOP_POLL_S:-15} ))
    done
    log "still '$state' after ${limit}s - forcing the stop"
    aws ec2 stop-instances --region "$AWS_REGION" --instance-ids "$id" --force >/dev/null
    aws ec2 wait instance-stopped --region "$AWS_REGION" --instance-ids "$id" || log "still stopping; reaper/watchdog remain as backstops"
  else
    log "instance $id is $state - nothing to stop"
  fi
}

case "${1:-}" in
  resolve|start|upload|run|collect|stop) "cmd_$1" ;;
  *) die "usage: $0 <resolve|start|upload|run|collect|stop>" ;;
esac
