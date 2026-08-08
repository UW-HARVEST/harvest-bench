cmake_minimum_required(VERSION 3.14)
project(libsodium_gtest_suite C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

# GoogleTest, fetched at configure time and pinned. Declared inline so the
# suite is self-contained wherever the directory is copied.
# Offline: pre-populate and point CMake at it with
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

# Library under test: absolute path to the shared library exporting the
# libsodium public API (the C build or the translated Rust cdylib).
if(NOT DEFINED TEST_LIB_PATH)
  message(FATAL_ERROR "Pass -DTEST_LIB_PATH=/abs/path/to/libsodium.so (library under test)")
endif()

# One glue TU per upstream driver: includes the driver verbatim with its
# cmptest.h main() renamed to harvest_<name>().
file(GLOB UPSTREAM_GLUE CONFIGURE_DEPENDS glue/glue_*.c)

# Upstream drivers are not warning-clean under this project's defaults, and
# cmptest.h reads each driver's .exp from TEST_SRCDIR (default ".") and writes
# a <name>.res scratch file in CWD. Point TEST_SRCDIR at the upstream/ dir so
# the .exp files resolve regardless of the process working directory.
set_source_files_properties(${UPSTREAM_GLUE} PROPERTIES
    COMPILE_OPTIONS "-w"
    COMPILE_DEFINITIONS "TEST_SRCDIR=\"${CMAKE_CURRENT_SOURCE_DIR}/upstream\""
)

# `harvest_gtest` is the target name the benchmark harness expects.
add_executable(harvest_gtest
    upstream_tests.cc
    ${UPSTREAM_GLUE}
)

# cmptest.h writes each driver's captured stdout to a RELATIVE "<name>.res"
# path, so without this the 104 scratch files land in whatever directory the
# suite was started from. upstream_tests.cc moves the working directory here
# before any test runs; see the comment there.
set_source_files_properties(upstream_tests.cc PROPERTIES
    COMPILE_DEFINITIONS "HARVEST_SCRATCH_DIR=\"${CMAKE_CURRENT_BINARY_DIR}/res\""
)

# Glue TUs #include upstream drivers, which #include "cmptest.h"/"quirks.h"
# (in upstream/) and the libsodium public headers.
#
# `include/` holds the library's public headers, copied verbatim from
# test_case/libsodium/include/ and kept separate from upstream/ (which holds
# the test drivers). The copy makes the suite self-contained: the harness
# stages gtest_suite/ alone into a translated project's snapshot, so any path
# reaching outside this directory breaks there. `LIBSODIUM_INCLUDE_DIR`
# overrides the copy when a caller wants to test against different headers.
if(NOT DEFINED LIBSODIUM_INCLUDE_DIR)
  set(LIBSODIUM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
endif()

target_include_directories(harvest_gtest PRIVATE
    upstream
    "${LIBSODIUM_INCLUDE_DIR}"
)

target_link_libraries(harvest_gtest PRIVATE GTest::gtest_main ${TEST_LIB_PATH} ${CMAKE_DL_LIBS})
