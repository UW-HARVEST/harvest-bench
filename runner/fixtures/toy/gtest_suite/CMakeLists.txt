cmake_minimum_required(VERSION 3.14)
project(toy_gtest_suite C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# The program has both dimensions; each artifact has its own variable.
if(NOT DEFINED TEST_LIB_PATH)
  message(FATAL_ERROR "Pass -DTEST_LIB_PATH=/abs/path/to/libtoy.so (library under test)")
endif()
if(NOT DEFINED TEST_BIN_PATH)
  message(FATAL_ERROR "Pass -DTEST_BIN_PATH=/abs/path/to/toy (executable under test)")
endif()
# The program name (argv[0]) comes from manifest.json's program.bin.name; the
# runner passes it. Never hard-code it here.
if(NOT DEFINED TEST_BIN_NAME)
  message(FATAL_ERROR "Pass -DTEST_BIN_NAME=<program.bin.name in manifest.json> with TEST_BIN_PATH")
endif()

add_executable(harvest_gtest lib_tests.cc bin_tests.cc)
# A real suite keeps its own copy of harvest_bin.h; this fixture uses the
# canonical one.
target_include_directories(harvest_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../support
    ${CMAKE_CURRENT_SOURCE_DIR}/../test_case/include)
target_compile_definitions(harvest_gtest PRIVATE
    HARVEST_BIN_PATH="${TEST_BIN_PATH}"
    HARVEST_BIN_NAME="${TEST_BIN_NAME}")
target_link_libraries(harvest_gtest PRIVATE GTest::gtest_main ${TEST_LIB_PATH})
