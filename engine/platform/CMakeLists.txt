add_library(g2n_platform
    src/platform.cpp)
add_library(g2n::platform ALIAS g2n_platform)
target_include_directories(g2n_platform PUBLIC include)
target_link_libraries(g2n_platform PUBLIC g2n::core ${G2N_SDL_TARGET})
g2n_target_defaults(g2n_platform)
