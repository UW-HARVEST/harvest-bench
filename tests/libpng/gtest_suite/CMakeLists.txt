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

# The glue TU includes upstream/pngvalid.c verbatim (main renamed) and is not
# warning clean under this project's defaults. It is an object library so the
# test binary and the scenario-list generator share one compilation of those
# ~12k lines.
add_library(pngvalid_glue OBJECT glue/glue_pngvalid.c)
target_compile_options(pngvalid_glue PRIVATE -w)
target_include_directories(pngvalid_glue PRIVATE upstream upstream/include glue)

add_executable(harvest_gtest
    upstream_tests.cc
    glue/pv_record.cc
    $<TARGET_OBJECTS:pngvalid_glue>
)

# Regenerates glue/scenario_list.h, the checked-in list of scenario names the
# suite registers. Not part of the default build's critical path, but built
# with it so it cannot rot:
#
#   cmake --build <build> --target harvest_scenario_list
#   ./<build>/harvest_scenario_list > gtest_suite/glue/scenario_list.h
#
# See glue/dump_scenarios.c for when this is needed.
add_executable(harvest_scenario_list
    glue/dump_scenarios.c
    $<TARGET_OBJECTS:pngvalid_glue>
)
target_include_directories(harvest_scenario_list PRIVATE glue)

# pngvalid.c (via PNG_FREESTANDING_TESTS) includes <png.h>, <zlib.h>, and
# <cexcept.h> from the include path. png.h + config are in upstream/include;
# cexcept.h is vendored next to pngvalid.c in upstream/.
target_include_directories(harvest_gtest PRIVATE
    upstream
    upstream/include
    glue
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

# The generator links the library under test because the shared glue object
# references its symbols; enumeration never calls any of them.
target_link_libraries(harvest_scenario_list PRIVATE ${TEST_LIB_PATH} ZLIB::ZLIB m)
