cmake_minimum_required(VERSION 3.25)

project(gta2_native
    VERSION 0.1.0
    DESCRIPTION "Native, cross-platform reimplementation of GTA 2 (C++20, SDL3, SDL_GPU)"
    LANGUAGES C CXX)

option(G2N_BUILD_APP "Build the SDL3/SDL_GPU application (needs SDL3)" ON)
option(G2N_BUILD_TESTS "Build unit tests (doctest)" ON)
option(G2N_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(G2N_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
set(G2N_SDL_PROVIDER "fetch" CACHE STRING "Where SDL3 comes from: fetch (pinned source) or system (find_package)")
set_property(CACHE G2N_SDL_PROVIDER PROPERTY STRINGS fetch system)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(CompilerOptions)
include(Dependencies)
include(EmbedBinary)

# Module graph (no cycles):
#   core <- assets <- sim <- testing
#   core <- scene(render-independent draw lists) <- render(SDL_GPU) <- app
#   core <- platform(SDL3) <- app
add_subdirectory(engine/core)
add_subdirectory(engine/assets)
add_subdirectory(engine/sim)
add_subdirectory(engine/scene)
add_subdirectory(engine/testing)
add_subdirectory(apps/g2n-headless)

if(G2N_BUILD_APP)
    add_subdirectory(engine/platform)
    add_subdirectory(engine/render)
    add_subdirectory(apps/gta2n)
endif()

if(G2N_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/unit)
    add_subdirectory(tests/conformance)
endif()
