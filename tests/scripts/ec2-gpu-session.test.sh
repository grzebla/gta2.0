#!/usr/bin/env bash
# Offline tests for scripts/ci/ec2-gpu-session.sh. A fake `aws` binary records every call;
# nothing reaches AWS. Run: bash tests/scripts/ec2-gpu-session.test.sh
#
# Every invocation copies the scripts under test, unchanged, into a fresh temporary workspace
# (same relative layout as the checkout) and runs them from there. Results, state files and logs
# stay inside that workspace, and cleanup only ever removes the workspace it allocated; the
# checkout is only read. The temporary base (TMPDIR, default /tmp) is resolved to a physical
# path first; a base inside the checkout, directly or through a symlink, is refused before
# anything is written.
#
# AWS-free check of that isolation (no aws, no mock, no operator scripts; synthetic fixtures):
#   bash tests/scripts/ec2-gpu-session.test.sh --workspace-self-test
set -uo pipefail

ROOT="$(cd -P "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)"
SELF="$ROOT/tests/scripts/$(basename "${BASH_SOURCE[0]}")"
WORKSPACE_PREFIX="gta20-ec2-gpu-test."
# The only checkout files the suite needs; relative paths are kept so the scripts resolve
# REPO_ROOT (and therefore gfx-results/) to the workspace copy.
FIXTURE_INPUTS=(
  scripts/ci/ec2-gpu-session.sh
  scripts/aws/common.sh
  scripts/aws/run-gfx-compat-local.sh
  scripts/aws/deploy-gfx-stack.sh
)

