cmake_minimum_required(VERSION 3.14)
project(lz4_gtest_suite C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

# GoogleTest, fetched at configure time and pinned to a release tag. Declared
# inline so this suite is self-contained wherever the directory is copied.
# Offline builds: pre-populate the source once and point CMake at it with
#   -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path/to/googletest-1.17.0
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)
# Only gtest is needed; skip GoogleMock and install rules.
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Library under test: absolute path to the shared library exporting the lz4
# public API (the C build or the translated Rust cdylib).
if(NOT DEFINED TEST_LIB_PATH)
  message(FATAL_ERROR "Pass -DTEST_LIB_PATH=/abs/path/to/liblz4.so (library under test)")
endif()

# Upstream lz4 test programs, included verbatim through glue TUs that rename
# main() and expose deterministic entry points (see ../README.md).
set(UPSTREAM_GLUE
    glue/glue_fuzzer.c
    glue/glue_frametest.c
    glue/glue_roundtrip.c
    glue/glue_decompress_partial.c
    glue/glue_decompress_partial_dict.c
)
# Match the flags the upstream tests are built with (tests/Makefile): the
# xxhash symbol namespace must agree with the library under test, and the
# upstream files are not warning-clean under this project's defaults.
set_source_files_properties(${UPSTREAM_GLUE} PROPERTIES
    COMPILE_DEFINITIONS "XXH_NAMESPACE=LZ4_"
    COMPILE_OPTIONS "-w"
)

# `harvest_gtest` is the target name the benchmark harness expects.
# C++ side of the glue: recording machinery and the scenario table. Kept out
# of the C translation units above, which include upstream sources verbatim.
set(GLUE_CXX
    glue/scenario_record.cc
    glue/fuzzer_scenarios.cc
)

add_executable(harvest_gtest
    upstream_tests.cc
    ${GLUE_CXX}
    ${UPSTREAM_GLUE}
)
target_include_directories(harvest_gtest PRIVATE
    glue
    upstream
    upstream/include
)
target_link_libraries(harvest_gtest PRIVATE GTest::gtest_main ${TEST_LIB_PATH} ${CMAKE_DL_LIBS})
