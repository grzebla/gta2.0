#!/usr/bin/env bash
# One-time deployment of the GPU test host stack (PAID: the instance starts immediately).
# Usage: scripts/aws/deploy-gfx-stack.sh [subnet-id]
# shellcheck source=common.sh
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

quota=$(gpu_quota)
awk -v q="$quota" 'BEGIN { exit !(q >= 4) }' || die "GPU quota (Running On-Demand G and VT) is $quota vCPU; need >= 4. Wait for the quota request."

vpc=$(aws ec2 describe-vpcs --filters Name=is-default,Values=true --query 'Vpcs[0].VpcId' --output text)
[[ "$vpc" == vpc-* ]] || die "no default VPC"
subnet="${1:-$(aws ec2 describe-subnets --filters Name=vpc-id,Values="$vpc" Name=default-for-az,Values=true \
  --query 'sort_by(Subnets,&AvailabilityZone)[0].SubnetId' --output text)}"
[[ "$subnet" == subnet-* ]] || die "no subnet found"

cd "$REPO_ROOT"
confirm "uruchom-ec2" "Deploy stack $GFX_STACK in $AWS_REGION (VPC $vpc, subnet $subnet).
COST: g4dn.xlarge Windows ~0.75 USD/h starts NOW; it stops itself after MaxRuntimeMinutes (60).
Stopped: ~5 USD/month for the 60 GiB volume until the stack is deleted."

aws cloudformation deploy --stack-name "$GFX_STACK" \
  --template-file infra/aws/gfx-test-stack.yaml \
  --capabilities CAPABILITY_IAM \
  --parameter-overrides "VpcId=$vpc" "SubnetId=$subnet" \
  --tags Project=GTA20

instance=$(stack_output "$GFX_STACK" InstanceId)
bucket=$(stack_output "$GFX_STACK" ArtifactsBucketName)
log "stack ready: instance $instance, bucket $bucket"
log "the instance is running now - either start the first probe right away"
log "(scripts/aws/run-gfx-compat-local.sh probe) or stop it: aws ec2 stop-instances --instance-ids $instance"
