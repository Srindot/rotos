# ROTOS core: the navigation library: portable C11, no dynamic memory, no OS calls.
#
#   rotos_nav       static library (firmware, simulator, Android JNI)
#   rotos_nav_ffi   shared library with the flat FFI (Python ctypes)
#   rotos_nav_tests unit tests (ctest)
cmake_minimum_required(VERSION 3.16)
project(rotos_core C)

include(${CMAKE_CURRENT_LIST_DIR}/sources.cmake)

set(RN_WARNINGS -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
                -Wdouble-promotion -Wstrict-prototypes -Wmissing-prototypes -Werror)

add_library(rotos_nav STATIC ${RN_CORE_SOURCES} ${RN_PHONE_SOURCES})
target_include_directories(rotos_nav PUBLIC ${RN_INCLUDE_DIR})
target_compile_features(rotos_nav PUBLIC c_std_11)
target_compile_options(rotos_nav PRIVATE ${RN_WARNINGS})
target_link_libraries(rotos_nav PUBLIC m)
set_target_properties(rotos_nav PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(rotos_nav_ffi SHARED ${RN_FFI_SOURCES})
target_link_libraries(rotos_nav_ffi PRIVATE rotos_nav)
target_include_directories(rotos_nav_ffi PRIVATE ${RN_FFI_INCLUDE_DIR})
target_compile_options(rotos_nav_ffi PRIVATE ${RN_WARNINGS})

if(PROJECT_IS_TOP_LEVEL OR ROTOS_BUILD_TESTS)
    enable_testing()
    file(GLOB RN_TEST_SOURCES ${RN_DIR}/tests/*.c)
    add_executable(rotos_nav_tests ${RN_TEST_SOURCES} ${RN_CORE_SOURCES} ${RN_PHONE_SOURCES})
    target_include_directories(rotos_nav_tests PRIVATE ${RN_INCLUDE_DIR} ${RN_DIR}/tests)
    target_compile_definitions(rotos_nav_tests PRIVATE RN_TESTING)
    target_compile_options(rotos_nav_tests PRIVATE -Wall -Wextra -Werror -fsanitize=address,undefined)
    target_link_options(rotos_nav_tests PRIVATE -fsanitize=address,undefined)
    target_link_libraries(rotos_nav_tests PRIVATE m z)    # zlib: reference compressor for test_inflate
    add_test(NAME rotos_nav_tests COMMAND rotos_nav_tests)
endif()
