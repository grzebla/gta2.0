#!/usr/bin/env bash
# READ-ONLY inventory of everything that can still cost money after the experiment.
# Optional --cost-explorer adds month-to-date cost by service (each Cost Explorer API call costs 0.01 USD).
# shellcheck source=common.sh
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

echo "## EC2 instances (Project=OpenGTA2)"
aws ec2 describe-instances --filters Name=tag:Project,Values=OpenGTA2 \
  --query 'Reservations[].Instances[].[InstanceId,InstanceType,State.Name,LaunchTime]' --output table
echo "## EBS volumes not deleted (any) and snapshots owned by this account"
# shellcheck disable=SC2016 # JMESPath literals, not shell
aws ec2 describe-volumes --query 'Volumes[].[VolumeId,Size,State,Tags[?Key==`Project`]|[0].Value]' --output table
aws ec2 describe-snapshots --owner-ids self --query 'Snapshots[].[SnapshotId,VolumeSize,StartTime]' --output table
echo "## Elastic IPs (billed when allocated)"
aws ec2 describe-addresses --query 'Addresses[].[PublicIp,AllocationId,InstanceId]' --output table
echo "## CloudFormation stacks"
aws cloudformation list-stacks --stack-status-filter CREATE_COMPLETE UPDATE_COMPLETE DELETE_FAILED ROLLBACK_COMPLETE \
  --query "StackSummaries[?starts_with(StackName,'opengta2')].[StackName,StackStatus]" --output table
echo "## S3 buckets (opengta2*) and size"
for b in $(aws s3api list-buckets --query "Buckets[?starts_with(Name,'opengta2')].Name" --output text); do
  echo "$b: $(aws s3 ls "s3://$b" --recursive --summarize | awk '/Total Size/ {printf "%.1f MiB", $3/1048576}')"
done
echo "## Lambda / log groups"
aws lambda list-functions --query "Functions[?starts_with(FunctionName,'opengta2')].FunctionName" --output text
aws logs describe-log-groups --log-group-name-prefix /aws/lambda/opengta2 --query 'logGroups[].[logGroupName,storedBytes]' --output text
echo "## Budgets"
aws budgets describe-budgets --region us-east-1 --account-id "$(aws sts get-caller-identity --query Account --output text)" \
  --query 'Budgets[].[BudgetName,BudgetLimit.Amount,CalculatedSpend.ActualSpend.Amount]' --output table || true

if [[ "${1:-}" == "--cost-explorer" ]]; then
  echo "## Month-to-date cost by service (Cost Explorer, 0.01 USD per call)"
  # shellcheck disable=SC2016 # JMESPath literals, not shell
  aws ce get-cost-and-usage --region us-east-1 \
    --time-period "Start=$(date -u +%Y-%m-01),End=$(date -u -v+1d +%Y-%m-%d 2>/dev/null || date -u -d tomorrow +%Y-%m-%d)" \
    --granularity MONTHLY --metrics UnblendedCost --group-by Type=DIMENSION,Key=SERVICE \
    --query 'ResultsByTime[0].Groups[?Metrics.UnblendedCost.Amount>`0`].[Keys[0],Metrics.UnblendedCost.Amount]' --output table
fi
