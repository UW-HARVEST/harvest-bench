cmake_minimum_required(VERSION 3.10)
project(zstd C)
set(CMAKE_C_STANDARD 99)

file(GLOB ZSTD_SOURCES
    "src/common/*.c"
    "src/compress/*.c"
    "src/decompress/*.c"
    "src/dictBuilder/*.c"
    "src/deprecated/*.c"
    "src/legacy/*.c"
)

add_library(zstd SHARED ${ZSTD_SOURCES})

target_include_directories(zstd
    PUBLIC src/include
    PRIVATE src src/common src/compress src/decompress src/dictBuilder src/deprecated src/legacy
)

target_compile_definitions(zstd PRIVATE
    ZSTD_LEGACY_SUPPORT=5
    XXH_NAMESPACE=ZSTD_
    DYNAMIC_BMI2=0
)

target_compile_options(zstd PRIVATE -w)
