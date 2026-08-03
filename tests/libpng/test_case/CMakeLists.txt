cmake_minimum_required(VERSION 3.10)
project(libpng C)

set(CMAKE_C_STANDARD 99)

# All libpng library sources, verbatim from libpng (libpng16 branch). The test
# drivers (pngtest.c) and the doc sample (example.c) are NOT part of the library
# and are excluded. pnglibconf.h is the prebuilt config header shipped upstream
# in scripts/ (normally produced at configure time).
file(GLOB LIBPNG_SOURCES CONFIGURE_DEPENDS src/*.c)

add_library(png SHARED ${LIBPNG_SOURCES})

target_include_directories(png PUBLIC include)

# libpng does NOT implement DEFLATE -- it calls zlib. The C reference build
# links the system zlib. A translated implementation of libpng is expected to
# provide the compression itself (e.g. a pure-Rust zlib such as flate2/
# miniz_oxide bundled into the cdylib), so it needs no external zlib. Only the
# reference C build resolves zlib here.
find_package(ZLIB REQUIRED)
target_link_libraries(png PRIVATE ZLIB::ZLIB)

# Emit the shared library to the build-dir root as libpng.so (the runner
# locates the single .so there).
set_target_properties(png PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    PREFIX "lib"
    OUTPUT_NAME "png"
)
