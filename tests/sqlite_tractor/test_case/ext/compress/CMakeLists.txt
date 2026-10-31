# © 2026 Massachusetts Institute of Technology
# MIT License

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.16)
  project(compress LANGUAGES C)
endif()

set(SQLITE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")

add_library(compress MODULE src/compress.c)
target_include_directories(compress
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${SQLITE_ROOT_DIR}/include" "${SQLITE_ROOT_DIR}/src"
)
target_compile_options(compress PRIVATE -USQLITE_CORE)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  find_package(ZLIB REQUIRED)
  target_link_libraries(compress PRIVATE ZLIB::ZLIB)
elseif(SQLITE_WITH_ZLIB)
  target_link_libraries(compress PRIVATE z)
endif()
