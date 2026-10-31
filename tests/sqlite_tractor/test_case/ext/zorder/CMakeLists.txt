# © 2026 Massachusetts Institute of Technology
# MIT License

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.16)
  project(zorder LANGUAGES C)
endif()

set(SQLITE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")

add_library(zorder MODULE src/zorder.c)
target_include_directories(zorder
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${SQLITE_ROOT_DIR}/include" "${SQLITE_ROOT_DIR}/src"
)
target_compile_options(zorder PRIVATE -USQLITE_CORE)
