add_executable(gta20-headless main.cpp)
target_link_libraries(gta20-headless PRIVATE gta20::testing gta20::assets gta20::core)
gta20_target_defaults(gta20-headless)

# Strict original-data gate: fails unless GTA20_GAME_DIR is a complete GTA 2 installation
# (docs/testing.md). Not part of CTest, so asset-free builds stay green without game data.
add_custom_target(gamedata-strict
    COMMAND gta20-headless inventory
    USES_TERMINAL
    COMMENT "Strict GTA 2 map inventory (needs GTA20_GAME_DIR)")
