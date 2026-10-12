#!/usr/bin/env bash
# Regenerates the committed SPIR-V from GLSL with a pinned glslang release.
# CI runs this and fails if the result differs from the committed files.
# Usage: scripts/shaders/compile-spirv.sh [path-to-glslangValidator]
set -euo pipefail
GLSLANG_VERSION="16.6.0"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SHADERS="$ROOT/engine/render/shaders"
glslang="${1:-glslangValidator}"
command -v "$glslang" >/dev/null || { echo "glslangValidator not found (want $GLSLANG_VERSION)" >&2; exit 1; }
"$glslang" --version | grep -q "$GLSLANG_VERSION" || echo "warning: glslang is not $GLSLANG_VERSION; output may differ" >&2
"$glslang" -V --target-env vulkan1.0 -S vert -o "$SHADERS/spirv/quad.vert.spv" "$SHADERS/quad.vert.glsl"
"$glslang" -V --target-env vulkan1.0 -S frag -o "$SHADERS/spirv/quad.frag.spv" "$SHADERS/quad.frag.glsl"
echo "SPIR-V regenerated in $SHADERS/spirv"
