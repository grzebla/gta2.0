# Bash checks of CI scripts. No game data, network or AWS; the EC2 session tests
# (ec2-gpu-session.test.sh) are run by the infra-lint workflow, not by CTest.
#   scripts.module-boundaries          - the checkout passes scripts/ci/check-module-boundaries.sh
#   scripts.module-boundaries-fixtures - SYNTHETIC fixture trees: every forbidden edge is rejected
# Skipped on Windows, where `bash` may resolve to WSL instead of a POSIX shell for this checkout.
if(WIN32)
    return()
endif()
find_program(GTA20_BASH bash)
if(NOT GTA20_BASH)
    message(STATUS "bash not found: module-boundary script tests are not registered")
    return()
endif()
add_test(NAME scripts.module-boundaries
    COMMAND ${GTA20_BASH} ${PROJECT_SOURCE_DIR}/scripts/ci/check-module-boundaries.sh)
add_test(NAME scripts.module-boundaries-fixtures
    COMMAND ${GTA20_BASH} ${CMAKE_CURRENT_SOURCE_DIR}/check-module-boundaries.test.sh)
