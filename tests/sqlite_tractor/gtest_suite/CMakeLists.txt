cmake_minimum_required(VERSION 3.16)
project(sqlite_tractor_suite LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
if(NOT DEFINED TEST_LIB_PATH)
  message(FATAL_ERROR "Pass -DTEST_LIB_PATH=/absolute/path/to/library.so")
endif()
find_path(TCL_INCLUDE_PATH tcl.h PATH_SUFFIXES tcl8.6 REQUIRED)
find_library(TCL_LIBRARY NAMES tcl8.6 REQUIRED)
file(STRINGS "${TCL_INCLUDE_PATH}/tcl.h" TCL_VERSION_LINE REGEX "^#define TCL_VERSION")
if(NOT TCL_VERSION_LINE MATCHES "8[.]6")
  message(FATAL_ERROR "This suite requires Tcl 8.6 headers")
endif()
include(FetchContent)
FetchContent_Declare(googletest
 URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
 DOWNLOAD_EXTRACT_TIMESTAMP ON)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
add_library(sqlite_candidate SHARED IMPORTED)
set_target_properties(sqlite_candidate PROPERTIES IMPORTED_LOCATION "${TEST_LIB_PATH}")
add_executable(harvest_gtest tests.cc glue/environment.c glue/compare.c glue/public_bridge.c glue/error_names.c glue/session.c glue/recover.c upstream/tclsqlite.c)
target_include_directories(harvest_gtest PRIVATE upstream ${TCL_INCLUDE_PATH})
# Engine feature macros deliberately do not enable private Tcl test modules.
target_compile_definitions(harvest_gtest PRIVATE NDEBUG SQLITE_ENABLE_PREUPDATE_HOOK SQLITE_ENABLE_SESSION
 SUITE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}" CANDIDATE_PATH="${TEST_LIB_PATH}")
target_link_libraries(harvest_gtest PRIVATE sqlite_candidate ${TCL_LIBRARY} GTest::gtest dl)

# Enable SESSION only in its support TU; suppress tclsqlite.c auto-registration.
set_source_files_properties(upstream/tclsqlite.c PROPERTIES COMPILE_OPTIONS "-USQLITE_ENABLE_SESSION")
