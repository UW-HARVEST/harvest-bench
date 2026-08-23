cmake_minimum_required(VERSION 3.14)
project(libsodium_gtest_suite C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

# GoogleTest, fetched at configure time and pinned. Declared inline so the
# suite is self-contained wherever the directory is copied.
# Offline: pre-populate and point CMake at it with
#   -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path/to/googletest-1.17.0
# `DOWNLOAD_EXTRACT_TIMESTAMP` silences a timestamp warning but exists only from CMake 3.24. On 3.22 the
# keyword is unknown to ExternalProject, so cmake_parse_arguments absorbs it into URL -- a MULTI-value
# keyword -- and the fetch dies with "At least one entry of URL is a path (invalid in a list)". That took
# out all seven suites on a 3.22 box: every project scored `no tests passed` from a healthy translation.
set(GTEST_FETCH_EXTRA "")
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
  set(GTEST_FETCH_EXTRA DOWNLOAD_EXTRACT_TIMESTAMP ON)
endif()
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
  ${GTEST_FETCH_EXTRA}
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

# Glue TUs #include upstream drivers, which #include "cmptest.h"/"quirks.h"
# (in upstream/) and the libsodium public headers.
target_include_directories(harvest_gtest PRIVATE
    upstream
    "${LIBSODIUM_INCLUDE_DIR}"
)
# The public headers live in the test_case source tree; allow an override but
# default to the sibling test_case include dir.
if(NOT DEFINED LIBSODIUM_INCLUDE_DIR)
  target_include_directories(harvest_gtest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../test_case/libsodium/include"
  )
endif()

target_link_libraries(harvest_gtest PRIVATE GTest::gtest_main ${TEST_LIB_PATH} ${CMAKE_DL_LIBS})
