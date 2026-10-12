# Every third-party dependency is pinned by URL + SHA-256 (reproducible, verifiable).
# Downloads are cached under FETCHCONTENT_BASE_DIR (CI caches that directory).
include(FetchContent)
set(FETCHCONTENT_QUIET ON)

if(GTA20_BUILD_TESTS)
    FetchContent_Declare(doctest
        URL https://github.com/doctest/doctest/archive/refs/tags/v2.5.3.tar.gz
        URL_HASH SHA256=174ebc4e769928959614789c5b4e9c3d0a0f81a62bb608756b127bfebfb21331
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(doctest)
endif()

# nlohmann/json: header-only, used by the testing module (scenarios, reports) only.
FetchContent_Declare(nlohmann_json_src
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/include.zip
    URL_HASH SHA256=b8cb0ef2dd7f57f18933997c9934bb1fa962594f701cd5a8d3c2c80541559372
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(nlohmann_json_src)
add_library(gta20_nlohmann_json INTERFACE)
target_include_directories(gta20_nlohmann_json SYSTEM INTERFACE ${nlohmann_json_src_SOURCE_DIR}/include)
add_library(gta20::json ALIAS gta20_nlohmann_json)

if(GTA20_BUILD_APP)
    if(GTA20_SDL_PROVIDER STREQUAL "system")
        find_package(SDL3 3.4 CONFIG REQUIRED)
    else()
        FetchContent_Declare(SDL3
            URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz
            URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        set(SDL_TESTS OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(SDL3)
    endif()
    if(TARGET SDL3::SDL3-static)
        set(GTA20_SDL_TARGET SDL3::SDL3-static CACHE INTERNAL "")
    else()
        set(GTA20_SDL_TARGET SDL3::SDL3 CACHE INTERNAL "")
    endif()
endif()
