# Script mode helper for tests/conformance (see that CMakeLists.txt).
# Inputs: HEADLESS (gta20-headless command, may be a list), SCENARIO, WORK (scratch directory of
# this test), MODE (determinism or regression) and, for regression, GOLDEN.
cmake_minimum_required(VERSION 3.25)

if(NOT MODE STREQUAL "determinism" AND NOT MODE STREQUAL "regression")
    message(FATAL_ERROR "unknown MODE '${MODE}' (expected determinism or regression)")
endif()
foreach(var HEADLESS SCENARIO WORK)
    if("${${var}}" STREQUAL "")
        message(FATAL_ERROR "${var} is not set")
    endif()
endforeach()

# Outputs of an earlier invocation must never stand in for this one.
file(MAKE_DIRECTORY "${WORK}")
file(REMOVE "${WORK}/a.jsonl" "${WORK}/b.jsonl" "${WORK}/report.md" "${WORK}/report.json")

# Runs the scenario into WORK/<trace>; fails unless the run succeeded and wrote the trace.
function(run_scenario trace label)
    execute_process(COMMAND ${HEADLESS} run "${SCENARIO}" --out "${WORK}/${trace}" RESULT_VARIABLE rc)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "${label} run failed (${rc})")
    endif()
    if(NOT EXISTS "${WORK}/${trace}")
        message(FATAL_ERROR "${label} run did not write ${WORK}/${trace}")
    endif()
endfunction()

run_scenario(a.jsonl first)
if(MODE STREQUAL "determinism")
    run_scenario(b.jsonl second)
    file(SHA256 "${WORK}/a.jsonl" ha)
    file(SHA256 "${WORK}/b.jsonl" hb)
    if(NOT ha STREQUAL hb)
        message(FATAL_ERROR "two runs produced different traces")
    endif()
else()
    if(NOT EXISTS "${GOLDEN}")
        message(FATAL_ERROR "missing golden trace ${GOLDEN} (generate with: gta20-headless run <scenario> --out <golden>)")
    endif()
    execute_process(COMMAND ${HEADLESS} compare "${GOLDEN}" "${WORK}/a.jsonl" --scenario "${SCENARIO}"
                            --markdown "${WORK}/report.md" --json "${WORK}/report.json"
                    RESULT_VARIABLE rc OUTPUT_VARIABLE out)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "trace differs from golden:\n${out}")
    endif()
endif()
