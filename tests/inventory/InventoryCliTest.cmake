# Asset-free checks of the strict gate `gta20-headless inventory`: every case must exit with 1
# and name the problem. Installations are SYNTHETIC directories under WORK (no original data).
#   cmake -DHEADLESS=<gta20-headless> -DCASE=<case> -DWORK=<dir> -P InventoryCliTest.cmake
foreach(var HEADLESS CASE WORK)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "${var} is required")
    endif()
endforeach()

set(maps wil lorne2e lorne2m lorne2h ste mike1e mike1m mike1h bil mike2e mike2m mike2h
         wil-multi bil-multi MP1-comp MP2-comp MP5-comp)

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(game_dir "${WORK}/game")
set(env_args --unset=GTA20_GAME_DIR --unset=G2N_GAME_DIR)
set(cli_args)

if(CASE STREQUAL "missing-env")
    set(expected "GTA20_GAME_DIR")
elseif(CASE STREQUAL "env-not-a-directory")
    set(env_args --unset=G2N_GAME_DIR "GTA20_GAME_DIR=${WORK}/does-not-exist")
    set(expected "game directory not found")
elseif(CASE STREQUAL "missing-dir")
    set(cli_args --game-dir "${WORK}/does-not-exist")
    set(expected "game directory not found")
elseif(CASE STREQUAL "no-data-dir")
    file(MAKE_DIRECTORY "${game_dir}")
    set(cli_args --game-dir "${game_dir}")
    set(expected "no data/ directory")
elseif(CASE STREQUAL "malformed-maps")
    # SYNTHETIC: 17 correctly named but empty map files.
    foreach(map ${maps})
        file(WRITE "${game_dir}/data/${map}.gmp" "")
    endforeach()
    set(cli_args --game-dir "${game_dir}")
    set(expected "data/wil.gmp: ")
elseif(CASE STREQUAL "wrong-count")
    list(REMOVE_ITEM maps MP5-comp)
    foreach(map ${maps})
        file(WRITE "${game_dir}/data/${map}.gmp" "")
    endforeach()
    set(cli_args --game-dir "${game_dir}")
    set(expected "found 16 maps")
else()
    message(FATAL_ERROR "unknown CASE ${CASE}")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E env ${env_args} "${HEADLESS}" inventory ${cli_args}
    RESULT_VARIABLE code
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
file(REMOVE_RECURSE "${WORK}")

if(NOT code STREQUAL "1")
    message(FATAL_ERROR "expected exit code 1, got '${code}'\nstdout:\n${out}\nstderr:\n${err}")
endif()
string(FIND "${out}" "${expected}" at)
if(at EQUAL -1)
    message(FATAL_ERROR "output does not mention '${expected}':\n${out}")
endif()
string(FIND "${out}" "result: FAIL" at)
if(at EQUAL -1)
    message(FATAL_ERROR "output lacks 'result: FAIL':\n${out}")
endif()
message(STATUS "${CASE}: exit 1 as expected")
