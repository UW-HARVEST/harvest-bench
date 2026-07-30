cmake_minimum_required(VERSION 3.14)
project(jansson_gtest_suite C CXX)

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

# Library under test: absolute path to the shared library exporting the
# jansson public API (the C build or the translated Rust cdylib).
if(NOT DEFINED TEST_LIB_PATH)
  message(FATAL_ERROR "Pass -DTEST_LIB_PATH=/abs/path/to/libjansson.so (library under test)")
endif()

# Upstream API unit-test programs (test/suites/api/*.c) and the data-suite
# helper (test/bin/json_process.c) are included verbatim through the glue TUs
# in glue/. Each api test file gets main() from upstream/api/util.h.
file(GLOB API_GLUE CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/glue/glue_test_*.c")
set(UPSTREAM_C_SOURCES
    ${API_GLUE}
    glue/glue_json_process.c
)
# Upstream test sources are not warning-clean under this project's defaults.
set_source_files_properties(${UPSTREAM_C_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-w"
)

add_executable(harvest_gtest
    data_tests.cc
    upstream_tests.cc
    ${UPSTREAM_C_SOURCES}
)
target_include_directories(harvest_gtest PRIVATE
    upstream
    upstream/api
    upstream/include
)
target_compile_definitions(harvest_gtest PRIVATE
    HAVE_CONFIG_H
    "JANSSON_GTEST_DATA_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/upstream\""
)
target_link_libraries(harvest_gtest PRIVATE GTest::gtest_main ${TEST_LIB_PATH})
