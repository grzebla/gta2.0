#!/usr/bin/env bash
# Enforces the engine's module dependency graph (docs/architecture.md, "Moduły i zależności").
#
# Every engine module (engine/<name>) and app (apps/<name>) has an allow-list of gta20 modules it
# may depend on. An edge is any of:
#   - include:      #include "gta20/<module>/..." in a source or header file (any path prefix),
#   - link:         gta20::<module> / gta20_<module> in target_link_libraries / link_libraries,
#   - include path: engine/<module> or ../<module> in target_include_directories / include_directories.
# SDL, GPU API headers, nlohmann/json and OS headers/libraries have their own allow-lists.
# A module or app directory without rules is an error, so new modules must be added here.
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

status=0
units=0
include_edges=0
link_edges=0

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

engine_dir_re='(^|[/}])engine/([A-Za-z0-9_-]+)(/|$)'
sibling_dir_re='(^|[/}])\.\./([A-Za-z0-9_-]+)(/|$)'

# Prints "<link|include><TAB><line><TAB><arguments>" for every target_link_libraries,
# link_libraries, target_include_directories and include_directories call (comments stripped,
# multi-line calls joined).
cmake_calls() {
  awk '
    FNR == 1 { cmd = "" }
    {
      text = $0
      sub(/#.*/, "", text)
      while (length(text) > 0) {
        if (cmd == "") {
          lower = tolower(text)
          if (!match(lower, /(target_link_libraries|link_libraries|target_include_directories|include_directories)[ \t]*\(/)) break
          cmd = (substr(lower, RSTART, RLENGTH) ~ /link/) ? "link" : "include"
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

check_cmake() { # unit dir
  local unit="$1" dir="$2" file kind line args tok name module
  local -a toks
  while IFS= read -r file; do
    while IFS=$'\t' read -r kind line args; do
      read -ra toks <<<"$args" # split on whitespace without globbing
      for tok in ${toks[@]+"${toks[@]}"}; do
        tok="${tok//\"/}"
        if [[ $kind == include ]]; then
          # Another module's directory: .../engine/<module>/... or ../<module>/...
          name=""
          if [[ $tok =~ $engine_dir_re || $tok =~ $sibling_dir_re ]]; then
            name="${BASH_REMATCH[2]}"
          fi
          if [[ -n $name ]]; then
            include_edges=$((include_edges + 1))
            check_module_edge "$unit" "$name" "include path $tok" "$file" "$line"
          fi
          continue
        fi
        case "$tok" in
          PUBLIC | PRIVATE | INTERFACE | LINK_PUBLIC | LINK_PRIVATE | LINK_INTERFACE_LIBRARIES | debug | optimized | general) ;;
          gta20::json | gta20_nlohmann_json | nlohmann_json | nlohmann_json::*)
            link_edges=$((link_edges + 1))
            may_use_json "$unit" ||
              violation "$file" "$line" "$unit -> nlohmann/json (link $tok): only engine/testing may use nlohmann/json" ;;
          *SDL*)
            link_edges=$((link_edges + 1))
            may_use_sdl "$unit" ||
              violation "$file" "$line" "$unit -> SDL (link $tok): only engine/render, engine/platform and apps/gta2 may use SDL" ;;
          gta20::*)
            link_edges=$((link_edges + 1))
            check_module_edge "$unit" "${tok#gta20::}" link "$file" "$line" ;;
          gta20_*)
            module="${tok#gta20_}"
            # shellcheck disable=SC2086
            if contains "$module" $MODULES; then
              link_edges=$((link_edges + 1))
              # The first argument of target_link_libraries is the unit's own target.
              check_module_edge "$unit" "$module" link "$file" "$line"
            fi ;;
          *)
            # Other libraries (psapi, doctest, ...); the simulation links nothing but gta20 modules.
            if is_pure "$unit"; then
              violation "$file" "$line" "$unit -> $tok (link): the simulation may link only gta20 modules"
            fi ;;
        esac
      done
    done < <(cmake_calls "$file")
  done < <(find "$dir" -type f \( -name CMakeLists.txt -o -name '*.cmake' \) | sort)
}

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
  check_cmake "$unit" "$dir"
done

if [[ $status -eq 0 ]]; then
  echo "module boundaries OK ($units units, $include_edges includes, $link_edges link edges checked)"
fi
exit $status
