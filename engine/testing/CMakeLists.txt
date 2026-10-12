# Headless conformance framework: scenarios, replay, traces, comparison, reports.
add_library(g2n_testing
    src/scenario.cpp
    src/trace.cpp
    src/compare.cpp
    src/runner.cpp)
add_library(g2n::testing ALIAS g2n_testing)
target_include_directories(g2n_testing PUBLIC include)
target_link_libraries(g2n_testing PUBLIC g2n::core g2n::assets g2n::sim PRIVATE g2n::json)
g2n_target_defaults(g2n_testing)
