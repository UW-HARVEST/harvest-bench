cmake_minimum_required(VERSION 3.14)
project(pcre2_gtest_suite C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

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
# pcre2 public API (the C build or the translated Rust cdylib).
if(NOT DEFINED TEST_LIB_PATH)
  message(FATAL_ERROR "Pass -DTEST_LIB_PATH=/abs/path/to/libpcre2.so (library under test)")
endif()

# pcre2test.c unconditionally includes pcre2posix.h and calls regfree() at
# exit, so the POSIX wrapper it expects has to be linkable even though none
# of the ported tests exercise it. Upstream itself builds pcre2posix.c as
# its own shared library (libpcre2-posix.so) rather than linking it
# directly into the pcre2test executable; pcre2posix.c and pcre2test.c each
# independently #include the library's internal pcre2_tables.c, so bundling
# both into one binary's object files would collide on the resulting
# non-static globals (_pcre2_OP_lengths and friends). Mirroring upstream's
# separate-shared-library layout avoids that.
add_library(pcre2_posix_glue SHARED upstream/pcre2posix.c)
target_include_directories(pcre2_posix_glue PRIVATE upstream upstream/include)
target_compile_definitions(pcre2_posix_glue PRIVATE HAVE_CONFIG_H PCRE2_CODE_UNIT_WIDTH=8)
target_compile_options(pcre2_posix_glue PRIVATE -w)
target_link_libraries(pcre2_posix_glue PRIVATE ${TEST_LIB_PATH})
set_target_properties(pcre2_posix_glue PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  OUTPUT_NAME "pcre2_posix_glue"
)

# `harvest_gtest` is the target name the benchmark harness expects.
add_executable(harvest_gtest
    upstream_tests.cc
    upstream/glue_pcre2test.c
)
target_include_directories(harvest_gtest PRIVATE
    upstream
    upstream/include
)
target_compile_definitions(harvest_gtest PRIVATE
    UPSTREAM_TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/upstream/testdata"
)
# Match the flags pcre2test.c is built with upstream (its own CMakeLists
# does not compile it -Wall/-Werror): the file is not warning-clean under
# this project's defaults, and it needs to see this build's config.h.
set_source_files_properties(upstream/glue_pcre2test.c PROPERTIES
    COMPILE_DEFINITIONS "HAVE_CONFIG_H;SUPPORT_PCRE2_8;HAVE_UNISTD_H"
    COMPILE_OPTIONS "-w"
)
target_link_libraries(harvest_gtest PRIVATE GTest::gtest_main ${TEST_LIB_PATH} pcre2_posix_glue)
