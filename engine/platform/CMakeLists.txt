add_library(gta20_platform
    src/platform.cpp)
add_library(gta20::platform ALIAS gta20_platform)
target_include_directories(gta20_platform PUBLIC include)
target_link_libraries(gta20_platform PUBLIC gta20::core ${GTA20_SDL_TARGET})
gta20_target_defaults(gta20_platform)
