# Zstandard Library Test Case

## Source

- **Project**: [zstd](https://github.com/facebook/zstd) — Zstandard, a fast lossless compression algorithm
- **Version**: v1.5.7
- **License**: dual BSD-3-Clause OR GPL-2.0 at the user's option. The copies here are used under the BSD license.
- **Author**: Yann Collet (Meta)

## What This Tests

The complete zstd library for C-to-Rust translation. `test_case/` contains
**only** verbatim upstream library sources. There is no dispatch wrapper and
no other adaptation code in the translation input. The translated Rust cdylib
must therefore export the **full public C ABI** of the library. The test
suite links against individual public symbols directly (single-shot and
streaming compression/decompression, contexts and advanced parameters,
dictionary APIs, `ZDICT_*` training, error helpers, ...).

The library build is single-threaded and pins the upstream build options:
`ZSTD_LEGACY_SUPPORT=5` (decode v0.5+ legacy frames), `XXH_NAMESPACE=ZSTD_`,
`DYNAMIC_BMI2=0`, and no `ZSTD_MULTITHREAD`. See "Single-threaded library
shim" below for how the suite absorbs the MT difference.

## Source Files

All files are taken **verbatim** from the zstd v1.5.7 release with **no
modifications** (byte-identical to upstream `lib/`). `src/` mirrors the
upstream `lib/` layout. `src/include/` holds a second copy of the three
public headers as the public include directory.

| Directory | Files | Lines | Role |
|---|---|---|---|
| `src/common/` | 25 | 12,475 | Shared internals (entropy coders, xxhash, error handling) |
| `src/compress/` | 29 | 23,366 | Compression |
| `src/decompress/` | 7 | 7,164 | Decompression |
| `src/dictBuilder/` | 6 | 5,323 | Dictionary training (`ZDICT_*`) |
| `src/deprecated/` | 4 | 484 | Deprecated zbuff API |
| `src/legacy/` | 15 | 26,295 | Legacy format decoders |
| `src/zstd.h`, `src/zdict.h`, `src/zstd_errors.h` | 3 | 3,786 | Public headers (also copied to `src/include/`) |
| **Total** | | **~78,900** | |

The only non-upstream file is `test_case/CMakeLists.txt`, which builds the
sources into a single shared library (`libzstd.so`).

## Tests: `gtest_suite/`

Validation is a GoogleTest suite that the repository runner executes against
the shared library (C build or translated Rust cdylib).

The suite wraps verbatim copies of the upstream zstd test programs (tag
v1.5.7) through the `glue/glue_*.c` translation units. The upstream files are
**unmodified**. Each glue TU includes one of them with `main` renamed and
exposes deterministic `harvest_*` entry points. `upstream_tests.cc` wraps
those entry points as test cases. `golden_tests.cc` drives the golden data
sets through the library API directly. Upstream only consumes those data sets
through CLI tests.

| File | Origin | Role |
|---|---|---|
| `fuzzer.c` | `tests/fuzzer.c` | Single-shot / block / dictionary / advanced-parameter tests (unit sections + fixed-seed fuzz loop) |
| `zstreamtest.c` | `tests/zstreamtest.c` | Streaming API tests, old and new API surfaces (unit section + fixed-seed fuzz loops) |
| `longmatch.c` | `tests/longmatch.c` | Long-distance match handling (streaming) |
| `bigdict.c`, `largeDictionary.c` | `tests/` | >=1 GiB dictionary windows (heavy: memory + wall clock) |
| `invalidDictionaries.c` | `tests/` | Malformed dictionaries must be rejected |
| `seqgen.c/.h`, `external_matchfinder.c/.h` | `tests/` | zstreamtest dependencies (sequence generator, external sequence producer) |
| `datagen.c/.h`, `util.c/.h`, `timefn.c/.h`, `platform.h` | `programs/` | Support code, compiled in per upstream's own `*-dll` recipes |
| `lib/**/*.h`, `lib/common/xxhash.c` | copies of `test_case/src` | Library headers (incl. internal ones the tests include) and the oracle-side xxhash implementation |
| `golden-*/`, `dict-files/` | `tests/` | Golden data sets, driven by `golden_tests.cc` |

### Determinism

The unit-test sections run with constant seed 0, exactly what upstream's own
`main()` does ("constant seed for predictability"). Fuzz loops take a pinned
seed. The wall-clock cutoff is disabled and cycle counts are fixed. The
golden tests are pure data-driven. No test depends on time or randomness.

### Single-threaded library shim

The library under test is built **without ZSTD_MULTITHREAD**. That is the
translation contract of this test case. Upstream always builds these tests
against an MT library and unconditionally sets the MT-only parameters
(`ZSTD_c_nbWorkers`, `ZSTD_c_jobSize`, `ZSTD_c_overlapLog`,
`ZSTD_c_rsyncable`). `glue_mt_shim.c` interposes the two parameter-setting
entry points at link level and clamps those four parameters to 0
(synchronous mode). Compressed-output semantics do not change. MT job
scheduling itself goes unexercised. The `#ifdef ZSTD_MULTITHREAD` test
sections (threadPoolTests etc.) compile out.

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

- `legacy.c` — its embedded test input is a single concatenated stream that
  starts with a v0.4.3 frame. The library build is `ZSTD_LEGACY_SUPPORT=5`
  (v0.5+ only), which rejects that frame with "Unknown frame descriptor",
  and the stream cannot be subset without changes to the upstream file.
  This becomes portable if the library contract ever changes to
  `ZSTD_LEGACY_SUPPORT=4`.
- `decodecorpus.c` — statically embeds the library sources
  (`#include "compress/zstd_compress.c"`). It would test its own embedded
  copy of upstream C rather than the shared library under test.
- `cli-tests/`, `playTests.sh` — exercise the `zstd` CLI binary, not the
  library.
- `fuzz/` + `regression/` — libFuzzer harnesses with a different execution
  model.
- `fullbench.c`, `paramgrill.c`, `*speed*.py` — performance benchmarks
  (policy excludes perf tests).
- `poolTests.c` — thread-pool only. The library under test is
  single-threaded.
- `checkTag.c`, `check_size.py`, `libzstd_builds.sh`, `versions` scripts —
  release and build infrastructure.
- `roundTripCrash.c` — libFuzzer-corpus companion tool (file-at-a-time crash
  reproducer).

### Test license

zstd, including `tests/` and `programs/`, is dual-licensed **BSD-3-Clause OR
GPL-2.0** at the user's option (see `gtest_suite/upstream/LICENSE`). These
copies are used under the BSD license.

## Build

```bash
cmake -S test_case -B build-ninja -G Ninja
cmake --build build-ninja
```

## Running Tests

```bash
# Standalone against the C build (from this directory):
cmake -S test_case -B c_build -DCMAKE_BUILD_TYPE=Release
cmake --build c_build -j
cmake -S gtest_suite -B gtest_build -DCMAKE_BUILD_TYPE=Release \
      -DTEST_LIB_PATH=$(pwd)/c_build/libzstd.so
cmake --build gtest_build -j
LD_LIBRARY_PATH=$(pwd)/c_build ./gtest_build/harvest_gtest

# Through the repository runner (manifest-aware):
harvest-bench run --suite gtest_suite --lib <path-to-your-libzstd.so>
```

A bare run (or a `--gtest_filter` that crosses monolith boundaries)
re-records each unit-test monolith once. Expect tens of seconds for the
recording tests. Filter one whole suite at a time for interactive debugging.
