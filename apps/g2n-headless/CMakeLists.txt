add_executable(g2n-headless main.cpp)
target_link_libraries(g2n-headless PRIVATE g2n::testing g2n::core)
g2n_target_defaults(g2n-headless)
