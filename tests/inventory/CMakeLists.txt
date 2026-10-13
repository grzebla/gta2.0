# Strict original-data gate (`gta20-headless inventory`, target `gamedata-strict`): asset-free
# negative cases. The positive SYNTHETIC fixture is covered by tests/unit/inventory_tests.cpp.
foreach(case missing-env env-not-a-directory missing-dir no-data-dir malformed-maps wrong-count)
    add_test(NAME inventory.strict.${case}
        COMMAND ${CMAKE_COMMAND} -DHEADLESS=$<TARGET_FILE:gta20-headless> -DCASE=${case}
                -DWORK=${CMAKE_CURRENT_BINARY_DIR}/${case} -P ${CMAKE_CURRENT_SOURCE_DIR}/InventoryCliTest.cmake)
endforeach()
