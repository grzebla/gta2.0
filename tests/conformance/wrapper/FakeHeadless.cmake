# SYNTHETIC stand-in for gta20-headless in the conformance.wrapper.* tests (WrapperTest.cmake).
# Run as
#   cmake -P FakeHeadless.cmake run <scenario> --out <file>
#   cmake -P FakeHeadless.cmake compare <golden> <actual> --scenario <s> --markdown <m> --json <j>
# Every call is recorded in $FAKE_HEADLESS_LOG. The n-th `run` call behaves as the n-th element of
# the ;-separated $FAKE_HEADLESS_RUNS (missing elements mean ok):
#   ok     write the fixed SYNTHETIC trace and succeed
#   differ write a different SYNTHETIC trace and succeed
#   none   succeed without writing anything
#   fail   fail without writing anything
#   wrote-fail  write the fixed SYNTHETIC trace, then fail
# `compare` succeeds exactly when both files exist and are byte-identical. No GTA 2 data involved.
cmake_minimum_required(VERSION 3.25)

set(args "")
math(EXPR last "${CMAKE_ARGC} - 1")
foreach(i RANGE 3 ${last})
    list(APPEND args "${CMAKE_ARGV${i}}")
endforeach()
list(GET args 0 command)

set(runs 0)
if(EXISTS "$ENV{FAKE_HEADLESS_LOG}")
    file(STRINGS "$ENV{FAKE_HEADLESS_LOG}" calls REGEX "^run ")
    list(LENGTH calls runs)
endif()
list(JOIN args " " line)
file(APPEND "$ENV{FAKE_HEADLESS_LOG}" "${line}\n")

if(command STREQUAL "compare")
    list(GET args 1 golden)
    list(GET args 2 actual)
    if(NOT EXISTS "${golden}" OR NOT EXISTS "${actual}")
        message(FATAL_ERROR "compare: missing input")
    endif()
    file(SHA256 "${golden}" hg)
    file(SHA256 "${actual}" ha)
    if(NOT hg STREQUAL ha)
        execute_process(COMMAND ${CMAKE_COMMAND} -E echo "synthetic traces differ")
        message(FATAL_ERROR "compare: traces differ")
    endif()
    return()
endif()
if(NOT command STREQUAL "run")
    message(FATAL_ERROR "unknown command '${command}'")
endif()

list(FIND args "--out" out_index)
math(EXPR out_index "${out_index} + 1")
list(GET args ${out_index} out)
set(behaviour ok)
set(modes "$ENV{FAKE_HEADLESS_RUNS}")
list(LENGTH modes count)
if(runs LESS count)
    list(GET modes ${runs} behaviour)
endif()

if(behaviour STREQUAL "ok" OR behaviour STREQUAL "wrote-fail")
    file(WRITE "${out}" "{\"synthetic\":true,\"tick\":0}\n")
elseif(behaviour STREQUAL "differ")
    file(WRITE "${out}" "{\"synthetic\":true,\"tick\":1}\n")
endif()
if(behaviour STREQUAL "fail" OR behaviour STREQUAL "wrote-fail")
    message(FATAL_ERROR "synthetic run failure")
endif()
