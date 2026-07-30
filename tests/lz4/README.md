# LZ4 Library Test Case

## Source

- **Project**: [lz4](https://github.com/lz4/lz4) — Extremely fast compression algorithm
- **Version**: v1.10.0 (commit `ebb370c`)
- **License**: BSD 2-Clause (library). The ported upstream tests are GPL-2.0-or-later.
- **Author**: Yann Collet

## What This Tests

The complete LZ4 library for C-to-Rust translation. `test_case/` contains
**only** verbatim upstream library sources. There is no dispatch wrapper and
no other adaptation code in the translation input. The translated Rust cdylib
must therefore export the **full public C ABI** of the library. The test
suite links against individual public symbols directly
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

Validation is a GoogleTest suite that the repository runner executes against
the shared library (C build or translated Rust cdylib).

The suite wraps verbatim copies of the upstream lz4 test programs through the
`glue_*.c` translation units. The upstream files are **unmodified**. Each
glue TU includes one of them with `main` renamed and exposes deterministic
`harvest_*` entry points. `upstream_tests.cc` wraps those entry points as
test cases. Coverage includes streaming, dictionaries, ring buffers, the
LZ4F frame API, and a 17-level round-trip sweep.

| File | Origin | Role |
|---|---|---|
| `fuzzer.c` | `tests/fuzzer.c` | Block/HC/stream API tests (unit section + fixed-seed fuzz loop) |
| `frametest.c` | `tests/frametest.c` | LZ4F frame API tests (unit section + fixed-seed fuzz loop) |
| `roundTripTest.c` | `tests/roundTripTest.c` | Compress/decompress round trip (pinned level + pinned buffer) |
| `decompress-partial.c` | `tests/decompress-partial.c` | Partial-decode checks |
| `decompress-partial-usingDict.c` | `tests/decompress-partial-usingDict.c` | Partial-decode with dictionary |
| `util.h`, `platform.h` | `programs/` | Self-contained helper headers the tests include |
| `include/*.h` | copies of `test_case/include/` | Public lz4 headers (the suite must build without `test_case/`) |

### Determinism

All randomness is pinned. Fuzz loops take an explicit seed (42) with the
wall-clock cutoff disabled. The round-trip test uses a fixed LCG buffer and
explicit compression levels. Upstream's default picks the level from a
content hash. Upstream gates the `fuzzer.c` unit-test section behind "no
seed given", which would randomize the subsequent loop. The glue calls the
section directly instead, which keeps both parts deterministic.

### Scenario-level reporting for the unit-test monoliths

Upstream packs dozens of independent scenarios into a single unit-test
function that aborts at the first failure. One GoogleTest case per such
function would collapse all of them into a single pass/fail bit. Each
scenario is therefore exposed as its own test case. The runner enumerates
them with `--gtest_list_tests`, which keeps one gtest case equal to one
scoring point and needs no protocol change.

The upstream function still executes only once per library under test:

- The glue TU intercepts the progress banners that upstream already prints,
  and records each scenario boundary. It defines `fprintf` as a macro before
  it includes the upstream file, so the upstream `DISPLAY` macro routes
  through the hook. The upstream sources are not modified.
- Language split: only the translation units that `#include` an upstream test
  source are C, because those sources are not valid C++ (implicit `void*`
  conversions, and whatever C-only constructs upstream may add later). Those
  TUs stay as thin as possible. The recording machinery
  (`scenario_record.cc`) and the scenario tables/banner parsing live in C++
  files behind a plain C interface. Nothing may throw across that boundary.
  An exception that unwinds through upstream C frames is undefined behavior,
  so every entry point is `noexcept`.
- The run happens in a **forked child** that appends to a record file and
  flushes after every scenario. A crashing or hanging library still leaves a
  usable partial record. The parent survives, so scenarios that already
  succeeded still report as passing.
- The runner computes the timeout of the current invocation from
  `manifest.json` and exports it as `HARVEST_TEST_SOFT_TIMEOUT_SECS`. The
  child uses that value as its deadline, so the partial record reaches disk
  before the runner kills the process. `manifest.json` is the single
  authority on time.
- The record is keyed to the library under test (path, mtime, size) and
  reused by later test processes. One recording therefore serves all
  scenario reads, and `manifest.json` groups each monolith and its scenario
  reads into one `suite`-mode process.

Verdicts: a scenario followed by another start (or by the run completing)
passed. The last scenario that started before an abort failed. Scenarios
that never started are reported as **failures, not skips**. Skipped tests
leave the denominator, which would let an early crash score better than a
late one.

### Deliberately not ported

- `tests/test-lz4-*.sh`, `test-lz4-*.py` — exercise the `lz4` CLI binary,
  not the library. Out of scope for a shared-library suite.
- `fullbench.c` — performance benchmark (policy excludes perf tests).
- `abiTest.c` — cross-version ABI check. Needs multiple installed versions.
- `freestanding.c` — freestanding-environment build test.
- `lz4_all.c` — amalgamated-build compile test.
- `datagen*`, `checkFrame.c`, `checkTag.c`, `loremOut.c` — support tools
  for the shell tests, not tests themselves.

### Test license

Upstream `tests/` and `programs/` files are **GPL-2.0-or-later** (see
`gtest_suite/upstream/COPYING`), unlike the lz4 library itself (BSD
2-Clause). The copied `include/` headers are from the library and stay BSD
2-Clause.

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
