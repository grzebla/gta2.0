#!/usr/bin/env bash
# Enforces the engine's dependency rules (docs/architecture.md):
# simulation-side modules must not see SDL, the GPU, the renderer or the platform layer.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
status=0
for module in core assets sim scene testing; do
  if grep -rnE '#include *[<"](SDL3?/|SDL\.h|gta20/render/|gta20/platform/)' "$ROOT/engine/$module"; then
    echo "::error::engine/$module must not include SDL, render or platform headers"
    status=1
  fi
done
for module in core assets; do
  if grep -rnE '#include *[<"]gta20/(sim|scene|testing)/' "$ROOT/engine/$module"; then
    echo "::error::engine/$module must not depend on higher-level modules"
    status=1
  fi
done
if grep -rnE '#include *[<"]gta20/(scene|testing)/' "$ROOT/engine/sim"; then
  echo "::error::engine/sim must not depend on scene or testing"
  status=1
fi
[[ $status -eq 0 ]] && echo "module boundaries OK"
exit $status
