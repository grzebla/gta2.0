# Every scenario in scenarios/ becomes two CTest tests:
#   conformance.determinism.<name>  - two runs produce byte-identical traces
#   conformance.regression.<name>   - the run matches golden/<name>.golden.jsonl
file(GLOB GTA20_SCENARIOS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/scenarios/*.json)
foreach(scenario ${GTA20_SCENARIOS})
    get_filename_component(name ${scenario} NAME_WE)
    add_test(NAME conformance.determinism.${name}
        COMMAND ${CMAKE_COMMAND} -DHEADLESS=$<TARGET_FILE:gta20-headless> -DSCENARIO=${scenario}
                -DWORK=${CMAKE_CURRENT_BINARY_DIR}/${name} -DMODE=determinism
                -P ${PROJECT_SOURCE_DIR}/cmake/RunConformance.cmake)
    add_test(NAME conformance.regression.${name}
        COMMAND ${CMAKE_COMMAND} -DHEADLESS=$<TARGET_FILE:gta20-headless> -DSCENARIO=${scenario}
                -DWORK=${CMAKE_CURRENT_BINARY_DIR}/${name}-regression -DMODE=regression
                -DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/golden/${name}.golden.jsonl
                -P ${PROJECT_SOURCE_DIR}/cmake/RunConformance.cmake)
endforeach()

# Asset-free checks of RunConformance.cmake itself: a CMake-script stand-in replaces gta20-headless.
foreach(case deterministic first-run-fails second-run-fails missing-output differing-output unknown-mode)
    add_test(NAME conformance.wrapper.${case}
        COMMAND ${CMAKE_COMMAND} -DCASE=${case} -DWRAPPER=${PROJECT_SOURCE_DIR}/cmake/RunConformance.cmake
                -DFAKE_HEADLESS=${CMAKE_CURRENT_SOURCE_DIR}/wrapper/FakeHeadless.cmake
                -DWORK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/wrapper-${case}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/wrapper/WrapperTest.cmake)
endforeach()

# Real-data render check (needs GTA20_GAME_DIR and a GPU device; otherwise reported as skipped).
# Smoke test only: it does not establish M1 fidelity. Screenshots of original data go to a fresh
# run directory under GTA20_RENDER_CAPTURE_DIR, which must be outside the checkout and build tree.
include(RenderCapture)
gta20_render_capture_default(gta20_render_capture_default_dir)
set(GTA20_RENDER_CAPTURE_DIR "${gta20_render_capture_default_dir}"
    CACHE PATH "Absolute directory outside the checkout and build tree for original-data render captures")
if(TARGET gta2)
    foreach(level wil ste bil)
        add_test(NAME gamedata.render.${level}
            COMMAND ${CMAKE_COMMAND} -DGTA2_EXE=$<TARGET_FILE:gta2> -DLEVEL=data/${level}.gmp
                    "-DCAPTURE_ROOT=${GTA20_RENDER_CAPTURE_DIR}" "-DCHECKOUT_DIR=${PROJECT_SOURCE_DIR}"
                    "-DBUILD_DIR=${PROJECT_BINARY_DIR}" -P ${PROJECT_SOURCE_DIR}/cmake/RunRenderCheck.cmake)
        set_tests_properties(gamedata.render.${level} PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP:")
    endforeach()
endif()

# Asset-free checks of the capture-directory handling: a CMake-script stand-in replaces gta2 and
# everything happens in a temporary directory outside the checkout.
foreach(case accepted-external rejects-checkout rejects-build path-with-spaces repeated-runs symlinks
             not-configured env-override missing-data gpu-skip thresholds configured-default)
    add_test(NAME rendercheck.capture.${case}
        COMMAND ${CMAKE_COMMAND} -DCASE=${case} -DRENDER_CHECK=${PROJECT_SOURCE_DIR}/cmake/RunRenderCheck.cmake
                -DFAKE_RENDERER=${CMAKE_CURRENT_SOURCE_DIR}/rendercheck/FakeRenderer.cmake
                "-DCONFIGURED_ROOT=${GTA20_RENDER_CAPTURE_DIR}" "-DCHECKOUT_DIR=${PROJECT_SOURCE_DIR}"
                "-DBUILD_DIR=${PROJECT_BINARY_DIR}" -P ${CMAKE_CURRENT_SOURCE_DIR}/rendercheck/CaptureDirTest.cmake)
    set_tests_properties(rendercheck.capture.${case} PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP: symbolic links")
endforeach()
