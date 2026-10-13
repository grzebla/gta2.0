#!/usr/bin/env bash
# Offline tests for scripts/ci/ec2-gpu-session.sh. A fake `aws` binary records every call;
# nothing reaches AWS. Run: bash tests/scripts/ec2-gpu-session.test.sh
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SCRIPT="$ROOT/scripts/ci/ec2-gpu-session.sh"
WORK="$(mktemp -d)"
export STOP_FORCE_AFTER_S="${STOP_FORCE_AFTER_S:-2}" STOP_POLL_S="${STOP_POLL_S:-1}"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/bin"

cat > "$WORK/bin/aws" <<'FAKE'
#!/usr/bin/env bash
printf '%s\n' "aws $*" >> "$FAKE_LOG"
case "$1 $2" in
  "ec2 describe-instances")
    if [[ "$*" == *"--filters"* ]]; then printf "%s\n" "${FAKE_IDS-i-0123456789abcdef0}"; else printf '%s\n' "${FAKE_STATE:-stopped}"; fi ;;
  "ssm describe-instance-information")
    if [[ "$*" == *"LastPingDateTime"* ]]; then date +%s; else echo Online; fi ;;
  "ssm send-command") echo "cmd-123" ;;
  "cloudformation describe-stacks") echo "gta20-gfx-artifacts-433213574600" ;;
  "service-quotas get-service-quota") echo "${FAKE_QUOTA:-0.0}" ;;
  "ec2 describe-vpcs") echo "vpc-012515c9ad46fee92" ;;
  "ec2 describe-subnets") echo "subnet-01ca4e08a6f9fa5b3" ;;
  "ssm get-command-invocation")
    if [[ "$*" == *"--query Status "* ]]; then echo "${FAKE_CMD_STATUS:-Success}"
    elif [[ "$*" == *"--query ResponseCode "* ]]; then echo "${FAKE_CMD_CODE:-0}"
    else echo '{"Status":"Success"}'; fi ;;
esac
exit 0
FAKE
chmod +x "$WORK/bin/aws"

pass=0; fail=0
check() { # name, expected exit code, command...
  local name="$1" expected="$2"; shift 2
  : > "$WORK/aws.log"; rm -f "$WORK"/gfx-session-*.env
  local out code
  out=$("$@" 2>&1); code=$?
  if [[ "$code" == "$expected" ]]; then pass=$((pass + 1)); echo "ok   - $name"
  else fail=$((fail + 1)); echo "FAIL - $name (exit $code, expected $expected)"; printf '       %s\n' "$out"; fi
}
assert() { # name, condition command...
  local name="$1"; shift
  if "$@"; then pass=$((pass + 1)); echo "ok   - $name"
  else fail=$((fail + 1)); echo "FAIL - $name"; fi
}
both() { "$1" "$2" && "$3" "$4"; }
log_has() { grep -q -- "$1" "$WORK/aws.log"; }
log_lacks() { ! grep -q -- "$1" "$WORK/aws.log"; }

export PATH="$WORK/bin:$PATH" FAKE_LOG="$WORK/aws.log" RUNNER_TEMP="$WORK" \
       AWS_REGION=eu-north-1 GFX_BUCKET=gta20-gfx-artifacts-433213574600 RUN_ID=123-1
unset GITHUB_ENV

run() { bash "$SCRIPT" "$@"; }

check "resolve finds exactly one tagged host" 0 run resolve
check "resolve refuses zero hosts" 1 env FAKE_IDS="" bash "$SCRIPT" resolve
check "resolve refuses two hosts" 1 env FAKE_IDS="i-0123456789abcdef0 i-0fedcba9876543210" bash "$SCRIPT" resolve

check "start starts a stopped host and waits for SSM" 0 bash -c "bash '$SCRIPT' resolve && bash '$SCRIPT' start"
assert "start called StartInstances + SSM ping" both log_has "ec2 start-instances" log_has "ssm describe-instance-information"

check "start reuses an already running host" 0 bash -c "bash '$SCRIPT' resolve && FAKE_STATE=running bash '$SCRIPT' start"
assert "running host not started twice" log_lacks "ec2 start-instances"

check "run sends one PowerShell command" 0 bash -c "bash '$SCRIPT' resolve && bash '$SCRIPT' run"
assert "run used AWS-RunPowerShellScript" log_has "AWS-RunPowerShellScript"

