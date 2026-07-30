cmake_minimum_required(VERSION 3.10)
project(lz4 C)

set(CMAKE_C_STANDARD 99)

add_library(lz4 SHARED
    src/lz4.c
    src/lz4hc.c
    src/lz4frame.c
    src/lz4file.c
    src/xxhash.c
)

target_include_directories(lz4
    PUBLIC include
    PRIVATE src
)

# Namespace xxhash to avoid symbol conflicts
target_compile_definitions(lz4 PRIVATE XXH_NAMESPACE=LZ4_)

# Match lz4's own build flags for correctness
target_compile_definitions(lz4 PRIVATE LZ4_HEAPMODE=0 LZ4F_HEAPMODE=0)

# Output the shared library to the build directory root (expected by the runner)
set_target_properties(lz4 PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    PREFIX "lib"
)
