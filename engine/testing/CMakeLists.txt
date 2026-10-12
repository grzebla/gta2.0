# Headless conformance framework: scenarios, replay, traces, comparison, reports.
add_library(gta20_testing
    src/scenario.cpp
    src/trace.cpp
    src/compare.cpp
    src/runner.cpp)
add_library(gta20::testing ALIAS gta20_testing)
target_include_directories(gta20_testing PUBLIC include)
target_link_libraries(gta20_testing PUBLIC gta20::core gta20::assets gta20::sim PRIVATE gta20::json)
gta20_target_defaults(gta20_testing)