check "run fails when SSM reports Failed" 1 bash -c "bash '$SCRIPT' resolve && FAKE_CMD_STATUS=Failed bash '$SCRIPT' run"
check "stop stops a running host" 0 env FAKE_STATE=running bash "$SCRIPT" stop
assert "stop called StopInstances" log_has "ec2 stop-instances"
check "stop is a no-op on a stopped host" 0 env FAKE_STATE=stopped bash "$SCRIPT" stop
assert "stopped host left alone" log_lacks "ec2 stop-instances"
check "stop forces a host that hangs in shutdown" 0 env FAKE_STATE=running STOP_FORCE_AFTER_S=2 STOP_POLL_S=1 bash "$SCRIPT" stop
assert "forced StopInstances after the timeout" log_has "ec2 stop-instances --region eu-north-1 --instance-ids i-0123456789abcdef0 --force"
check "run reboots through EC2 when the host asks (exit 194), max 3 times" 1 bash -c "bash '$SCRIPT' resolve && FAKE_CMD_STATUS=Failed FAKE_CMD_CODE=194 REBOOT_SETTLE_S=0 SSM_POLL_S=0 bash '$SCRIPT' run"
assert "rebooted through the EC2 API" log_has "ec2 reboot-instances --region eu-north-1 --instance-ids i-0123456789abcdef0"
assert "exactly three reboots" test "$(grep -c 'ec2 reboot-instances' "$WORK/aws.log")" -eq 3

# Injection / bounds guards: nothing may reach AWS.
check "rejects shell/PowerShell metacharacters in GFX_CASES" 1 env GFX_CASES="all'; Remove-Item C:\\ -Recurse #" bash "$SCRIPT" run
assert "no command sent for bad GFX_CASES" log_lacks "send-command"
check "rejects MAX_MINUTES above 60" 1 env MAX_MINUTES=61 bash "$SCRIPT" run
check "rejects MAX_MINUTES below 10" 1 env MAX_MINUTES=5 bash "$SCRIPT" run
check "rejects unknown mode" 1 env GFX_MODE=deploy bash "$SCRIPT" run
check "capture needs exactly one scenario id" 1 env GFX_MODE=capture GFX_CASES=a,b bash "$SCRIPT" resolve
check "capture accepts one scenario id" 0 env GFX_MODE=capture GFX_CASES=frontend-walk bash "$SCRIPT" resolve
mkdir -p "$WORK/staged"; echo '{}' > "$WORK/staged/frontend-walk.json"
check "upload sends the staged scenario directory" 0 env GFX_MODE=capture GFX_CASES=frontend-walk GFX_SCENARIOS_DIR="$WORK/staged" bash "$SCRIPT" upload
assert "staged scenarios go next to the host scripts" log_has "$WORK/staged/ s3://gta20-gfx-artifacts-433213574600/runs/123-1/scripts/scenarios/"
check "rejects bad RUN_ID" 1 env RUN_ID='1;rm -rf /' bash "$SCRIPT" resolve
check "rejects bad bucket" 1 env GFX_BUCKET='Bad_Bucket' bash "$SCRIPT" resolve
check "rejects unknown subcommand" 1 run terminate
assert "never terminates or launches instances" both log_lacks "terminate-instances" log_lacks "run-instances"

# Operator scripts (scripts/aws): confirmation gates, quota gate, stop-on-exit trap.
LOCAL="$ROOT/scripts/aws/run-gfx-compat-local.sh"
DEPLOY="$ROOT/scripts/aws/deploy-gfx-stack.sh"
check "local run without confirmation changes nothing" 1 env CONFIRM=no bash "$LOCAL" probe
assert "no start/stop/send without confirmation" both log_lacks "start-instances" log_lacks "send-command"
check "local run stops the host even when the test fails" 1 env CONFIRM=uruchom-ec2 FAKE_STATE=running FAKE_CMD_STATUS=Failed bash "$LOCAL" probe
assert "trap called StopInstances after the failure" log_has "ec2 stop-instances"
check "deploy refuses while the GPU quota is 0" 1 env CONFIRM=uruchom-ec2 FAKE_QUOTA=0.0 bash "$DEPLOY"
assert "no CloudFormation deploy without quota" log_lacks "cloudformation deploy"
check "deploy without confirmation changes nothing" 1 env CONFIRM=no FAKE_QUOTA=4.0 bash "$DEPLOY"
assert "no CloudFormation deploy without confirmation" log_lacks "cloudformation deploy"
check "deploy runs after quota + confirmation" 0 env CONFIRM=uruchom-ec2 FAKE_QUOTA=4.0 bash "$DEPLOY"
assert "deploy used the gfx template in eu-north-1" both log_has "cloudformation deploy --stack-name gta20-gfx" log_has "SubnetId=subnet-01ca4e08a6f9fa5b3"
rm -rf "$ROOT/gfx-results"/local-* 2>/dev/null

echo "---"
echo "passed: $pass, failed: $fail"
[[ "$fail" -eq 0 ]]
