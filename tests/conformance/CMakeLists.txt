# Every scenario in scenarios/ becomes two CTest tests:
#   conformance.determinism.<name>  - two runs produce byte-identical traces
#   conformance.regression.<name>   - the run matches golden/<name>.golden.jsonl
file(GLOB G2N_SCENARIOS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/*.json)
foreach(scenario ${G2N_SCENARIOS})
    get_filename_component(name ${scenario} NAME_WE)
    add_test(NAME conformance.determinism.${name}
        COMMAND ${CMAKE_COMMAND} -DHEADLESS=$<TARGET_FILE:g2n-headless> -DSCENARIO=${scenario}
                -DWORK=${CMAKE_CURRENT_BINARY_DIR}/${name} -DMODE=determinism
                -P ${PROJECT_SOURCE_DIR}/cmake/RunConformance.cmake)
    add_test(NAME conformance.regression.${name}
        COMMAND ${CMAKE_COMMAND} -DHEADLESS=$<TARGET_FILE:g2n-headless> -DSCENARIO=${scenario}
                -DWORK=${CMAKE_CURRENT_BINARY_DIR}/${name}-regression -DMODE=regression
                -DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/golden/${name}.golden.jsonl
                -P ${PROJECT_SOURCE_DIR}/cmake/RunConformance.cmake)
endforeach()

# Real-data render check (needs G2N_GAME_DIR and a GPU device; otherwise reported as skipped).
if(TARGET gta2n)
    foreach(level wil ste bil)
        add_test(NAME gamedata.render.${level}
            COMMAND ${CMAKE_COMMAND} -DGTA2N=$<TARGET_FILE:gta2n> -DLEVEL=data/${level}.gmp
                    -DOUT=${CMAKE_CURRENT_BINARY_DIR}/render-${level}.bmp
                    -P ${PROJECT_SOURCE_DIR}/cmake/RunRenderCheck.cmake)
        set_tests_properties(gamedata.render.${level} PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP:")
    endforeach()
endif()
