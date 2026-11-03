cmake_minimum_required(VERSION 3.14)
project(toy C)

# A program with both dimensions, shaped like sqlite3 + libsqlite3: a shared
# library, and an executable that links it. The second shared library's name
# sorts first, so the historical "first .so in the build root" rule would pick
# it; the manifest's c_target selects the right one.
add_library(toy SHARED src/toy.c)
target_include_directories(toy PUBLIC include)
set_target_properties(toy PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

add_library(aplugin SHARED src/plugin.c)
set_target_properties(aplugin PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# The executable's file name (driver) differs from its program name (toy) and
# it lives in a subdirectory: the runner finds it through the CMake file API.
add_executable(toyexe src/main.c)
target_link_libraries(toyexe PRIVATE toy)
set_target_properties(toyexe PROPERTIES
    OUTPUT_NAME driver
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
