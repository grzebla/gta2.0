#!/usr/bin/env bash
# Fixture tests for scripts/ci/check-module-boundaries.sh. Run:
#   bash tests/scripts/check-module-boundaries.test.sh
#
# Every case builds a fresh fixture tree in a temporary directory and runs the checker on it:
#   - "reject" cases copy the checkout's CMake surface and units (CMakeLists.txt, cmake/, engine/,
#     apps/, tests/) and add ONE SYNTHETIC edge (a source file or CMake lines marked "SYNTHETIC");
#     the checker must fail with exactly one error that names the edge, the file and the line;
#   - "accept" cases (current graph, comments, allowed edges in every supported form) must pass;
#   - a minimal SYNTHETIC tree in which every unit uses every dependency it is allowed must pass.
# The checkout is only read: the suite fails if any copied tree or the checker changes while it runs.
set -uo pipefail

ROOT="$(cd -P "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)"
CHECKER="$ROOT/scripts/ci/check-module-boundaries.sh"
COPIED=(CMakeLists.txt cmake engine apps tests)
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
  (cd "$ROOT" && find "${COPIED[@]}" scripts/ci/check-module-boundaries.sh -type f -exec cksum {} + | LC_ALL=C sort)
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

copy_fixture() { # -> prints a new fixture root holding copies of the checked trees
  local dir item
  dir="$(mktemp -d "$WORK/fixture.XXXXXX")" || return 1
  for item in "${COPIED[@]}"; do
    cp -R "$ROOT/$item" "$dir/" || return 1
  done
  printf '%s\n' "$dir"
}

# Applies one synthetic change to a fixture. Prints "<file relative to the fixture> <line>" of the
# first line it added.
#   src   - new source file <unit>/src/synthetic_boundary_fixture.cpp with the payload line
#   hdr   - new header <unit>/include/gta20/<module>/synthetic_boundary_fixture.hpp
#   cmake - payload (may span lines) appended to the CMake file <where> (relative to the root)
#   unit  - new unit directory <where> with a CMakeLists.txt
#   none  - no change
mutate() { # fixture kind where payload
  local fx="$1" kind="$2" where="$3" payload="$4" rel lines
  case "$kind" in
    src | hdr)
      if [[ $kind == src ]]; then
        rel="$where/src/synthetic_boundary_fixture.cpp"
      else
        rel="$where/include/gta20/${where#*/}/synthetic_boundary_fixture.hpp"
      fi
      mkdir -p "$(dirname "$fx/$rel")"
      printf '// %s\n%s\n' "$MARK" "$payload" >"$fx/$rel"
      echo "$rel 2"
      ;;
    cmake)
      rel="$where"
      lines="$(wc -l <"$fx/$rel")"
      printf '# %s\n%s\n' "$MARK" "$payload" >>"$fx/$rel"
      echo "$rel $((lines + 2))"
      ;;
    unit)
      mkdir -p "$fx/$where"
      printf '# %s\n' "$MARK" >"$fx/$where/CMakeLists.txt"
      echo "$where 1"
      ;;
    none) echo "- 0" ;;
  esac
}

