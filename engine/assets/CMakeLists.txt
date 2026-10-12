add_library(gta20_assets
    src/chunk_file.cpp
    src/file_io.cpp
    src/gmp_map.cpp
    src/gmp_writer.cpp
    src/game_install.cpp
    src/sty_style.cpp)
add_library(gta20::assets ALIAS gta20_assets)
target_include_directories(gta20_assets PUBLIC include)
target_link_libraries(gta20_assets PUBLIC gta20::core)
gta20_target_defaults(gta20_assets)
