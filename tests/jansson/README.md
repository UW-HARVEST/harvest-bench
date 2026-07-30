# Jansson JSON Library Test Case

## Source

- **Project**: [jansson](https://github.com/akheron/jansson) — C library for encoding, decoding, and manipulating JSON data
- **Version**: v2.15.0
- **License**: MIT
- **Author**: Petri Lehtinen

## What This Tests

The complete jansson library for C-to-Rust translation. `test_case/` contains
**only** verbatim upstream library sources. There is no dispatch wrapper and
no other adaptation code in the translation input. The translated Rust cdylib
must therefore export the **full public C ABI** of the
library. The test suite calls individual public symbols directly (parse,
serialize, value/object/array APIs, `json_pack`/`json_unpack`/`json_sprintf`,
copy/equal, custom allocators, dtoa-backed real formatting, ...).

## Source Files

All files are taken **verbatim** from the jansson v2.15.0 release with **no
modifications** (byte-identical to upstream `src/`).

| File | Lines | Role |
|------|-------|------|
| `dtoa.c` | 6,265 | Double-to-string conversion |
| `load.c` | 1,106 | JSON parser |
| `value.c` | 1,108 | JSON value creation, refcounting, comparison |
| `pack_unpack.c` | 938 | Format-string pack/unpack |
| `dump.c` | 496 | JSON serializer |
| `hashtable.c` + `hashtable.h` | 525 | Internal hash table (objects) |
| `hashtable_seed.c` | 277 | Hash seed initialization |
| `strconv.c` | 237 | String/number conversion |
| `utf.c` + `utf.h` | 187 | UTF-8 validation |
| `memory.c` | 103 | Custom allocator hooks |
| `strbuffer.c` + `strbuffer.h` | 130 | Dynamic string buffer |
| `error.c` | 59 | Error state helpers |
| `version.c` | 28 | Library version |
| `lookup3.h` | 382 | Hash function (internal header) |
| `jansson.h` + config headers | 425+ | Public API + generated config |
| **Total** | **~12,266** | |

The only non-upstream file is `test_case/CMakeLists.txt`, which builds the
sources into a single shared library (`libjansson.so`, `HAVE_CONFIG_H`, dtoa
enabled).

## Tests: `gtest_suite/`

Validation is a GoogleTest suite that the repository runner executes against
the shared library (C build or translated Rust cdylib). See
`gtest_suite/upstream/README.md` for provenance and porting details. The
suite replaces the retired single-call vector suite (81 single-call vectors
over a 4-operation dispatch wrapper) and covers far more of the library:

- `upstream_tests.cc` + `glue/` — the 20 upstream API unit-test programs
  (`test/suites/api/*.c`), included verbatim through glue TUs. These add
  coverage that the fixed-signature dispatch could never reach:
  `json_pack`/`unpack`/`sprintf` (variadic), mutation (object/array
  set/append/insert), `json_equal`, number edge cases, custom-allocator
  hooks, and deterministic allocation-failure injection (`test_chaos`).
- `data_tests.cc` + `upstream/{valid,invalid,invalid-unicode,encoding-flags}`
  — the 119 upstream data-suite cases. The upstream `json_process` helper
  drives them: it parses `input` with the optional `env` config and compares
  the result to the expected `output`/`error`. This adds the entire
  **error-path** surface (invalid and invalid-unicode inputs) that the
  state-equality vector format could not express.

All tests are deterministic (insertion-order object output, fixed inputs).
The suite honors the upstream `nostrip` and `skip_*_dtoa` markers. The full C
baseline is **135/135 green** (one `no_dtoa` variant self-skips on this
dtoa-enabled build) and runs in well under one second.

## Known limitation: variadic APIs are not implementable in stable Rust

Part of jansson's public C ABI is variadic: `json_pack`, `json_pack_ex`,
`json_unpack`, `json_unpack_ex`, `json_sprintf`, and their `va_list`
counterparts (`json_vpack_ex`, `json_vunpack_ex`, `json_vsprintf`). A
variadic `extern "C"` function (or a `va_list` parameter) requires the
`c_variadic` feature, which is **nightly-only**. A translation constrained
to a stable Rust toolchain cannot export these functions faithfully.
Workarounds like `RUSTC_BOOTSTRAP=1` violate the stable-toolchain contract.
Tests that exercise this surface (`UpstreamApi.Pack`, `UpstreamApi.Unpack`,
`UpstreamApi.Sprintf`, and parts of `UpstreamApi.Chaos`/`ChaosRealloc`) will
therefore fail against stable-Rust translations. Treat them as a measure of
this gap, not of translation quality.

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
      -DTEST_LIB_PATH=$(pwd)/c_build/libjansson.so
cmake --build gtest_build -j
# The runner starts one process per test. Do the same here: custom-allocator
# hooks in a few tests bleed if the whole binary runs in one process.
LD_LIBRARY_PATH=$(pwd)/c_build ./gtest_build/harvest_gtest --gtest_filter=UpstreamApi.Array

# Through the repository runner (manifest-aware):
harvest-bench run --suite gtest_suite --lib <path-to-your-libjansson.so>
```
