#!/usr/bin/env bash
# Fixture tests for scripts/ci/check-module-boundaries.sh. Run:
#   bash tests/scripts/check-module-boundaries.test.sh
#
# Every case builds a fresh fixture tree in a temporary directory and runs the checker on it:
#   - "reject" cases copy engine/ and apps/ from the checkout and add ONE SYNTHETIC edge (a source
#     file or CMake lines marked "SYNTHETIC"); the checker must fail with exactly one error that
#     names the edge, the file and the line;
#   - "accept" cases (current graph, comments, allowed edges) must pass;
#   - a minimal SYNTHETIC tree in which every unit uses every dependency it is allowed must pass.
# The checkout is only read: the suite fails if engine/, apps/ or the checker change while it runs.
set -uo pipefail

ROOT="$(cd -P "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)"
CHECKER="$ROOT/scripts/ci/check-module-boundaries.sh"
BASE="$(cd -P "${TMPDIR:-/tmp}" 2>/dev/null && pwd -P)" || {
  echo "error: temporary base '${TMPDIR:-/tmp}' is not a directory" >&2
  exit 1
}
case "$BASE/" in
  "$ROOT"/*)
    echo "error: temporary base '$BASE' is inside the checkout" >&2
    exit 1
    ;;
esac
WORK="$(mktemp -d "$BASE/gta20-boundaries.XXXXXX")" || exit 1
trap 'rm -rf -- "$WORK"' EXIT

MARK="SYNTHETIC boundary-checker fixture (tests/scripts/check-module-boundaries.test.sh)"
passed=0
failed=0

fingerprint() {
  (cd "$ROOT" && find engine apps scripts/ci/check-module-boundaries.sh -type f -exec cksum {} + | LC_ALL=C sort)
}
BEFORE="$(fingerprint)"

ok() {
  passed=$((passed + 1))
  echo "ok   $1"
}
fail() { # name reason output
  failed=$((failed + 1))
  echo "FAIL $1: $2"
  printf '%s\n' "$3" | sed 's/^/     | /'
}

copy_fixture() { # -> prints a new fixture root holding copies of engine/ and apps/
  local dir
  dir="$(mktemp -d "$WORK/fixture.XXXXXX")" && cp -R "$ROOT/engine" "$ROOT/apps" "$dir/" && printf '%s\n' "$dir"
}

# Applies one synthetic change to a fixture. Prints "<file relative to the fixture> <line>" of the
# edge it added.
#   src   - new source file <unit>/src/synthetic_boundary_fixture.cpp with the payload line
#   hdr   - new header <unit>/include/gta20/<module>/synthetic_boundary_fixture.hpp
#   cmake - payload (may span lines) appended to <unit>/CMakeLists.txt
#   unit  - new unit directory with an empty CMakeLists.txt
#   none  - no change
mutate() { # fixture kind unit payload
  local fx="$1" kind="$2" unit="$3" payload="$4" rel lines
  case "$kind" in
    src | hdr)
      if [[ $kind == src ]]; then
        rel="$unit/src/synthetic_boundary_fixture.cpp"
      else
        rel="$unit/include/gta20/${unit#*/}/synthetic_boundary_fixture.hpp"
      fi
      mkdir -p "$(dirname "$fx/$rel")"
      printf '// %s\n%s\n' "$MARK" "$payload" >"$fx/$rel"
      echo "$rel 2"
      ;;
    cmake)
      rel="$unit/CMakeLists.txt"
      lines="$(wc -l <"$fx/$rel")"
      printf '# %s\n%s\n' "$MARK" "$payload" >>"$fx/$rel"
      echo "$rel $((lines + 2))"
      ;;
    unit)
      mkdir -p "$fx/$unit"
      printf '# %s\n' "$MARK" >"$fx/$unit/CMakeLists.txt"
      echo "$unit 1"
      ;;
    none) echo "- 0" ;;
  esac
}

reject() { # name kind unit payload expected-message-fragment
  local name="$1" fx where rel line out code errors
  fx="$(copy_fixture)" || {
    fail "$name" "could not create the fixture" ""
    return
  }
  where="$(mutate "$fx" "$2" "$3" "$4")"
  rel="${where% *}"
  line="${where##* }"
  out="$(bash "$CHECKER" "$fx" 2>&1)"
  code=$?
  errors="$(printf '%s\n' "$out" | grep -c '^::error' || true)"
  if [[ $code -eq 0 ]]; then
    fail "$name" "checker accepted a forbidden edge" "$out"
  elif [[ $errors -ne 1 ]]; then
    fail "$name" "expected exactly one error, got $errors" "$out"
  elif [[ $out != *"::error file=$rel,line=$line::"* ]]; then
    fail "$name" "error does not point at $rel:$line" "$out"
  elif [[ $out != *"$5"* ]]; then
    fail "$name" "error does not mention '$5'" "$out"
  else
    ok "$name"
  fi
}

