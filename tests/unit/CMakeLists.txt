add_executable(gta20_unit_tests
    main.cpp
    core_tests.cpp
    assets_tests.cpp
    sim_tests.cpp
    scene_tests.cpp
    testing_tests.cpp
    gamedata_tests.cpp
    inventory_tests.cpp)
target_link_libraries(gta20_unit_tests PRIVATE gta20::core gta20::assets gta20::sim gta20::scene gta20::testing synthetic_install_fixture doctest::doctest)
target_compile_definitions(gta20_unit_tests PRIVATE GTA20_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
gta20_target_defaults(gta20_unit_tests)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(gta20_unit_tests)

# Tests that need an original GTA 2 installation (GTA20_GAME_DIR) are tagged [gamedata]
# and skip themselves when it is not set.
