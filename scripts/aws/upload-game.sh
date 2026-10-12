#!/usr/bin/env bash
# Uploads YOUR copy of GTA 2 (zip of the game folder) to the PRIVATE artifacts bucket and
# prints its SHA-256 for tests/gfx/compat-matrix.json. Never commit game files to Git.
# Usage: scripts/aws/upload-game.sh /path/to/gta2-10.5.zip
# shellcheck source=common.sh
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
zip="${1:?usage: $0 <gta2-10.5.zip>}"
[[ -f "$zip" ]] || die "$zip not found"
unzip -l "$zip" | grep -qi 'gta2.exe' || die "$zip does not contain gta2.exe"

bucket=$(stack_output "$GFX_STACK" ArtifactsBucketName)
[[ -n "$bucket" && "$bucket" != "None" ]] || die "stack $GFX_STACK not found - deploy it first"
block=$(aws s3api get-public-access-block --bucket "$bucket" --query 'PublicAccessBlockConfiguration' --output json)
[[ $(jq '[.[]] | all' <<<"$block") == "true" ]] || die "bucket $bucket does not block public access - refusing to upload"

sha=$(shasum -a 256 "$zip" | cut -d' ' -f1)
aws s3 cp "$zip" "s3://$bucket/game/gta2-10.5.zip" --sse AES256 --only-show-errors
log "uploaded to s3://$bucket/game/gta2-10.5.zip (private)"
echo "Set game.sha256 in tests/gfx/compat-matrix.json to: $sha"
