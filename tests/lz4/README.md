# LZ4 Library Test Case

## Source

- **Project**: [lz4](https://github.com/lz4/lz4) — Extremely fast compression algorithm
- **Version**: v1.10.0
- **License**: BSD 2-Clause (library); the ported upstream tests are GPL-2.0
- **Author**: Yann Collet

## What This Tests

The complete LZ4 library for C-to-Rust translation. `test_case/` contains
**only** verbatim upstream library sources — there is no dispatch wrapper or
any other adaptation code mixed into the translation input. The translated
Rust cdylib is therefore expected to export the **full public C ABI** of the
library; the test suite links against individual public symbols directly
(`LZ4_compress_default`, `LZ4_compress_HC`, streaming/dictionary APIs,
`LZ4F_*` frame APIs, `LZ4_XXH32`/`LZ4_XXH64`, ...).

## Source Files

All files are taken **verbatim** from the lz4 v1.10.0 release with **no
modifications** (byte-identical to upstream `lib/`).

| File | Lines | Role |
|------|-------|------|
| `lz4.c` + `lz4.h` | 3,713 | Core block compression/decompression |
| `lz4hc.c` + `lz4hc.h` | 2,606 | High compression variant |
| `lz4frame.c` + `lz4frame.h` + `lz4frame_static.h` | 2,934 | Frame format |
| `lz4file.c` + `lz4file.h` | 434 | File I/O wrapper |
| `xxhash.c` + `xxhash.h` | 1,358 | xxHash (vendored, used for frame CRC) |
| **Total** | **~11,045** | |

The only non-upstream file is `test_case/CMakeLists.txt`, which builds the
sources into a single shared library (`liblz4.so`, `XXH_NAMESPACE=LZ4_`).

## Tests: `gtest_suite/`

Validation is a GoogleTest suite run by the repository runner against the
shared library (C build or translated Rust cdylib) — see
`gtest_suite/README.md` for provenance details.

- `upstream_tests.cc` + `upstream/` — the upstream lz4 test programs
  (`fuzzer.c`, `frametest.c`, `roundTripTest.c`, `decompress-partial*.c`)
  included verbatim through glue TUs, with all randomness pinned
  (fixed seeds, explicit compression levels, no wall-clock cutoffs).
  Covers streaming, dictionaries, ring buffers, the LZ4F frame API, and a
  17-level round-trip sweep. CLI shell tests, benchmarks (`fullbench`),
  ABI/freestanding/amalgamation checks are deliberately not ported.

## Build

```bash
cmake -S test_case -B build-ninja -G Ninja -DCMAKE_C_COMPILER=clang
cmake --build build-ninja
```

## Running Tests

```bash
# Standalone against the C build (from this directory):
cmake -S test_case -B c_build -DCMAKE_BUILD_TYPE=Release
cmake --build c_build -j
cmake -S gtest_suite -B gtest_build -DCMAKE_BUILD_TYPE=Release \
      -DTEST_LIB_PATH=$(pwd)/c_build/liblz4.so
cmake --build gtest_build -j
LD_LIBRARY_PATH=$(pwd)/c_build ./gtest_build/harvest_gtest

# Through the repository runner (manifest-aware):
harvest-bench run --suite gtest_suite --lib <path-to-your-liblz4.so>
```
