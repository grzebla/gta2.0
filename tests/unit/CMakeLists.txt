add_executable(g2n_unit_tests
    main.cpp
    core_tests.cpp
    assets_tests.cpp
    sim_tests.cpp
    scene_tests.cpp
    testing_tests.cpp
    gamedata_tests.cpp)
target_link_libraries(g2n_unit_tests PRIVATE g2n::core g2n::assets g2n::sim g2n::scene g2n::testing doctest::doctest)
target_compile_definitions(g2n_unit_tests PRIVATE G2N_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
g2n_target_defaults(g2n_unit_tests)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(g2n_unit_tests)

# Tests that need an original GTA 2 installation (G2N_GAME_DIR) are tagged [gamedata]
# and skip themselves when it is not set.
