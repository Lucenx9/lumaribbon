# Historical design study, retaining the pre-integration baseline. No install rule.
add_executable(luma-motion-prototype main.cpp)
target_link_libraries(luma-motion-prototype PRIVATE luma_audio Qt6::Quick)
target_compile_options(luma-motion-prototype PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(luma-motion-prototype PRIVATE LUMA_PROTOTYPE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
qt_add_shaders(luma-motion-prototype baseline_shader PREFIX "/prototype"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ribbon-legacy.frag)
qt_add_shaders(luma-motion-prototype candidate_shader PREFIX "/prototype"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ribbon-prototype.frag)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(test-prototype-accents test_accents.cpp)
    target_link_libraries(test-prototype-accents PRIVATE Qt6::Quick Qt6::Test)
    target_compile_definitions(test-prototype-accents PRIVATE LUMA_PROTOTYPE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    qt_add_shaders(test-prototype-accents accent_test_shader PREFIX "/prototype"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ribbon-prototype.frag)
    add_test(NAME prototype-accents COMMAND test-prototype-accents)
    set_tests_properties(prototype-accents PROPERTIES TIMEOUT 30)
endif()
