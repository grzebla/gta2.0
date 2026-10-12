#!/usr/bin/env bash
# Creates the monthly cost budget with e-mail alerts (50% / 80% actual, 100% forecast).
# Budgets has no CloudFormation resource in eu-north-1, and CloudFormation in
# us-east-1 is blocked by the project SCP, so this calls the Budgets API directly.
# Usage: infra/aws/create-budget.sh <alert-email> [monthly-usd=15]
set -euo pipefail
EMAIL="${1:?usage: $0 <alert-email> [monthly-usd]}"
AMOUNT="${2:-15}"
NAME="opengta2-gfx-monthly"
[[ "$AMOUNT" =~ ^[0-9]{1,5}$ ]] || { echo "invalid amount" >&2; exit 1; }
[[ "$EMAIL" =~ ^[^@[:space:]]+@[^@[:space:]]+$ ]] || { echo "invalid e-mail" >&2; exit 1; }

ACCOUNT_ID=$(aws sts get-caller-identity --query Account --output text)
if aws budgets describe-budget --region us-east-1 --account-id "$ACCOUNT_ID" --budget-name "$NAME" >/dev/null 2>&1; then
  echo "Budget $NAME already exists - not changing it."; exit 0
fi

budget=$(jq -n --arg n "$NAME" --arg a "$AMOUNT" \
  '{BudgetName:$n, BudgetType:"COST", TimeUnit:"MONTHLY", BudgetLimit:{Amount:$a, Unit:"USD"}}')
notifications=$(jq -n --arg e "$EMAIL" '[
  {Notification:{NotificationType:"ACTUAL",     ComparisonOperator:"GREATER_THAN", Threshold:50,  ThresholdType:"PERCENTAGE"}, Subscribers:[{SubscriptionType:"EMAIL", Address:$e}]},
  {Notification:{NotificationType:"ACTUAL",     ComparisonOperator:"GREATER_THAN", Threshold:80,  ThresholdType:"PERCENTAGE"}, Subscribers:[{SubscriptionType:"EMAIL", Address:$e}]},
  {Notification:{NotificationType:"FORECASTED", ComparisonOperator:"GREATER_THAN", Threshold:100, ThresholdType:"PERCENTAGE"}, Subscribers:[{SubscriptionType:"EMAIL", Address:$e}]}
]')
aws budgets create-budget --region us-east-1 --account-id "$ACCOUNT_ID" \
  --budget "$budget" --notifications-with-subscribers "$notifications"
echo "Budget $NAME created: ${AMOUNT} USD/month, alerts to $EMAIL"
