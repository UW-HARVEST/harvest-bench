cmake_minimum_required(VERSION 3.10)
project(mujs C)
set(CMAKE_C_STANDARD 99)

file(GLOB MUJS_SOURCES "src/*.c")

add_library(mujs SHARED ${MUJS_SOURCES})

target_include_directories(mujs
    PUBLIC include
    PRIVATE src
)

target_compile_options(mujs PRIVATE -w)

# Output the shared library to the build directory root (expected by gtest_suite)
set_target_properties(mujs PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    PREFIX "lib"
)
