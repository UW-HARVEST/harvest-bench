cmake_minimum_required(VERSION 3.10)
project(libsodium C)

set(CMAKE_C_STANDARD 99)

# All library sources, verbatim from libsodium 1.0.23 (src/libsodium/).
# SIMD/asm variants compile from the same portable sources here: we define no
# HAVE_* feature macros, so every `#ifdef HAVE_*` selects the portable fallback
# (equivalent to configure --disable-asm). This is the reference semantics.
file(GLOB_RECURSE LIBSODIUM_SOURCES CONFIGURE_DEPENDS libsodium/*.c)

add_library(sodium SHARED ${LIBSODIUM_SOURCES})

# libsodium resolves internal headers relative to include/sodium (e.g.
# "private/common.h", "core.h") plus each source file's own directory.
target_include_directories(sodium
    PUBLIC  libsodium/include
    PRIVATE libsodium/include/sodium
)

# Every source #includes headers from its own subtree; add the primitive dirs.
file(GLOB LIBSODIUM_SUBDIRS LIST_DIRECTORIES true libsodium/*)
foreach(d ${LIBSODIUM_SUBDIRS})
    if(IS_DIRECTORY "${d}")
        target_include_directories(sodium PRIVATE "${d}")
    endif()
endforeach()

# libsodium builds with SODIUM_STATIC-free default visibility for the public
# API; the SODIUM_EXPORT macro handles export annotations. Nothing extra needed.

# Emit the shared library to the build-dir root as liblibsodium... no: the
# runner locates the single .so in the build root. Name it to match the target.
set_target_properties(sodium PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    PREFIX "lib"
)
