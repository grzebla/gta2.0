add_library(g2n_core
    src/angle.cpp
    src/camera2d.cpp
    src/diagnostics.cpp
    src/handle_pool.cpp)
add_library(g2n::core ALIAS g2n_core)
target_include_directories(g2n_core PUBLIC include)
g2n_target_defaults(g2n_core)
if(WIN32)
    target_link_libraries(g2n_core PRIVATE psapi)
endif()