accept() { # name kind unit payload
  local name="$1" fx out
  fx="$(copy_fixture)" || {
    fail "$name" "could not create the fixture" ""
    return
  }
  mutate "$fx" "$2" "$3" "$4" >/dev/null
  if out="$(bash "$CHECKER" "$fx" 2>&1)" && [[ $out == "module boundaries OK"* ]]; then
    ok "$name"
  else
    fail "$name" "checker rejected an allowed graph" "$out"
  fi
}

# Minimal SYNTHETIC tree: every unit includes and links every module it may depend on (plus SDL,
# nlohmann/json and OS headers where allowed); nothing else.
minimal_fixture() {
  local fx="$WORK/minimal" spec unit deps target dep links module
  for spec in \
    "engine/core:" \
    "engine/assets:core" \
    "engine/sim:core assets" \
    "engine/scene:core assets" \
    "engine/testing:core assets sim" \
    "engine/render:core scene" \
    "engine/platform:core" \
    "apps/gta20-headless:core assets sim scene testing" \
    "apps/gta2:core assets sim scene testing render platform"; do
    unit="${spec%%:*}"
    deps="${spec#*:}"
    module="${unit#*/}"
    target="gta20_$module"
    mkdir -p "$fx/$unit/src"
    links=""
    {
      echo "// $MARK"
      for dep in $deps; do
        echo "#include \"gta20/$dep/api.hpp\""
        links="$links gta20::$dep"
      done
      case "$unit" in
        engine/render | engine/platform | apps/gta2) echo "#include <SDL3/SDL.h>" ;;
        engine/testing) echo "#include <nlohmann/json.hpp>" ;;
      esac
      [[ $unit == engine/core ]] && echo "#include <windows.h>"
    } >"$fx/$unit/src/main.cpp"
    case "$unit" in
      engine/render | engine/platform | apps/gta2) links="$links \${GTA20_SDL_TARGET}" ;;
      engine/testing) links="$links PRIVATE gta20::json" ;;
    esac
    if [[ $unit == engine/* ]]; then
      mkdir -p "$fx/$unit/include/gta20/$module"
      printf '// %s\n#pragma once\n' "$MARK" >"$fx/$unit/include/gta20/$module/api.hpp"
    fi
    printf '# %s\nadd_library(%s src/main.cpp)\ntarget_include_directories(%s PUBLIC include)\n' \
      "$MARK" "$target" "$target" >"$fx/$unit/CMakeLists.txt"
    [[ -n $links ]] && printf 'target_link_libraries(%s PUBLIC%s)\n' "$target" "$links" >>"$fx/$unit/CMakeLists.txt"
  done
  printf '%s\n' "$fx"
}

echo "== accepted graphs"
if out="$(bash "$CHECKER" 2>&1)" && [[ $out == "module boundaries OK"* ]]; then
  ok "checkout (default root)"
else
  fail "checkout (default root)" "checker rejected the checkout" "$out"
fi
accept current-graph-copy none - ""
if out="$(bash "$CHECKER" "$(minimal_fixture)" 2>&1)" && [[ $out == "module boundaries OK"* ]]; then
  ok "minimal-synthetic-graph"
else
  fail "minimal-synthetic-graph" "checker rejected the documented graph" "$out"
fi
accept commented-include src engine/scene '// #include "gta20/sim/world.hpp"'
accept commented-link cmake engine/scene '# target_link_libraries(gta20_scene PRIVATE gta20::sim)'
accept render-include-scene src engine/render '#include "gta20/scene/draw_list.hpp"'
accept testing-include-sim src engine/testing '#include "gta20/sim/world.hpp"'
accept gta2-includes-testing src apps/gta2 '#include "gta20/testing/runner.hpp"'
accept core-os-header src engine/core '#include <windows.h>'

echo "== forbidden module edges (include)"
reject core-include-assets src engine/core '#include "gta20/assets/gmp_map.hpp"' 'engine/core -> assets (include)'
reject scene-include-sim src engine/scene '#include "gta20/sim/world.hpp"' 'engine/scene -> sim (include)'
reject scene-include-testing src engine/scene '#include "gta20/testing/trace.hpp"' 'engine/scene -> testing (include)'
reject render-include-sim src engine/render '#include "gta20/sim/world.hpp"' 'engine/render -> sim (include)'
reject platform-include-render src engine/platform '#include "gta20/render/renderer.hpp"' 'engine/platform -> render (include)'
reject assets-include-scene src engine/assets '#include "gta20/scene/draw_list.hpp"' 'engine/assets -> scene (include)'
reject sim-header-include-render hdr engine/sim '#include <gta20/render/renderer.hpp>' 'engine/sim -> render (include)'
reject testing-include-scene src engine/testing '#include "gta20/scene/draw_list.hpp"' 'engine/testing -> scene (include)'
reject render-include-platform src engine/render '#include "gta20/platform/platform.hpp"' 'engine/render -> platform (include)'
reject headless-include-render src apps/gta20-headless '#include "gta20/render/renderer.hpp"' 'apps/gta20-headless -> render (include)'
reject scene-include-relative-path src engine/scene '#include "../../render/include/gta20/render/renderer.hpp"' 'engine/scene -> render (include)'
reject core-include-spaced src engine/core '#  include   "gta20/sim/world.hpp"' 'engine/core -> sim (include)'
reject core-include-unknown-module src engine/core '#include "gta20/audio/mixer.hpp"' 'engine/core -> audio (include): unknown gta20 module'

echo "== forbidden module edges (link)"
reject core-link-assets cmake engine/core 'target_link_libraries(gta20_core PUBLIC gta20::assets)' 'engine/core -> assets (link)'
reject scene-link-sim cmake engine/scene 'target_link_libraries(gta20_scene PUBLIC gta20::sim)' 'engine/scene -> sim (link)'
reject scene-link-testing-target cmake engine/scene 'target_link_libraries(gta20_scene PRIVATE gta20_testing)' 'engine/scene -> testing (link)'
reject render-link-sim-multiline cmake engine/render $'target_link_libraries(gta20_render\n    PRIVATE\n        gta20::sim)' 'engine/render -> sim (link)'
reject platform-link-render cmake engine/platform 'target_link_libraries(gta20_platform PUBLIC gta20::render)' 'engine/platform -> render (link)'
reject sim-link-testing cmake engine/sim 'target_link_libraries(gta20_sim PRIVATE gta20::testing)' 'engine/sim -> testing (link)'
reject headless-link-platform cmake apps/gta20-headless 'target_link_libraries(gta20-headless PRIVATE gta20::platform)' 'apps/gta20-headless -> platform (link)'

echo "== forbidden module edges (include path)"
reject scene-include-dir-sim cmake engine/scene 'target_include_directories(gta20_scene PRIVATE ../sim/include)' 'engine/scene -> sim (include path ../sim/include)'
# shellcheck disable=SC2016 # CMake variable, not a shell expansion
reject render-include-dir-sim cmake engine/render 'target_include_directories(gta20_render PRIVATE ${PROJECT_SOURCE_DIR}/engine/sim/include)' 'engine/render -> sim (include path'
reject headless-include-dir-render cmake apps/gta20-headless 'include_directories(../../engine/render/include)' 'apps/gta20-headless -> render (include path'

echo "== SDL, GPU API, nlohmann/json, OS"
reject sim-include-sdl src engine/sim '#include <SDL3/SDL.h>' 'engine/sim -> SDL (include SDL3/SDL.h)'
reject scene-include-sdl src engine/scene '#include <SDL3/SDL_gpu.h>' 'engine/scene -> SDL (include SDL3/SDL_gpu.h)'
# shellcheck disable=SC2016
reject testing-link-sdl cmake engine/testing 'target_link_libraries(gta20_testing PRIVATE ${GTA20_SDL_TARGET})' 'engine/testing -> SDL (link'
reject headless-link-sdl cmake apps/gta20-headless 'target_link_libraries(gta20-headless PRIVATE SDL3::SDL3)' 'apps/gta20-headless -> SDL (link SDL3::SDL3)'
reject platform-include-gpu-api src engine/platform '#include <vulkan/vulkan.h>' 'engine/platform -> GPU API (include vulkan/vulkan.h)'
reject sim-include-json src engine/sim '#include <nlohmann/json.hpp>' 'engine/sim -> nlohmann/json (include'
reject scene-link-json cmake engine/scene 'target_link_libraries(gta20_scene PRIVATE gta20::json)' 'engine/scene -> nlohmann/json (link'
reject sim-include-os src engine/sim '#include <windows.h>' 'engine/sim -> OS (include windows.h)'
reject sim-link-os cmake engine/sim 'target_link_libraries(gta20_sim PRIVATE psapi)' 'engine/sim -> psapi (link)'

echo "== units without rules"
reject unknown-engine-module unit engine/audio '' 'engine/audio: no dependency rules'
reject unknown-app unit apps/editor '' 'apps/editor: no dependency rules'

if [[ "$(fingerprint)" == "$BEFORE" ]]; then
  ok "checkout unchanged (engine/, apps/, checker)"
else
  fail "checkout unchanged" "engine/, apps/ or the checker changed during the run" ""
fi

echo "$passed passed, $failed failed"
[[ $failed -eq 0 ]]
