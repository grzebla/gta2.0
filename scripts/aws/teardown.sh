#!/usr/bin/env bash
# Removes the GPU test infrastructure. Stacks are deleted; the artifacts bucket is
# retained by design (DeletionPolicy: Retain) and is emptied/deleted only with --purge-bucket.
# Usage: scripts/aws/teardown.sh [--purge-bucket]
# shellcheck source=common.sh
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
purge="${1:-}"
bucket=$(stack_output "$GFX_STACK" ArtifactsBucketName || true)

confirm "usun-opengta2" "Delete stacks $OIDC_STACK and $GFX_STACK in $AWS_REGION (instance, volume, roles, reaper).
$( [[ "$purge" == "--purge-bucket" ]] && echo "ALSO empty and delete bucket ${bucket:-?} (game zip, results)." )"

for stack in "$OIDC_STACK" "$GFX_STACK"; do
  if aws cloudformation describe-stacks --stack-name "$stack" >/dev/null 2>&1; then
    log "deleting $stack"
    aws cloudformation delete-stack --stack-name "$stack"
    aws cloudformation wait stack-delete-complete --stack-name "$stack"
  fi
done

if [[ "$purge" == "--purge-bucket" && -n "$bucket" && "$bucket" != "None" ]]; then
  log "emptying and deleting $bucket"
  aws s3 rm "s3://$bucket" --recursive --only-show-errors
  aws s3api delete-bucket --bucket "$bucket"
fi
log "done - run scripts/aws/cost-check.sh to confirm nothing billable remains"
