# Strict original-data gate (`gta20-headless inventory`, target `gamedata-strict`): asset-free
# CLI cases on SYNTHETIC installations. Library-level cases: tests/unit/inventory_tests.cpp.
add_library(synthetic_install_fixture STATIC synthetic_install.cpp)
target_include_directories(synthetic_install_fixture PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(synthetic_install_fixture PUBLIC gta20::assets)
gta20_target_defaults(synthetic_install_fixture)

add_executable(gta20_write_synthetic_install write_synthetic_install.cpp)
target_link_libraries(gta20_write_synthetic_install PRIVATE synthetic_install_fixture)
gta20_target_defaults(gta20_write_synthetic_install)

foreach(case missing-env legacy-env-ignored empty-env env-not-a-directory missing-dir no-data-dir malformed-maps
             wrong-count fixture-option fixture-env unknown-option unknown-option-with-value positional-argument
             missing-value empty-value duplicate-option)
    add_test(NAME inventory.strict.${case}
        COMMAND ${CMAKE_COMMAND} -DHEADLESS=$<TARGET_FILE:gta20-headless>
                -DFIXTURE_TOOL=$<TARGET_FILE:gta20_write_synthetic_install> -DCASE=${case}
                -DWORK=${CMAKE_CURRENT_BINARY_DIR}/${case} -P ${CMAKE_CURRENT_SOURCE_DIR}/InventoryCliTest.cmake)
endforeach()
