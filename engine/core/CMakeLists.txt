add_library(gta20_core
    src/angle.cpp
    src/camera2d.cpp
    src/diagnostics.cpp
    src/handle_pool.cpp)
add_library(gta20::core ALIAS gta20_core)
target_include_directories(gta20_core PUBLIC include)
gta20_target_defaults(gta20_core)
if(WIN32)
    target_link_libraries(gta20_core PRIVATE psapi)
endif()
