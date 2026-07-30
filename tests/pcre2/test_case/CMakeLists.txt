cmake_minimum_required(VERSION 3.10)
project(pcre2 C)
set(CMAKE_C_STANDARD 99)

file(GLOB PCRE2_SOURCES "src/*.c")

add_library(pcre2 SHARED ${PCRE2_SOURCES})

target_include_directories(pcre2
    PUBLIC include
    PRIVATE src
)

target_compile_definitions(pcre2 PRIVATE
    HAVE_CONFIG_H
    PCRE2_CODE_UNIT_WIDTH=8
    SUPPORT_UNICODE
)

target_compile_options(pcre2 PRIVATE -w)

# Output the shared library to the build directory root (expected by the runner)
set_target_properties(pcre2 PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    PREFIX "lib"
)