reject() { # name kind where payload expected-message-fragment [line offset of the edge in payload]
  local name="$1" fx where rel line out code errors
  fx="$(copy_fixture)" || {
    fail "$name" "could not create the fixture" ""
    return
  }
  where="$(mutate "$fx" "$2" "$3" "$4")"
  rel="${where% *}"
  line="$((${where##* } + ${6:-0}))"
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

accept() { # name kind where payload
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
    [[ $unit == apps/* ]] && target="$module"
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
    if [[ -n $links ]]; then
      printf 'target_link_libraries(%s PUBLIC%s)\n' "$target" "$links" >>"$fx/$unit/CMakeLists.txt"
    fi
  done
  printf '%s\n' "$fx"
}

S=engine/scene/CMakeLists.txt
R=engine/render/CMakeLists.txt
TOP=CMakeLists.txt
APP=apps/gta2/CMakeLists.txt

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
accept commented-link cmake "$S" '# target_link_libraries(gta20_scene PRIVATE gta20::sim)'
accept render-include-scene src engine/render '#include "gta20/scene/draw_list.hpp"'
accept testing-include-sim src engine/testing '#include "gta20/sim/world.hpp"'
accept gta2-includes-testing src apps/gta2 '#include "gta20/testing/runner.hpp"'
accept core-os-header src engine/core '#include <windows.h>'

echo "== accepted CMake forms (counterparts of the rejections below)"
accept scene-link-genex-allowed cmake "$S" 'target_link_libraries(gta20_scene PRIVATE $<BUILD_INTERFACE:gta20::core>)'
accept scene-link-nested-genex-allowed cmake "$S" 'target_link_libraries(gta20_scene PRIVATE $<$<CONFIG:Debug>:gta20::assets>)'
accept render-link-list-allowed cmake "$R" 'target_link_libraries(gta20_render PRIVATE "gta20_core;gta20_scene")'
accept render-link-unquoted-list-allowed cmake "$R" 'target_link_libraries(gta20_render PRIVATE gta20_core;gta20::scene)'
# shellcheck disable=SC2016 # CMake syntax, not shell expansions
accept render-link-sdl-genex-allowed cmake "$R" 'target_link_libraries(gta20_render PRIVATE $<BUILD_INTERFACE:${GTA20_SDL_TARGET}>)'
accept core-link-os-genex-allowed cmake engine/core/CMakeLists.txt 'target_link_libraries(gta20_core PRIVATE $<$<PLATFORM_ID:Windows>:psapi>)'
accept sim-link-genex-allowed cmake engine/sim/CMakeLists.txt 'target_link_libraries(gta20_sim PRIVATE $<BUILD_INTERFACE:gta20::core>)'
accept root-links-scene-to-assets cmake "$TOP" 'target_link_libraries(gta20_scene PRIVATE gta20::assets)'
accept gta2-dir-links-render-to-scene cmake "$APP" 'target_link_libraries(gta20_render PRIVATE gta20::scene)'
accept gta2-links-testing-genex cmake "$APP" 'target_link_libraries(gta2 PRIVATE $<LINK_ONLY:gta20::testing>)'
accept test-target-links-anything cmake tests/unit/CMakeLists.txt 'target_link_libraries(gta20_unit_tests PRIVATE gta20::render gta20::platform)'
# shellcheck disable=SC2016
accept render-include-dir-own-vars cmake "$R" 'target_include_directories(gta20_render PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/engine/scene/include)'
accept scene-include-dir-own-subdir cmake "$S" 'target_include_directories(gta20_scene PRIVATE ../scene/src)'
accept scene-set-property-non-dependency cmake "$S" 'set_target_properties(gta20_scene PROPERTIES OUTPUT_NAME gta20scene)'

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
reject core-link-assets cmake engine/core/CMakeLists.txt 'target_link_libraries(gta20_core PUBLIC gta20::assets)' 'engine/core -> assets (link)'
reject scene-link-sim cmake "$S" 'target_link_libraries(gta20_scene PUBLIC gta20::sim)' 'engine/scene -> sim (link)'
reject scene-link-testing-target cmake "$S" 'target_link_libraries(gta20_scene PRIVATE gta20_testing)' 'engine/scene -> testing (link)'
reject render-link-sim-multiline cmake "$R" $'target_link_libraries(gta20_render\n    PRIVATE\n        gta20::sim)' 'engine/render -> sim (link)'
reject platform-link-render cmake engine/platform/CMakeLists.txt 'target_link_libraries(gta20_platform PUBLIC gta20::render)' 'engine/platform -> render (link)'
reject sim-link-testing cmake engine/sim/CMakeLists.txt 'target_link_libraries(gta20_sim PRIVATE gta20::testing)' 'engine/sim -> testing (link)'
reject headless-link-platform cmake apps/gta20-headless/CMakeLists.txt 'target_link_libraries(gta20-headless PRIVATE gta20::platform)' 'apps/gta20-headless -> platform (link)'
reject root-add-dependencies-scene-sim cmake "$TOP" 'add_dependencies(gta20_scene gta20_sim)' 'engine/scene -> sim (link)'

echo "== generator expressions and CMake lists"
reject scene-link-genex-build-interface cmake "$S" 'target_link_libraries(gta20_scene PRIVATE $<BUILD_INTERFACE:gta20::sim>)' 'engine/scene -> sim (link)'
reject scene-link-nested-genex cmake "$S" 'target_link_libraries(gta20_scene PRIVATE $<$<CONFIG:Debug>:gta20::testing>)' 'engine/scene -> testing (link)'
reject render-link-quoted-list cmake "$R" 'target_link_libraries(gta20_render PRIVATE "gta20_core;gta20_sim")' 'engine/render -> sim (link)'
reject render-link-unquoted-list cmake "$R" 'target_link_libraries(gta20_render PRIVATE gta20::core;gta20::sim)' 'engine/render -> sim (link)'
reject render-link-genex-list cmake "$R" 'target_link_libraries(gta20_render PRIVATE "$<BUILD_INTERFACE:gta20_core;gta20_sim>")' 'engine/render -> sim (link)'
reject scene-include-dir-target-property cmake "$S" 'target_include_directories(gta20_scene PRIVATE $<TARGET_PROPERTY:gta20_sim,INTERFACE_INCLUDE_DIRECTORIES>)' 'engine/scene -> sim (include path'
reject sim-link-os-genex cmake engine/sim/CMakeLists.txt 'target_link_libraries(gta20_sim PRIVATE $<$<PLATFORM_ID:Windows>:psapi>)' 'the simulation may link only gta20 modules'

echo "== unknown targets and unsupported expressions"
reject scene-link-unknown-gta20-target cmake "$S" 'target_link_libraries(gta20_scene PRIVATE gta20_sim_helpers)' 'engine/scene -> gta20_sim_helpers (link): unknown gta20 target'
reject scene-link-unknown-alias cmake "$S" 'target_link_libraries(gta20_scene PRIVATE gta20::physics)' 'engine/scene -> gta20::physics (link): unknown gta20 target'
reject scene-link-unknown-in-genex cmake "$S" 'target_link_libraries(gta20_scene PRIVATE $<BUILD_INTERFACE:gta20_extra>)' 'unknown gta20 target'
# shellcheck disable=SC2016
reject scene-link-variable cmake "$S" 'target_link_libraries(gta20_scene PRIVATE ${SCENE_DEPS})' 'engine/scene -> ${SCENE_DEPS} (link): unsupported dependency expression'
# shellcheck disable=SC2016
reject scene-include-dir-variable cmake "$S" 'target_include_directories(gta20_scene PRIVATE ${SIM_INCLUDE_DIR})' 'engine/scene -> ${SIM_INCLUDE_DIR} (include path'
reject scene-link-app cmake "$S" 'target_link_libraries(gta20_scene PRIVATE gta20-headless)' 'engine/scene -> apps/gta20-headless (link)'

echo "== target ownership (edge attributed to the target, wherever the CMake file is)"
reject gta2-dir-links-scene-to-sim cmake "$APP" 'target_link_libraries(gta20_scene PRIVATE gta20::sim)' 'engine/scene -> sim (link)'
reject root-links-scene-to-sim cmake "$TOP" 'target_link_libraries(gta20_scene PRIVATE gta20::sim)' 'engine/scene -> sim (link)'
reject shared-cmake-links-core-alias-to-assets cmake cmake/Dependencies.cmake 'target_link_libraries(gta20::core INTERFACE gta20::assets)' 'engine/core -> assets (link)'
reject tests-dir-links-render-to-sim cmake tests/unit/CMakeLists.txt 'target_link_libraries(gta20_render PRIVATE gta20::sim)' 'engine/render -> sim (link)'
reject scene-dir-links-platform-to-render cmake "$S" 'target_link_libraries(gta20_platform PRIVATE gta20::render)' 'engine/platform -> render (link)'
# shellcheck disable=SC2016
reject gta2-dir-include-dir-scene-to-sim cmake "$APP" 'target_include_directories(gta20_scene PRIVATE ${PROJECT_SOURCE_DIR}/engine/sim/include)' 'engine/scene -> sim (include path'
reject root-include-dir-render-to-sim cmake "$TOP" 'target_include_directories(gta20_render PUBLIC engine/sim/include)' 'engine/render -> sim (include path engine/sim/include)'
reject gta2-dir-sources-scene-from-sim cmake "$APP" 'target_sources(gta20_scene PRIVATE ${PROJECT_SOURCE_DIR}/engine/sim/src/world.cpp)' 'engine/scene -> sim (sources'
reject scene-sources-from-sim cmake "$S" 'target_sources(gta20_scene PRIVATE ../sim/src/world.cpp)' 'engine/scene -> sim (sources ../sim/src/world.cpp)'
# shellcheck disable=SC2016
reject shared-function-variable-target cmake cmake/CompilerOptions.cmake $'function(synthetic_link target)\n    target_link_libraries(${target} PRIVATE gta20::sim)\nendfunction()' 'cannot attribute a variable target to a module' 1
reject root-link-libraries cmake "$TOP" 'link_libraries(gta20::sim)' 'directory-scoped link_libraries in a shared CMake file'
reject root-include-directories cmake "$TOP" 'include_directories(engine/sim/include)' 'directory-scoped include_directories in a shared CMake file'
reject gta2-dir-set-property-link cmake "$APP" 'set_property(TARGET gta20_scene APPEND PROPERTY INTERFACE_LINK_LIBRARIES gta20::sim)' 'unsupported dependency command set_property'
reject root-set-target-properties-link cmake "$TOP" 'set_target_properties(gta20_render PROPERTIES LINK_LIBRARIES gta20_sim)' 'unsupported dependency command set_target_properties'

echo "== forbidden module edges (include path, own directory)"
reject scene-include-dir-sim cmake "$S" 'target_include_directories(gta20_scene PRIVATE ../sim/include)' 'engine/scene -> sim (include path ../sim/include)'
# shellcheck disable=SC2016
reject render-include-dir-sim cmake "$R" 'target_include_directories(gta20_render PRIVATE ${PROJECT_SOURCE_DIR}/engine/sim/include)' 'engine/render -> sim (include path'
reject headless-include-dir-render cmake apps/gta20-headless/CMakeLists.txt 'include_directories(../../engine/render/include)' 'apps/gta20-headless -> render (include path'

echo "== SDL, GPU API, nlohmann/json, OS"
reject sim-include-sdl src engine/sim '#include <SDL3/SDL.h>' 'engine/sim -> SDL (include SDL3/SDL.h)'
reject scene-include-sdl src engine/scene '#include <SDL3/SDL_gpu.h>' 'engine/scene -> SDL (include SDL3/SDL_gpu.h)'
# shellcheck disable=SC2016
reject testing-link-sdl cmake engine/testing/CMakeLists.txt 'target_link_libraries(gta20_testing PRIVATE ${GTA20_SDL_TARGET})' 'engine/testing -> SDL (link'
reject headless-link-sdl cmake apps/gta20-headless/CMakeLists.txt 'target_link_libraries(gta20-headless PRIVATE SDL3::SDL3)' 'apps/gta20-headless -> SDL (link SDL3::SDL3)'
reject root-links-scene-to-sdl-genex cmake "$TOP" 'target_link_libraries(gta20_scene PRIVATE $<BUILD_INTERFACE:SDL3::SDL3>)' 'engine/scene -> SDL (link SDL3::SDL3)'
reject platform-include-gpu-api src engine/platform '#include <vulkan/vulkan.h>' 'engine/platform -> GPU API (include vulkan/vulkan.h)'
reject sim-include-json src engine/sim '#include <nlohmann/json.hpp>' 'engine/sim -> nlohmann/json (include'
reject scene-link-json cmake "$S" 'target_link_libraries(gta20_scene PRIVATE gta20::json)' 'engine/scene -> nlohmann/json (link'
reject sim-include-os src engine/sim '#include <windows.h>' 'engine/sim -> OS (include windows.h)'
reject sim-link-os cmake engine/sim/CMakeLists.txt 'target_link_libraries(gta20_sim PRIVATE psapi)' 'engine/sim -> psapi (link)'

echo "== units without rules"
reject unknown-engine-module unit engine/audio '' 'engine/audio: no dependency rules'
reject unknown-app unit apps/editor '' 'apps/editor: no dependency rules'

if [[ "$(fingerprint)" == "$BEFORE" ]]; then
  ok "checkout unchanged (CMakeLists.txt, cmake/, engine/, apps/, tests/, checker)"
else
  fail "checkout unchanged" "a checked tree or the checker changed during the run" ""
fi

echo "$passed passed, $failed failed"
[[ $failed -eq 0 ]]
