# Render-API-independent draw lists: testable without SDL or a GPU.
add_library(g2n_scene
    src/draw_list.cpp)
add_library(g2n::scene ALIAS g2n_scene)
target_include_directories(g2n_scene PUBLIC include)
target_link_libraries(g2n_scene PUBLIC g2n::core g2n::assets)
g2n_target_defaults(g2n_scene)