# Prints the absolute physical path of a temporary base. Refuses the checkout and any extra
# protected tree, including symlinked aliases of them.
tmp_base_resolve() { # candidate, extra protected trees...
  local cand="$1" phys tree tree_phys
  shift
  if [[ -z "$cand" ]] || ! phys="$(cd -P -- "$cand" 2>/dev/null && pwd -P)"; then
    echo "error: temporary base '$cand' is not an accessible directory" >&2
    return 1
  fi
  for tree in "$ROOT" "$@"; do
    tree_phys="$(cd -P -- "$tree" 2>/dev/null && pwd -P)" || tree_phys="$tree"
    if [[ "$phys" == "$tree_phys" || "$phys" == "${tree_phys%/}"/* ]]; then
      echo "error: temporary base '$cand' (resolves to '$phys') is inside '$tree_phys'" >&2
      return 1
    fi
  done
  printf '%s\n' "$phys"
}
# Resolved before anything is written; TMPDIR is later redirected into the workspace.
TEST_TMP_BASE="$(tmp_base_resolve "${TMPDIR:-/tmp}")" || exit 1

workspace_owned() { # path; true only for a workspace allocated by workspace_create
  case "$1" in
    "$TEST_TMP_BASE/$WORKSPACE_PREFIX"??????) ;;
    *) return 1 ;;
  esac
  [[ -d "$1" && ! -L "$1" && "$(cat "$1/.workspace-owner" 2>/dev/null)" == "$1" ]]
}
workspace_create() { # source root -> prints the new workspace path
  local src="$1" base ws rel
  base="$(tmp_base_resolve "$TEST_TMP_BASE" "$src")" || return 1
  if [[ "$base" != "$TEST_TMP_BASE" ]]; then
    echo "error: temporary base '$TEST_TMP_BASE' is not a physical path" >&2
    return 1
  fi
  if ! ws="$(mktemp -d "$base/${WORKSPACE_PREFIX}XXXXXX")" || [[ ! -d "$ws" || -L "$ws" ]]; then
    echo "error: cannot allocate a test workspace in $base" >&2
    return 1
  fi
  if ! printf '%s\n' "$ws" > "$ws/.workspace-owner"; then
    rm -f -- "$ws/.workspace-owner"
    rmdir -- "$ws"
    echo "error: cannot mark test workspace $ws" >&2
    return 1
  fi
  if ! mkdir -- "$ws/bin" "$ws/tmp" "$ws/repo"; then
    echo "error: cannot lay out test workspace $ws" >&2
    workspace_cleanup "$ws"
    return 1
  fi
  for rel in "${FIXTURE_INPUTS[@]}"; do
    if [[ ! -f "$src/$rel" ]] || ! mkdir -p "$ws/repo/$(dirname "$rel")" || ! cp -p "$src/$rel" "$ws/repo/$rel"; then
      echo "error: cannot copy $rel into the test workspace" >&2
      workspace_cleanup "$ws"
      return 1
    fi
  done
  printf '%s\n' "$ws"
}
workspace_cleanup() { # workspace; refuses anything this script did not allocate
  local ws="$1"
  [[ -n "$ws" ]] || return 0
  case "$ws" in
    "$TEST_TMP_BASE/$WORKSPACE_PREFIX"??????) ;;
    *) echo "error: refusing to remove $ws (not a test workspace)" >&2; return 1 ;;
  esac
  [[ -e "$ws" || -L "$ws" ]] || return 0
  if ! workspace_owned "$ws"; then
    echo "error: refusing to remove $ws (ownership marker missing)" >&2
    return 1
  fi
  rm -rf -- "$ws"
}
workspace_enter() { # source root; sets WORK and removes it when the shell exits
  WORK="$(workspace_create "$1")" || return 1
  trap 'workspace_cleanup "$WORK"' EXIT
}

pass=0; fail=0
check() { # name, expected exit code, command...
  local name="$1" expected="$2"; shift 2
  : > "$WORK/aws.log"; rm -f "$WORK"/gfx-session-*.env "$WORK"/tmp/gfx-session-*.env
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
summary() { echo "---"; echo "passed: $pass, failed: $fail"; [[ "$fail" -eq 0 ]]; }

# --- workspace self-test (AWS-free) --------------------------------------------------------
SELFTEST_DIR=""; WS1=""; WS2=""; SRC_LISTING=""
selftest_cleanup() {
  workspace_cleanup "$WS1"; workspace_cleanup "$WS2"
  [[ -n "$SELFTEST_DIR" ]] && rm -rf -- "$SELFTEST_DIR"
}
setup_failed() { echo "error: self-test setup failed: $*" >&2; exit 1; }
simulate_outputs() { # workspace; writes what a suite run leaves behind, without running it
  local ws="$1"
  if ! workspace_owned "$ws"; then
    echo "error: refusing to write outputs into '$ws' (not an allocated workspace)" >&2
    return 1
  fi
  mkdir -p "$ws/repo/gfx-results/local-existing" "$ws/repo/gfx-results/local-20991231-235959" &&
    printf 'SYNTHETIC workspace output\n' > "$ws/repo/gfx-results/local-existing/sentinel" &&
    printf '{"overall":"SYNTHETIC"}\n' > "$ws/repo/gfx-results/local-20991231-235959/summary.json" &&
    printf 'GFX_INSTANCE_ID=i-0123456789abcdef0\n' > "$ws/tmp/gfx-session-local-20991231-235959.env" &&
    printf 'aws SYNTHETIC\n' > "$ws/aws.log"
}
inputs_match() { # workspace, source root
  local rel
  for rel in "${FIXTURE_INPUTS[@]}"; do cmp -s "$2/$rel" "$1/repo/$rel" || return 1; done
}
only_inputs() { # workspace
  [[ "$(cd "$1/repo" && find . -type f | sed 's|^\./||' | sort)" == "$(printf '%s\n' "${FIXTURE_INPUTS[@]}" | sort)" ]]
}
outside() { # path, tree
  [[ "$1" != "$2" && "$1" != "$2"/* ]]
}
tree_listing() { (cd -- "$1" && find . | LC_ALL=C sort); }
sentinel_intact() { cmp -s "$SELFTEST_DIR/sentinel.ref" "$SELFTEST_DIR/source/gfx-results/local-existing/sentinel"; }
source_results_unchanged() {
  [[ "$(cd "$SELFTEST_DIR/source/gfx-results" && find . | sort)" == "$(printf '%s\n' . ./local-existing ./local-existing/sentinel)" ]]
}
source_unchanged() { [[ "$(tree_listing "$SELFTEST_DIR/source")" == "$SRC_LISTING" ]] && sentinel_intact; }
no_checkout_workspace() { ! compgen -G "$ROOT/${WORKSPACE_PREFIX}*" > /dev/null; }
is_dir() { [[ -d "$1" ]]; }
empty_dir() { [[ -d "$1" && -z "$(ls -A -- "$1")" ]]; }
removed() { [[ -n "$1" && ! -e "$1" ]]; }
equals() { [[ "$1" == "$2" ]]; }
differs() { [[ "$1" != "$2" ]]; }
not() { ! "$@"; }

workspace_self_test() {
  # Fixtures live under the same validated physical base as the suite's workspaces.
  if ! SELFTEST_DIR="$(mktemp -d "$TEST_TMP_BASE/gta20-ec2-gpu-selftest.XXXXXX")" || [[ ! -d "$SELFTEST_DIR" ]]; then
    SELFTEST_DIR=""
    setup_failed "cannot allocate the self-test directory in $TEST_TMP_BASE"
  fi
  trap selftest_cleanup EXIT
  local src="$SELFTEST_DIR/source" missing="$SELFTEST_DIR/source-missing-input"
  local inj="$SELFTEST_DIR/injection-base" rel code failed_ws unmarked out
  # Tripwire: any aws call during the self-test is recorded and fails it.
  mkdir -p "$SELFTEST_DIR/bin" &&
    printf '#!/usr/bin/env bash\necho "$*" >> "%s/aws-invoked"\nexit 97\n' "$SELFTEST_DIR" > "$SELFTEST_DIR/bin/aws" &&
    chmod +x "$SELFTEST_DIR/bin/aws" || setup_failed "aws tripwire"
  export PATH="$SELFTEST_DIR/bin:$PATH"

  # SYNTHETIC source checkout: placeholder inputs (never executed) plus pre-existing results.
  for rel in "${FIXTURE_INPUTS[@]}"; do
    mkdir -p "$src/$(dirname "$rel")" &&
      printf '#!/usr/bin/env bash\n# SYNTHETIC self-test fixture for %s; never executed.\nexit 99\n' "$rel" > "$src/$rel" ||
      setup_failed "source fixture $rel"
  done
  mkdir -p "$src/gfx-results/local-existing" "$src/tmp-inside" "$inj" &&
    printf 'SYNTHETIC sentinel %s\n\001\377\000 binary tail\n' "$RANDOM$RANDOM" > "$src/gfx-results/local-existing/sentinel" &&
    cp -p "$src/gfx-results/local-existing/sentinel" "$SELFTEST_DIR/sentinel.ref" &&
    cp -Rp "$src" "$missing" && rm -f -- "$missing/${FIXTURE_INPUTS[1]}" ||
    setup_failed "sentinel / injection fixtures"
  ln -s "$src" "$SELFTEST_DIR/source-alias" && ln -s "$src/tmp-inside" "$SELFTEST_DIR/source-inner-alias" &&
    ln -s "$ROOT" "$SELFTEST_DIR/checkout-alias" && ln -s "$inj" "$SELFTEST_DIR/allowed-alias" ||
    setup_failed "symlink aliases"
  SRC_LISTING="$(tree_listing "$src")" || setup_failed "source listing"

  WS1="$(workspace_create "$src")" || setup_failed "first workspace"
  WS2="$(workspace_create "$src")" || setup_failed "second workspace"
  assert "first workspace allocated" is_dir "$WS1"
  assert "second workspace allocated" is_dir "$WS2"
  assert "invocation workspaces are distinct" differs "$WS1" "$WS2"
  assert "workspace lies outside the source fixture" outside "$WS1" "$src"
  assert "workspace lies outside the checkout" outside "$WS1" "$ROOT"
  assert "workspace inputs are byte-identical copies" inputs_match "$WS1" "$src"
  assert "workspace holds only the required inputs (no gfx-results)" only_inputs "$WS1"
  assert "cleanup refuses the source fixture" not workspace_cleanup "$src"
  assert "cleanup refuses another temp directory" not workspace_cleanup "$SELFTEST_DIR"
  unmarked="$(mktemp -d "$TEST_TMP_BASE/${WORKSPACE_PREFIX}XXXXXX")" && [[ -d "$unmarked" ]] ||
    setup_failed "unmarked directory"
  assert "cleanup refuses a workspace-named directory without its owner marker" not workspace_cleanup "$unmarked"
  assert "unmarked directory left in place" is_dir "$unmarked"
  assert "outputs refused for an unmarked workspace-named directory" not simulate_outputs "$unmarked"
  assert "unmarked directory still empty" empty_dir "$unmarked"
  rmdir -- "$unmarked" || setup_failed "remove unmarked directory"
  assert "source fixture untouched by refused cleanups" source_unchanged

  # Temporary base: physical path only, never inside the checkout or the source tree.
  assert "base resolver rejects the checkout" not tmp_base_resolve "$ROOT"
  assert "base resolver rejects a directory inside the checkout" not tmp_base_resolve "$ROOT/tests/scripts"
  assert "base resolver rejects a symlink alias of the checkout" not tmp_base_resolve "$SELFTEST_DIR/checkout-alias"
  assert "base resolver rejects the source tree" not tmp_base_resolve "$src" "$src"
  assert "base resolver rejects a symlink alias of the source tree" not tmp_base_resolve "$SELFTEST_DIR/source-alias" "$src"
  assert "base resolver accepts an outside alias, physically" equals "$(tmp_base_resolve "$SELFTEST_DIR/allowed-alias" "$src")" "$inj"
  # An unknown mode exits 2 right after base resolution without writing; 1 means TMPDIR was refused.
  TMPDIR="$inj" bash "$SELF" --no-such-mode 2>/dev/null; code=$?
  assert "control: an outside TMPDIR reaches argument parsing" equals "$code" 2
  TMPDIR="$ROOT" bash "$SELF" --no-such-mode 2>/dev/null; code=$?
  assert "invocation refuses TMPDIR=checkout before any write" equals "$code" 1
  TMPDIR="$SELFTEST_DIR/checkout-alias" bash "$SELF" --no-such-mode 2>/dev/null; code=$?
  assert "invocation refuses TMPDIR=symlink alias of the checkout before any write" equals "$code" 1
  (TEST_TMP_BASE="$src/tmp-inside"; workspace_create "$src") > /dev/null 2>&1; code=$?
  assert "workspace refused inside the source tree" differs "$code" 0
  (TEST_TMP_BASE="$SELFTEST_DIR/source-alias"; workspace_create "$src") > /dev/null 2>&1; code=$?
  assert "workspace refused at a symlink alias of the source tree" differs "$code" 0
  (TEST_TMP_BASE="$SELFTEST_DIR/source-inner-alias"; workspace_create "$src") > /dev/null 2>&1; code=$?
  assert "workspace refused at a symlink alias inside the source tree" differs "$code" 0
  assert "rejected destinations wrote nothing into the source tree" source_unchanged
  assert "source tmp-inside still empty" empty_dir "$src/tmp-inside"
  assert "no workspace was created in the checkout" no_checkout_workspace

  # Injected setup failures: nonzero, nothing left behind, nothing written elsewhere.
  (TEST_TMP_BASE="$inj"; mktemp() { return 1; }; workspace_create "$src") > /dev/null 2>&1; code=$?
  assert "allocation failure exits nonzero" differs "$code" 0
  assert "allocation failure leaves nothing behind" empty_dir "$inj"
  out="$(TEST_TMP_BASE="$inj"; workspace_create "$missing" 2>/dev/null)"; code=$?
  assert "missing input exits nonzero" differs "$code" 0
  assert "missing input prints no workspace path" equals "$out" ""
  assert "missing input failure removes its partial workspace" empty_dir "$inj"
  (TEST_TMP_BASE="$inj"; cp() { return 1; }; workspace_create "$src") > /dev/null 2>&1; code=$?
  assert "copy failure exits nonzero" differs "$code" 0
  assert "copy failure removes its partial workspace" empty_dir "$inj"
  (TEST_TMP_BASE="$inj"; workspace_enter "$missing" 2>/dev/null || exit 1; : > "$SELFTEST_DIR/wrote-after-enter"); code=$?
  assert "failed workspace setup stops the invocation" equals "$code" 1
  assert "no write after a failed workspace setup" removed "$SELFTEST_DIR/wrote-after-enter"
  assert "outputs refused for an empty workspace path" not simulate_outputs "" 2>/dev/null
  assert "outputs refused for a missing workspace" not simulate_outputs "$TEST_TMP_BASE/${WORKSPACE_PREFIX}absent" 2>/dev/null
  assert "outputs refused for the source tree" not simulate_outputs "$src" 2>/dev/null
  assert "injected failures left the source tree unchanged" source_unchanged
  assert "injected failures left nothing in the injection base" empty_dir "$inj"
  assert "injected failures created no workspace in the checkout" no_checkout_workspace

  simulate_outputs "$WS1" && simulate_outputs "$WS2" || setup_failed "workspace outputs"
  workspace_cleanup "$WS1"; code=$?
  assert "normal cleanup succeeds" equals "$code" 0
  assert "normal cleanup removes its workspace" removed "$WS1"
  assert "normal cleanup preserves the other workspace" inputs_match "$WS2" "$src"
  assert "other workspace keeps its outputs" is_dir "$WS2/repo/gfx-results/local-20991231-235959"
  assert "sentinel survives normal cleanup byte-for-byte" sentinel_intact
  assert "source gfx-results unchanged after normal cleanup" source_results_unchanged

  # Simulated failure: same setup + exit trap as the suite, then the invocation dies.
  (
    workspace_enter "$src" || exit 90
    printf '%s\n' "$WORK" > "$SELFTEST_DIR/failed-workspace" || exit 91
    simulate_outputs "$WORK" || exit 92
    exit 3
  )
  code=$?
  failed_ws="$(cat "$SELFTEST_DIR/failed-workspace" 2>/dev/null)"
  assert "simulated failure keeps its exit status" equals "$code" 3
  assert "failed invocation allocated its own workspace" differs "${failed_ws:-$WS2}" "$WS2"
  assert "failure cleanup removes the failed workspace" removed "$failed_ws"
  assert "failure cleanup preserves the other workspace" inputs_match "$WS2" "$src"
  assert "sentinel survives the simulated failure byte-for-byte" sentinel_intact
  assert "source gfx-results unchanged after the simulated failure" source_results_unchanged

  workspace_cleanup "$WS2"
  assert "second workspace removed by its own cleanup" removed "$WS2"
  assert "sentinel survives all cleanups byte-for-byte" sentinel_intact
  assert "source tree unchanged after all cleanups" source_unchanged
  assert "aws was never invoked" removed "$SELFTEST_DIR/aws-invoked"
  summary
}

case "${1:-}" in
  "") ;;
  --workspace-self-test) workspace_self_test; exit $? ;;
  *) echo "usage: $0 [--workspace-self-test]" >&2; exit 2 ;;
esac

# --- offline suite ---------------------------------------------------------------------------
workspace_enter "$ROOT" || exit 1
cd "$WORK" || exit 1
SCRIPT="$WORK/repo/scripts/ci/ec2-gpu-session.sh"
export STOP_FORCE_AFTER_S="${STOP_FORCE_AFTER_S:-2}" STOP_POLL_S="${STOP_POLL_S:-1}"

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

# TMPDIR: run-gfx-compat-local.sh keeps its session state there.
export PATH="$WORK/bin:$PATH" FAKE_LOG="$WORK/aws.log" RUNNER_TEMP="$WORK" TMPDIR="$WORK/tmp" \
       AWS_REGION=eu-north-1 GFX_BUCKET=gta20-gfx-artifacts-433213574600 RUN_ID=123-1
# Orchestrator settings must not send operator-script output outside the workspace.
unset GITHUB_ENV GFX_RESULTS_ROOT GFX_SCENARIOS_DIR

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
assert "the harness revision is uploaded with the scripts" log_has "harness-revision.txt s3://gta20-gfx-artifacts-433213574600/runs/123-1/scripts/"
assert "staged scenarios go next to the host scripts" log_has "$WORK/staged/ s3://gta20-gfx-artifacts-433213574600/runs/123-1/scripts/scenarios/"
check "rejects bad RUN_ID" 1 env RUN_ID='1;rm -rf /' bash "$SCRIPT" resolve
check "rejects bad bucket" 1 env GFX_BUCKET='Bad_Bucket' bash "$SCRIPT" resolve
check "rejects unknown subcommand" 1 run terminate
assert "never terminates or launches instances" both log_lacks "terminate-instances" log_lacks "run-instances"

# Operator scripts (scripts/aws): confirmation gates, quota gate, stop-on-exit trap.
# Workspace copies: their gfx-results/local-* output lands in $WORK/repo and goes with $WORK.
LOCAL="$WORK/repo/scripts/aws/run-gfx-compat-local.sh"
DEPLOY="$WORK/repo/scripts/aws/deploy-gfx-stack.sh"
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

summary
