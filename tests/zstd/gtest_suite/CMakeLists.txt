cmake_minimum_required(VERSION 3.14)
project(zstd_gtest_suite C CXX)

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

# Library under test: absolute path to the shared library exporting the zstd
# public API (the C build or the translated Rust cdylib).
if(NOT DEFINED TEST_LIB_PATH)
  message(FATAL_ERROR "Pass -DTEST_LIB_PATH=/abs/path/to/libzstd.so (library under test)")
endif()

# Upstream test programs are included verbatim through glue TUs (glue/, 
# see README.md). Support code compiled in per upstream's own *-dll recipes:
# datagen/util/timefn (programs/), seqgen + external_matchfinder (tests/),
# and xxhash.c as the oracle-side checksum implementation.
# NOTE: ZSTD_MULTITHREAD is deliberately NOT defined - the library under test
# is built single-threaded and the MT-only test sections compile out.
set(UPSTREAM_C_SOURCES
    glue/glue_fuzzer.c
    glue/glue_zstream.c
    glue/glue_small_tests.c
    glue/glue_invalid_dicts.c
    glue/glue_bigdict.c
    glue/glue_large_dictionary.c
    glue/glue_mt_shim.c
    upstream/seqgen.c
    upstream/external_matchfinder.c
    upstream/datagen.c
    upstream/util.c
    upstream/timefn.c
    upstream/lib/common/xxhash.c
)
set_source_files_properties(${UPSTREAM_C_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-w"
)

# C++ side of the glue: recording machinery and the scenario tables. Kept out
# of the C translation units above, which include upstream sources verbatim.
set(GLUE_CXX
    glue/scenario_record.cc
    glue/scenarios.cc
)

add_executable(harvest_gtest
    golden_tests.cc
    ${GLUE_CXX}
    upstream_tests.cc
    ${UPSTREAM_C_SOURCES}
)
target_include_directories(harvest_gtest PRIVATE
    glue
    upstream
    upstream/lib
    upstream/lib/include
    upstream/lib/common
    upstream/lib/compress
    upstream/lib/decompress
    upstream/lib/legacy
    upstream/lib/dictBuilder
    upstream/lib/deprecated
)
target_compile_definitions(harvest_gtest PRIVATE
    XXH_STATIC_LINKING_ONLY
    ZSTD_STATIC_LINKING_ONLY
    "ZSTD_GTEST_DATA_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/upstream\""
)
target_link_libraries(harvest_gtest PRIVATE GTest::gtest_main ${TEST_LIB_PATH} ${CMAKE_DL_LIBS})
