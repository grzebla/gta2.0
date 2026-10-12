add_executable(gta20-headless main.cpp)
target_link_libraries(gta20-headless PRIVATE gta20::testing gta20::core)
gta20_target_defaults(gta20-headless)
