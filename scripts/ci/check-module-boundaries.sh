#!/usr/bin/env bash
# Enforces the engine's module dependency graph (docs/architecture.md, "Moduły i zależności").
#
# Every engine module (engine/<name>) and app (apps/<name>) is a unit with an allow-list of gta20
# modules it may depend on. SDL, GPU API headers, nlohmann/json and OS headers/libraries have their
# own allow-lists. A module or app directory without rules is an error.
#
# Sources: every #include/#import in a unit's C/C++/Objective-C files whose path contains
# gta20/<module>/ is an edge of the unit that holds the file.
#
# CMake: EVERY project-owned CMakeLists.txt / *.cmake file is scanned (root, cmake/, engine/,
# apps/, tests/, ...), because any of them can modify any target. Target commands are attributed
# to the unit that owns the target in their first argument (gta20_<module>, gta20::<module>, or an
# app target = its directory name), not to the directory of the file:
#   - link:         target_link_libraries, add_dependencies;
#   - include path: target_include_directories (engine/<module>, ../<module>, module targets);
#   - sources:      target_sources, add_library, add_executable (another module's directory).
# Arguments are split into CMake list items (;) and generator-expression atoms ($<...:...>), so
# neither hides a dependency. In commands on a unit's target, an unknown gta20_*/gta20:: target, a
# variable other than ${GTA20_SDL_TARGET} in link arguments, and a variable other than the
# CMAKE_/PROJECT_ source and binary directories in path arguments are rejected. Rejected
# everywhere: directory-scoped link_libraries/include_directories in shared files (outside units
# and tests/), dependency properties set with set_property/set_target_properties on unit targets,
# and gta20 dependencies of a target that is only a variable (a CMake function argument).
#
# Usage: check-module-boundaries.sh [ROOT]
#   ROOT defaults to the repository root; tests/scripts/check-module-boundaries.test.sh passes
#   synthetic fixture trees.
set -euo pipefail

ROOT="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
ROOT="${ROOT%/}"
MODULES="core assets sim scene testing render platform"

# gta20 modules a unit may depend on, besides itself. Keep in sync with docs/architecture.md.
allowed_modules() {
  case "$1" in
    engine/core) echo "" ;;
    engine/assets) echo "core" ;;
    engine/sim) echo "core assets" ;;
    engine/scene) echo "core assets" ;;
    engine/testing) echo "core assets sim" ;;
    engine/render) echo "core scene" ;;
    engine/platform) echo "core" ;;
    apps/gta20-headless) echo "core assets sim scene testing" ;;
    apps/gta2) echo "core assets sim scene testing render platform" ;;
    *) return 1 ;;
  esac
}
may_use_sdl() { [[ $1 == engine/render || $1 == engine/platform || $1 == apps/gta2 ]]; }
may_use_gpu_api() { [[ $1 == engine/render ]]; }
may_use_json() { [[ $1 == engine/testing ]]; }
# The simulation must stay free of the OS: no OS headers and no non-gta20 link libraries.
is_pure() { [[ $1 == engine/sim ]]; }
# Variables allowed in path arguments of a unit's target commands.
PATH_VARS="CMAKE_CURRENT_SOURCE_DIR CMAKE_CURRENT_BINARY_DIR PROJECT_SOURCE_DIR PROJECT_BINARY_DIR CMAKE_SOURCE_DIR CMAKE_BINARY_DIR"

status=0
units=0
include_edges=0
link_edges=0
cmake_files=0

violation() { # file line message
  local rel="${1#"$ROOT"/}"
  echo "::error file=$rel,line=$2::$3"
  status=1
}

contains() { # word list...
  local word="$1" item
  shift
  for item in "$@"; do
    [[ $item == "$word" ]] && return 0
  done
  return 1
}

