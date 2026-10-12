# SYNTHETIC stand-in for gta2 in the rendercheck.capture.* tests (CaptureDirTest.cmake). Run as
#   cmake -P FakeRenderer.cmake --offscreen WxH ... --screenshot <file>
# It records the call in $FAKE_RENDER_LOG and, like gta2, writes <file> only into an existing
# directory. The file is a text placeholder, not an image and not GTA 2 data.
# $FAKE_RENDER_MODE: ok (default, passes the image thresholds), blank (fails them),
# nogpu (reports a missing GPU device the way SDL does).
cmake_minimum_required(VERSION 3.25)

set(screenshot "")
math(EXPR last "${CMAKE_ARGC} - 1")
foreach(i RANGE 3 ${last})
    if(prev STREQUAL "--screenshot")
        set(screenshot "${CMAKE_ARGV${i}}")
    endif()
    set(prev "${CMAKE_ARGV${i}}")
endforeach()
file(APPEND "$ENV{FAKE_RENDER_LOG}" "screenshot=${screenshot}\n")

if("$ENV{FAKE_RENDER_MODE}" STREQUAL "nogpu")
    message(FATAL_ERROR "SDL_CreateGPUDevice failed: no supported device (synthetic)")
endif()
get_filename_component(dir "${screenshot}" DIRECTORY)
if(NOT IS_DIRECTORY "${dir}")
    message(FATAL_ERROR "cannot write screenshot: '${dir}' is not a directory")
endif()
file(WRITE "${screenshot}" "SYNTHETIC placeholder written by FakeRenderer.cmake - not a GTA 2 render\n")
if("$ENV{FAKE_RENDER_MODE}" STREQUAL "blank")
    execute_process(COMMAND ${CMAKE_COMMAND} -E echo "unique_colours=1 covered=1.000")
else()
    execute_process(COMMAND ${CMAKE_COMMAND} -E echo "unique_colours=4096 covered=1.000")
endif()
