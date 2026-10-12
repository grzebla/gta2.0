# The simulation must stay free of SDL, OS and GPU dependencies (checked in CI).
add_library(g2n_sim
    src/world.cpp)
add_library(g2n::sim ALIAS g2n_sim)
target_include_directories(g2n_sim PUBLIC include)
target_link_libraries(g2n_sim PUBLIC g2n::core g2n::assets)
g2n_target_defaults(g2n_sim)
