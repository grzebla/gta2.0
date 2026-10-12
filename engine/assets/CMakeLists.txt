add_library(g2n_assets
    src/chunk_file.cpp
    src/file_io.cpp
    src/gmp_map.cpp
    src/gmp_writer.cpp
    src/game_install.cpp
    src/sty_style.cpp)
add_library(g2n::assets ALIAS g2n_assets)
target_include_directories(g2n_assets PUBLIC include)
target_link_libraries(g2n_assets PUBLIC g2n::core)
g2n_target_defaults(g2n_assets)
