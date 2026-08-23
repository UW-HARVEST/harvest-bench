cmake_minimum_required(VERSION 3.14)
project(mujs_gtest_suite CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GoogleTest, fetched at configure time and pinned to a release tag. Declared
# inline so this suite is self-contained wherever the directory is copied.
# Offline builds: pre-populate the source once and point CMake at it with
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
# Only gtest is needed; skip GoogleMock and install rules.
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Library under test: absolute path to the shared library exporting the
# mujs public API (the C build or the translated Rust cdylib).
if(NOT DEFINED TEST_LIB_PATH)
  message(FATAL_ERROR "Pass -DTEST_LIB_PATH=/abs/path/to/libmujs.so (library under test)")
endif()

# `harvest_gtest` is the target name the benchmark harness expects.
add_executable(harvest_gtest
    test262_vectors.cc
)
# mujs.h is vendored into the suite (copy of ../test_case/include/mujs.h) so
# the directory stays self-contained when copied into a translated output.
target_include_directories(harvest_gtest PRIVATE
    include
)
target_link_libraries(harvest_gtest PRIVATE GTest::gtest_main ${TEST_LIB_PATH})
