cmake_minimum_required(VERSION 3.10)
project(jansson C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Suppress dtoa.c warnings (it's old third-party code)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")

add_library(jansson SHARED
    src/dtoa.c
    src/dump.c
    src/error.c
    src/hashtable.c
    src/hashtable_seed.c
    src/load.c
    src/memory.c
    src/pack_unpack.c
    src/strbuffer.c
    src/strconv.c
    src/utf.c
    src/value.c
    src/version.c
)

target_include_directories(jansson
    PUBLIC include
    PRIVATE src
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)

# Define config from our pre-written header
target_compile_definitions(jansson PRIVATE HAVE_CONFIG_H)

# Output the shared library to the build directory root (expected by the runner)
set_target_properties(jansson PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    PREFIX "lib"
)
