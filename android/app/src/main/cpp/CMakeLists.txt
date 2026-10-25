# ROTOS JNI library: the shared navigation library (core/, compiled from
# source, no copy) + rotos_jni.c, the thin JNI bridge used by NativeNav.kt.
cmake_minimum_required(VERSION 3.22.1)
project(rotos_jni C)

include(${CMAKE_CURRENT_LIST_DIR}/../../../../../core/sources.cmake)

add_library(rotos_jni SHARED
    rotos_jni.c
    ${RN_CORE_SOURCES}
    ${RN_PHONE_SOURCES}
    ${RN_FFI_SOURCES})

target_include_directories(rotos_jni PRIVATE ${RN_INCLUDE_DIR} ${RN_FFI_INCLUDE_DIR})
set_property(TARGET rotos_jni PROPERTY C_STANDARD 11)
target_compile_options(rotos_jni PRIVATE -Wall -Wextra -O2)
# The bridge itself is held to the project's full warning set.
set_source_files_properties(rotos_jni.c PROPERTIES COMPILE_OPTIONS
    "-Wpedantic;-Wshadow;-Wconversion;-Wsign-conversion;-Wdouble-promotion;-Wstrict-prototypes;-Werror")
target_link_libraries(rotos_jni PRIVATE m log z)    # z: map compression (deflateRaw)