is_unit() { [[ $1 == engine/* || $1 == apps/* ]]; }

check_module_edge() { # unit module kind file line
  local unit="$1" module="$2" kind="$3" self allowed
  self="${unit#engine/}"
  [[ $unit == engine/* && $module == "$self" ]] && return 0
  # shellcheck disable=SC2086 # word lists
  if ! contains "$module" $MODULES; then
    violation "$4" "$5" "$unit -> $module ($kind): unknown gta20 module"
    return 0
  fi
  allowed="$(allowed_modules "$unit")"
  # shellcheck disable=SC2086
  if ! contains "$module" $allowed; then
    violation "$4" "$5" "$unit -> $module ($kind): forbidden module edge; $unit may depend on: ${allowed:-nothing}"
  fi
}

# --- Sources -----------------------------------------------------------------------------------

include_re='^[[:space:]]*#[[:space:]]*(include|import)[[:space:]]*[<"]([^>"]*)[>"]'
module_include_re='(^|/)gta20/([A-Za-z0-9_]+)/'

check_includes() { # unit dir
  local unit="$1" dir="$2" entry file rest line text path module
  while IFS= read -r entry; do
    file="${entry%%:*}"
    rest="${entry#*:}"
    line="${rest%%:*}"
    text="${rest#*:}"
    [[ $text =~ $include_re ]] || continue
    path="${BASH_REMATCH[2]}"
    include_edges=$((include_edges + 1))
    if [[ $path =~ $module_include_re ]]; then
      module="${BASH_REMATCH[2]}"
      check_module_edge "$unit" "$module" include "$file" "$line"
    fi
    case "$path" in
      SDL3/* | SDL2/* | SDL/* | SDL.h | SDL_*.h)
        may_use_sdl "$unit" ||
          violation "$file" "$line" "$unit -> SDL (include $path): only engine/render, engine/platform and apps/gta2 may use SDL" ;;
      vulkan/* | d3d1[12]*.h | dxgi*.h | Metal/* | MetalKit/* | QuartzCore/* | GL/* | GLES*/* | OpenGL/*)
        may_use_gpu_api "$unit" ||
          violation "$file" "$line" "$unit -> GPU API (include $path): only engine/render may use GPU API headers" ;;
      nlohmann/*)
        may_use_json "$unit" ||
          violation "$file" "$line" "$unit -> nlohmann/json (include $path): only engine/testing may use nlohmann/json" ;;
      windows.h | Windows.h | psapi.h | unistd.h | pthread.h | dlfcn.h | sys/* | mach/* | mach-o/*)
        if is_pure "$unit"; then
          violation "$file" "$line" "$unit -> OS (include $path): the simulation must not use OS headers"
        fi ;;
    esac
  done < <(grep -rnE \
    --include='*.h' --include='*.hh' --include='*.hpp' --include='*.hxx' --include='*.inl' --include='*.ipp' \
    --include='*.c' --include='*.cc' --include='*.cpp' --include='*.cxx' --include='*.m' --include='*.mm' \
    '^[[:space:]]*#[[:space:]]*(include|import)[[:space:]]*[<"]' "$dir" || true)
}

# --- CMake -------------------------------------------------------------------------------------

engine_dir_re='(^|[/}])engine/([A-Za-z0-9_-]+)(/|$)'
sibling_dir_re='(^|[/}])\.\./([A-Za-z0-9_-]+)(/|$)'
variable_re='\$\{([A-Za-z0-9_]*)\}'
genex_operator_re='^[A-Z][A-Z0-9_]*$'
dependency_property_re='[[:space:]](INTERFACE_)?(LINK_LIBRARIES|INCLUDE_DIRECTORIES|SOURCES|MANUALLY_ADDED_DEPENDENCIES|LINK_DEPENDS)[[:space:]]'

# Prints "<command><TAB><line><TAB><arguments>" for every dependency-relevant command (comments
# stripped, multi-line calls joined, command names lower-cased).
cmake_calls() {
  awk '
    FNR == 1 { cmd = "" }
    {
      text = $0
      sub(/#.*/, "", text)
      while (length(text) > 0) {
        if (cmd == "") {
          lower = tolower(text)
          if (!match(lower, /(^|[^a-z0-9_])(target_link_libraries|link_libraries|target_include_directories|include_directories|target_sources|add_library|add_executable|add_dependencies|set_property|set_target_properties)[ \t]*\(/)) break
          cmd = substr(lower, RSTART, RLENGTH)
          if (substr(cmd, 1, 1) !~ /[a-z]/) cmd = substr(cmd, 2)
          sub(/[ \t]*\($/, "", cmd)
          start = FNR
          args = ""
          text = substr(text, RSTART + RLENGTH)
        }
        p = index(text, ")")
        if (p == 0) { args = args " " text; break }
        args = args " " substr(text, 1, p - 1)
        print cmd "\t" start "\t" args
        cmd = ""
        text = substr(text, p + 1)
      }
    }' "$1"
}

file_scope() { # file -> engine/<m> | apps/<a> | tests | shared
  local rel="${1#"$ROOT"/}" sub
  case "$rel" in
    engine/*/* | apps/*/*)
      sub="${rel#*/}"
      echo "${rel%%/*}/${sub%%/*}"
      ;;
    tests/*) echo tests ;;
    *) echo shared ;;
  esac
}

is_path_var() {
  case " $PATH_VARS " in
    *" $1 "*) return 0 ;;
  esac
  return 1
}

unit_of_target() { # target -> prints the unit owning it, or nothing
  local t="$1" module=""
  case "$t" in
    gta20::*) module="${t#gta20::}" ;;
    gta20_*) module="${t#gta20_}" ;;
  esac
  # shellcheck disable=SC2086
  if [[ -n $module ]] && contains "$module" $MODULES; then
    echo "engine/$module"
  elif allowed_modules "apps/$t" >/dev/null; then
    echo "apps/$t"
  fi
}

# One dependency argument item (a CMake list element) of a command whose target belongs to
# `owner`: a unit, "" (a non-production target, e.g. a test executable) or "?" (target is a
# variable). kind: link | include path | sources.
check_item() { # owner kind item file line target
  local owner="$1" kind="$2" item="$3" file="$4" line="$5" target="$6"
  local rest var name atom label others=0
  local -a atoms
  label="$kind"
  [[ $kind != link ]] && label="$kind $item"

  # Paths into another module's directory: .../engine/<name>/... or ../<module>/...
  if [[ $kind != link ]]; then
    rest="$item"
    while [[ $rest =~ $engine_dir_re || $rest =~ $sibling_dir_re ]]; do
      name="${BASH_REMATCH[2]}"
      rest="${rest#*"${BASH_REMATCH[0]}"}"
      # shellcheck disable=SC2086
      if [[ ${BASH_REMATCH[0]} != *engine/* ]] && ! contains "$name" $MODULES; then
        continue # ../src, ../include, ... inside the unit itself
      fi
      include_edges=$((include_edges + 1))
      if [[ $owner == "?" ]]; then
        violation "$file" "$line" "$target -> $name ($label): cannot attribute a variable target to a module; name the module target"
      elif [[ -n $owner ]]; then
        check_module_edge "$owner" "$name" "$label" "$file" "$line"
      fi
    done
  fi

  # Variables: ${GTA20_SDL_TARGET} is SDL; anything else could hide an edge.
  rest="$item"
  while [[ $rest =~ $variable_re ]]; do
    var="${BASH_REMATCH[1]}"
    rest="${rest/"${BASH_REMATCH[0]}"/ }"
    if [[ $var == GTA20_SDL_TARGET ]]; then
      check_sdl_edge "$owner" "$label" "$file" "$line" "$target"
    elif is_unit "$owner" && { [[ $kind == link ]] || ! is_path_var "$var"; }; then
      violation "$file" "$line" "$owner -> \${$var} ($label): unsupported dependency expression; name targets and paths literally"
    fi
  done

  # Atoms of CMake generator expressions, e.g. $<BUILD_INTERFACE:gta20::sim> -> BUILD_INTERFACE, gta20::sim.
  rest="${rest//::/@@}"
  IFS='$<>,:' read -ra atoms <<<"$rest"
  for atom in ${atoms[@]+"${atoms[@]}"}; do
    atom="${atom//@@/::}"
    atom="${atom#"${atom%%[![:space:]]*}"}"
    atom="${atom%"${atom##*[![:space:]]}"}"
    case "$atom" in
      "" | PUBLIC | PRIVATE | INTERFACE | LINK_PUBLIC | LINK_PRIVATE | LINK_INTERFACE_LIBRARIES | debug | optimized | general) ;;
      gta20::json | gta20_nlohmann_json | nlohmann_json | nlohmann_json::*)
        [[ $kind == link ]] && link_edges=$((link_edges + 1))
        if [[ $owner == "?" ]]; then
          violation "$file" "$line" "$target -> nlohmann/json ($label): cannot attribute a variable target to a module; name the module target"
        elif is_unit "$owner" && ! may_use_json "$owner"; then
          violation "$file" "$line" "$owner -> nlohmann/json ($label $atom): only engine/testing may use nlohmann/json"
        fi ;;
      *SDL*)
        check_sdl_edge "$owner" "$label $atom" "$file" "$line" "$target" ;;
      gta20::* | gta20_*)
        name="${atom#gta20::}"
        name="${name#gta20_}"
        [[ $kind == link ]] && link_edges=$((link_edges + 1))
        # shellcheck disable=SC2086
        if ! contains "$name" $MODULES; then
          violation "$file" "$line" "${owner:-$target} -> $atom ($label): unknown gta20 target"
        elif [[ $owner == "?" ]]; then
          violation "$file" "$line" "$target -> $name ($label): cannot attribute a variable target to a module; name the module target"
        elif [[ -n $owner ]]; then
          check_module_edge "$owner" "$name" "$label" "$file" "$line"
        fi ;;
      *)
        if [[ $kind == link ]] && is_unit "$owner" && allowed_modules "apps/$atom" >/dev/null; then
          violation "$file" "$line" "$owner -> apps/$atom (link): an application is not a dependency"
        elif [[ $kind == link && ! ($item == *'$<'* && $atom =~ $genex_operator_re) ]]; then
          others=1
        fi ;;
    esac
  done
  if [[ $others -eq 1 ]] && is_pure "$owner"; then
    violation "$file" "$line" "$owner -> $item (link): the simulation may link only gta20 modules"
  fi
}

check_sdl_edge() { # owner label file line target
  [[ $2 == link* ]] && link_edges=$((link_edges + 1))
  if [[ $1 == "?" ]]; then
    violation "$3" "$4" "$5 -> SDL ($2): cannot attribute a variable target to a module; name the module target"
  elif is_unit "$1" && ! may_use_sdl "$1"; then
    violation "$3" "$4" "$1 -> SDL ($2): only engine/render, engine/platform and apps/gta2 may use SDL"
  fi
}

check_cmake_file() { # file
  local file="$1" scope cmd line args tok item kind target owner i first production
  local -a toks items
  scope="$(file_scope "$file")"
  while IFS=$'\t' read -r cmd line args; do
    read -ra toks <<<"${args//\"/}" # split on whitespace without globbing; quotes do not matter here
    [[ ${#toks[@]} -gt 0 ]] || continue
    target=""
    first=1
    case "$cmd" in
      target_link_libraries | add_dependencies) kind=link ;;
      target_include_directories) kind="include path" ;;
      target_sources | add_library | add_executable) kind=sources ;;
      link_libraries | include_directories)
        first=0
        kind="include path"
        if [[ $cmd == link_libraries ]]; then kind=link; fi
        if [[ $scope == shared ]]; then
          violation "$file" "$line" "directory-scoped $cmd in a shared CMake file affects every module; use the target_* command on the target"
          continue
        fi ;;
      set_property | set_target_properties)
        # Dependencies must be declared with target_* commands, where they are checked.
        if [[ " ${toks[*]} " =~ $dependency_property_re ]]; then
          production=0
          for tok in "${toks[@]}"; do
            if [[ -n $(unit_of_target "$tok") || $tok == *'$'* ]]; then production=1; fi
          done
          if [[ $production -eq 1 ]] || is_unit "$scope"; then
            violation "$file" "$line" "unsupported dependency command $cmd on a module target; use target_link_libraries/target_include_directories/target_sources"
          fi
        fi
        continue ;;
      *) continue ;;
    esac
    if [[ $first -eq 1 ]]; then
      target="${toks[0]}"
      if [[ $target == *'$'* ]]; then
        owner="?"
      else
        owner="$(unit_of_target "$target")"
        if [[ -z $owner ]] && is_unit "$scope"; then owner="$scope"; fi
      fi
    else
      owner=""
      if is_unit "$scope"; then owner="$scope"; fi
      target="$cmd"
    fi
    for ((i = first; i < ${#toks[@]}; i++)); do
      IFS=';' read -ra items <<<"${toks[i]}"
      for item in ${items[@]+"${items[@]}"}; do
        if [[ -n $item ]]; then check_item "$owner" "$kind" "$item" "$file" "$line" "$target"; fi
      done
    done
  done < <(cmake_calls "$file")
}

# --- Main --------------------------------------------------------------------------------------

if [[ ! -d $ROOT/engine ]]; then
  echo "::error::$ROOT has no engine/ directory"
  exit 1
fi
for dir in "$ROOT"/engine/*/ "$ROOT"/apps/*/; do
  [[ -d $dir ]] || continue
  dir="${dir%/}"
  unit="${dir#"$ROOT"/}"
  if ! allowed_modules "$unit" >/dev/null; then
    violation "$dir" 1 "$unit: no dependency rules; add it to scripts/ci/check-module-boundaries.sh and docs/architecture.md"
    continue
  fi
  units=$((units + 1))
  check_includes "$unit" "$dir"
done
while IFS= read -r file; do
  cmake_files=$((cmake_files + 1))
  check_cmake_file "$file"
done < <(find "$ROOT" \( -name .git -o -name .deps -o -name _deps -o -name CMakeFiles -o -name node_modules \
  -o -path "$ROOT/build" -o -path "$ROOT/build-*" -o -path "$ROOT/out" -o -path "$ROOT/.orchestrator" \
  -o -path "$ROOT/.claude" \) -prune -o -type f \( -name CMakeLists.txt -o -name '*.cmake' \) -print | LC_ALL=C sort)

if [[ $status -eq 0 ]]; then
  echo "module boundaries OK ($units units, $cmake_files CMake files, $include_edges include edges, $link_edges link edges checked)"
fi
exit $status
