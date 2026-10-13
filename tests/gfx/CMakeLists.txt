# Offline checks of original-game capture scenarios for the EC2 test host. Nothing here contacts the
# host or needs game data; captures themselves are requested by the orchestrator, never by CTest.
#   gfx.scenario.<id>             - tests/gfx/scenarios/<id>.json passes cmake/ValidateGfxScenario.cmake
#   gfx.scenario-check.<case>     - the validator accepts or rejects one SYNTHETIC scenario as expected
set(GTA20_GFX_VALIDATOR ${PROJECT_SOURCE_DIR}/cmake/ValidateGfxScenario.cmake)
set(GTA20_GFX_MATRIX ${CMAKE_CURRENT_SOURCE_DIR}/compat-matrix.json)

file(GLOB GTA20_GFX_SCENARIOS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/*.json)
foreach(scenario ${GTA20_GFX_SCENARIOS})
    get_filename_component(name ${scenario} NAME_WE)
    add_test(NAME gfx.scenario.${name}
        COMMAND ${CMAKE_COMMAND} -DSCENARIO=${scenario} -DMATRIX=${GTA20_GFX_MATRIX} -P ${GTA20_GFX_VALIDATOR})
endforeach()

foreach(case valid valid-no-inputs valid-window-mode unsupported-field missing-field id-mismatch id-not-string
             unknown-wrapper unknown-screen-key screen-value-string bad-start-mode duration-too-short
             duration-too-long duration-fraction screenshot-zero screenshot-at-end screenshots-duplicate
             screenshots-unsorted screenshots-empty screenshots-too-many screenshot-not-integer input-bad-key
             input-lowercase-key input-hold-zero input-hold-too-long input-extra-field input-missing-hold
             input-after-end input-held-past-end inputs-unsorted inputs-too-many screenshot-during-hold
             duplicate-key duplicate-nested-key trailing-text not-json top-level-array)
    add_test(NAME gfx.scenario-check.${case}
        COMMAND ${CMAKE_COMMAND} -DCASE=${case} -DVALIDATOR=${GTA20_GFX_VALIDATOR} -DMATRIX=${GTA20_GFX_MATRIX}
                -DWORK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/scenario-check-${case}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/scenario-check/ScenarioCheckTest.cmake)
endforeach()
