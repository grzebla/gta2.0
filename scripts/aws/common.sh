# shellcheck shell=bash
# Shared helpers for the operator scripts in scripts/aws/. Source, do not execute.
# These scripts run with YOUR AWS CLI session (no GitHub OIDC needed) and only in eu-north-1.
set -euo pipefail

export AWS_REGION="${AWS_REGION:-eu-north-1}"
export AWS_DEFAULT_REGION="$AWS_REGION"
GFX_STACK="${GFX_STACK:-opengta2-gfx}"
OIDC_STACK="${OIDC_STACK:-opengta2-github-oidc}"
# shellcheck disable=SC2034 # used by the scripts that source this file
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

die() { echo "error: $*" >&2; exit 1; }
log() { echo "[$(date -u +%H:%M:%S)] $*"; }

[[ "$AWS_REGION" == "eu-north-1" ]] || die "this project may only use eu-north-1 (got $AWS_REGION)"
command -v aws >/dev/null || die "AWS CLI not found"
command -v jq >/dev/null || die "jq not found"

# Paid or destructive steps require typing the exact word (or CONFIRM=<word> for scripted use).
confirm() {
  local word="$1" message="$2" answer="${CONFIRM:-}"
  echo "$message"
  if [[ -z "$answer" ]]; then
    read -r -p "Type '$word' to continue: " answer
  fi
  [[ "$answer" == "$word" ]] || die "not confirmed - nothing was changed"
}

stack_output() { # stack, output key
  aws cloudformation describe-stacks --stack-name "$1" \
    --query "Stacks[0].Outputs[?OutputKey=='$2'].OutputValue" --output text 2>/dev/null
}

gpu_quota() {
  aws service-quotas get-service-quota --service-code ec2 --quota-code L-DB2E81BA \
    --query 'Quota.Value' --output text
}
