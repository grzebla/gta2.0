# The simulation must stay free of SDL, OS and GPU dependencies (checked in CI).
add_library(gta20_sim
    src/world.cpp)
add_library(gta20::sim ALIAS gta20_sim)
target_include_directories(gta20_sim PUBLIC include)
target_link_libraries(gta20_sim PUBLIC gta20::core gta20::assets)
gta20_target_defaults(gta20_sim)
