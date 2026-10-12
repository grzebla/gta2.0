# Renders a real GTA 2 city fragment offscreen and checks the image is a textured city, not
# a blank or single-colour frame. Skipped (CTest SKIP) without GTA20_GAME_DIR or a usable GPU device.
# The screenshot is derived from original data, so it goes to a fresh run directory under the
# external capture root - CAPTURE_ROOT, or the GTA20_RENDER_CAPTURE_DIR environment variable when
# set - which must lie outside CHECKOUT_DIR and BUILD_DIR (checked before the renderer starts).
# Inputs: GTA2_EXE (renderer command, may be a list), LEVEL, CAPTURE_ROOT, CHECKOUT_DIR, BUILD_DIR.
cmake_minimum_required(VERSION 3.25)
include(${CMAKE_CURRENT_LIST_DIR}/RenderCapture.cmake)

if((NOT DEFINED ENV{GTA20_GAME_DIR} OR "$ENV{GTA20_GAME_DIR}" STREQUAL "") AND DEFINED ENV{G2N_GAME_DIR})
    set(ENV{GTA20_GAME_DIR} "$ENV{G2N_GAME_DIR}") # pre-rename name
endif()
if(NOT DEFINED ENV{GTA20_GAME_DIR} OR "$ENV{GTA20_GAME_DIR}" STREQUAL "")
    message("SKIP: GTA20_GAME_DIR not set")
    return()
endif()
if(NOT "$ENV{GTA20_RENDER_CAPTURE_DIR}" STREQUAL "")
    set(CAPTURE_ROOT "$ENV{GTA20_RENDER_CAPTURE_DIR}")
endif()
get_filename_component(district "${LEVEL}" NAME_WE)
gta20_render_capture_prepare("${CAPTURE_ROOT}" "${CHECKOUT_DIR}" "${BUILD_DIR}" "${district}" run_dir)
set(OUT "${run_dir}/render-${district}.bmp")
message("Render capture (original data - local only, do not commit or upload): ${OUT}")
execute_process(COMMAND ${GTA2_EXE} --offscreen 640x360 --zoom 24 --level ${LEVEL} --screenshot ${OUT}
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
file(GLOB leftovers LIST_DIRECTORIES true "${run_dir}/*")
if(NOT leftovers)
    file(REMOVE_RECURSE "${run_dir}") # nothing was captured; drop the empty run directory
endif()
if(err MATCHES "SDL_CreateGPUDevice")
    message("SKIP: no GPU device: ${err}")
    return()
endif()
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "gta2 failed (${rc}): ${err}")
endif()
string(REGEX MATCH "unique_colours=([0-9]+) covered=([0-9.]+)" _ "${out}")
set(unique ${CMAKE_MATCH_1})
set(covered ${CMAKE_MATCH_2})
message("${out}")
if(unique LESS 500)
    message(FATAL_ERROR "only ${unique} distinct colours - textures missing?")
endif()
if(covered LESS 0.95)
    message(FATAL_ERROR "only ${covered} of the frame covered by the map")
endif()
message("Render capture saved: ${OUT}")
