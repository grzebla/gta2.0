# Render-API-independent draw lists: testable without SDL or a GPU.
add_library(gta20_scene
    src/block_geometry.cpp
    src/draw_list.cpp
    src/face_texture.cpp)
add_library(gta20::scene ALIAS gta20_scene)
target_include_directories(gta20_scene PUBLIC include)
target_link_libraries(gta20_scene PUBLIC gta20::core gta20::assets)
gta20_target_defaults(gta20_scene)
