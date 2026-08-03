cmake_minimum_required(VERSION 3.14)
project(libpng_gtest_suite C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

# GoogleTest, fetched at configure time and pinned. Offline:
#   -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path/to/googletest-1.17.0
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Library under test: absolute path to the shared library exporting the libpng
# public API (the C build or the translated Rust cdylib).
if(NOT DEFINED TEST_LIB_PATH)
  message(FATAL_ERROR "Pass -DTEST_LIB_PATH=/abs/path/to/libpng.so (library under test)")
endif()

# Glue TU includes upstream/pngvalid.c verbatim (main renamed). Not warning
# clean under this project's defaults.
set_source_files_properties(glue/glue_pngvalid.c PROPERTIES
    COMPILE_OPTIONS "-w"
)

add_executable(harvest_gtest
    upstream_tests.cc
    glue/glue_pngvalid.c
)

# pngvalid.c (via PNG_FREESTANDING_TESTS) includes <png.h>, <zlib.h>, and
# <cexcept.h> from the include path. png.h + config are in upstream/include;
# cexcept.h is vendored next to pngvalid.c in upstream/.
target_include_directories(harvest_gtest PRIVATE
    upstream
    upstream/include
)

# pngvalid calls zlib crc32() directly in the HARNESS (not via the library under
# test), so the test binary links the system zlib for its own use. This is
# independent of how the library under test implements compression internally
# (the C reference links zlib; a translated cdylib bundles its own).
find_package(ZLIB REQUIRED)

target_link_libraries(harvest_gtest PRIVATE
    GTest::gtest_main
    ${TEST_LIB_PATH}
    ZLIB::ZLIB
    ${CMAKE_DL_LIBS}
    m
)
